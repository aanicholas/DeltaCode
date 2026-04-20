/*
 * DeltaCode - Unreal Engine Code Helper
 * Copyright (c) 2026 Andrew Nicholas
 *
 * This program is free software: you can redistribute
 * it and/or modify it under the terms of the GNU
 * General Public License as published by the Free
 * Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */
#include "API/DCAnthropicClient.h"
#include "DeltaCodeEditorLog.h"
#include "Settings/DeltaCodeSettings.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace DCAnthropicClientPrivate
{
	constexpr const TCHAR* MessagesEndpoint = TEXT("https://api.anthropic.com/v1/messages");
	constexpr const TCHAR* AnthropicVersion = TEXT("2023-06-01");

	/** Dispatch a synthesised failure to OnComplete without touching the network. */
	static void FailFast(const FString& ErrorMessage, FDCOnAnthropicResponse OnComplete)
	{
		FDCAnthropicResponse Response;
		Response.bSuccess = false;
		Response.ErrorMessage = ErrorMessage;
		OnComplete.ExecuteIfBound(Response);
	}

	/** Serialize FDCAnthropicRequest to the JSON body Anthropic expects. */
	static FString BuildRequestBody(const FDCAnthropicRequest& Req,
	                                const FString& EffectiveModel,
	                                int32 EffectiveMaxTokens)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("model"), EffectiveModel);
		Root->SetNumberField(TEXT("max_tokens"), EffectiveMaxTokens);

		if (!Req.SystemPrompt.IsEmpty())
		{
			Root->SetStringField(TEXT("system"), Req.SystemPrompt);
		}

		// Zero temperature means "use provider default" for us — skip the field
		// entirely rather than pinning it to 0.0 (which would be deterministic).
		if (Req.Temperature > 0.0f)
		{
			Root->SetNumberField(TEXT("temperature"), Req.Temperature);
		}

		TArray<TSharedPtr<FJsonValue>> MessageValues;
		MessageValues.Reserve(Req.Messages.Num());
		for (const FDCAnthropicMessage& Msg : Req.Messages)
		{
			const TSharedRef<FJsonObject> MsgObj = MakeShared<FJsonObject>();
			MsgObj->SetStringField(TEXT("role"), Msg.Role);
			MsgObj->SetStringField(TEXT("content"), Msg.Content);
			MessageValues.Add(MakeShared<FJsonValueObject>(MsgObj));
		}
		Root->SetArrayField(TEXT("messages"), MessageValues);

		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root, Writer);
		return Out;
	}

	/** Map Anthropic success payload → FDCAnthropicResponse. Returns false if malformed. */
	static bool ParseSuccessBody(const FString& Body, FDCAnthropicResponse& OutResponse)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}

		// content: [{ type: "text", text: "..." }, ...]
		const TArray<TSharedPtr<FJsonValue>>* ContentArray = nullptr;
		if (Root->TryGetArrayField(TEXT("content"), ContentArray) && ContentArray)
		{
			for (const TSharedPtr<FJsonValue>& Entry : *ContentArray)
			{
				const TSharedPtr<FJsonObject>* BlockObj = nullptr;
				if (!Entry.IsValid() || !Entry->TryGetObject(BlockObj) || !BlockObj || !BlockObj->IsValid())
				{
					continue;
				}
				FString BlockType;
				(*BlockObj)->TryGetStringField(TEXT("type"), BlockType);
				if (BlockType == TEXT("text"))
				{
					FString BlockText;
					if ((*BlockObj)->TryGetStringField(TEXT("text"), BlockText))
					{
						OutResponse.Text.Append(BlockText);
					}
				}
				// Non-text blocks (tool_use, image, etc.) silently ignored at this layer.
			}
		}

		Root->TryGetStringField(TEXT("stop_reason"), OutResponse.StopReason);

		const TSharedPtr<FJsonObject>* UsageObj = nullptr;
		if (Root->TryGetObjectField(TEXT("usage"), UsageObj) && UsageObj && UsageObj->IsValid())
		{
			(*UsageObj)->TryGetNumberField(TEXT("input_tokens"), OutResponse.InputTokens);
			(*UsageObj)->TryGetNumberField(TEXT("output_tokens"), OutResponse.OutputTokens);
		}

		return true;
	}

	/** Extract Anthropic's structured error.message, or fall back to a generic message. */
	static FString ExtractErrorMessage(const FString& Body, int32 HttpStatus)
	{
		FString Detailed;
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			const TSharedPtr<FJsonObject>* ErrorObj = nullptr;
			if (Root->TryGetObjectField(TEXT("error"), ErrorObj) && ErrorObj && ErrorObj->IsValid())
			{
				(*ErrorObj)->TryGetStringField(TEXT("message"), Detailed);
			}
		}

		if (!Detailed.IsEmpty())
		{
			return FString::Printf(TEXT("HTTP %d — %s"), HttpStatus, *Detailed);
		}

		switch (HttpStatus)
		{
		case 401: return TEXT("HTTP 401 — key rejected. Re-enter or reissue at console.anthropic.com.");
		case 429: return TEXT("HTTP 429 — rate limited. Back off and retry.");
		case 500:
		case 502:
		case 503:
		case 529: return FString::Printf(TEXT("HTTP %d — Anthropic service error. Retry in a moment."), HttpStatus);
		default:  return FString::Printf(TEXT("HTTP %d — unexpected response from Anthropic."), HttpStatus);
		}
	}
}

FHttpRequestPtr FDCAnthropicClient::Send(const FDCAnthropicRequest& Request,
                                         FDCOnAnthropicResponse OnComplete)
{
	using namespace DCAnthropicClientPrivate;

	const UDeltaCodeSettings* Settings = UDeltaCodeSettings::Get();
	if (!Settings)
	{
		FailFast(TEXT("DeltaCode settings unavailable."), OnComplete);
		return nullptr;
	}

	if (!Settings->HasValidAPIKey())
	{
		FailFast(
			TEXT("No valid Anthropic API key. Set one in Project Settings → Plugins → DeltaCode."),
			OnComplete);
		return nullptr;
	}

	if (Request.Messages.Num() == 0)
	{
		FailFast(TEXT("Request has no messages — nothing to send."), OnComplete);
		return nullptr;
	}

	// Resolve effective values against settings defaults.
	const FString EffectiveModel      = Request.Model.IsEmpty()      ? Settings->ClaudeModel         : Request.Model;
	const int32   EffectiveMaxTokens  = Request.MaxTokens > 0        ? Request.MaxTokens             : Settings->MaxOutputTokens;
	const float   EffectiveTimeout    = Request.TimeoutSeconds > 0.0f ? Request.TimeoutSeconds       : Settings->RequestTimeoutSeconds;
	const bool    bVerbose            = Settings->bVerboseLogging;

	const FString Body = BuildRequestBody(Request, EffectiveModel, EffectiveMaxTokens);
	const double StartSeconds = FPlatformTime::Seconds();

	if (bVerbose)
	{
		UE_LOG(LogDeltaCodeEditor, Log,
			TEXT("[Client] POST %s  model=%s  messages=%d  system_bytes=%d  "
			     "body_bytes=%d  max_tokens=%d  temperature=%.2f  timeout=%.1fs"),
			MessagesEndpoint,
			*EffectiveModel,
			Request.Messages.Num(),
			Request.SystemPrompt.Len(),
			Body.Len(),
			EffectiveMaxTokens,
			Request.Temperature,
			EffectiveTimeout);
	}

	const TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(MessagesEndpoint);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("x-api-key"), Settings->AnthropicAPIKey);
	HttpRequest->SetHeader(TEXT("anthropic-version"), AnthropicVersion);
	HttpRequest->SetHeader(TEXT("content-type"), TEXT("application/json"));
	HttpRequest->SetTimeout(FMath::Clamp(EffectiveTimeout, 5.0f, 600.0f));
	HttpRequest->SetContentAsString(Body);

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[OnComplete, bVerbose, StartSeconds, EffectiveModel]
		(FHttpRequestPtr /*Req*/, FHttpResponsePtr HttpResponse, bool bConnectedOk)
		{
			FDCAnthropicResponse Response;

			if (!bConnectedOk || !HttpResponse.IsValid())
			{
				Response.bSuccess = false;
				Response.ErrorMessage =
					TEXT("Request failed — likely a network error, a timeout, or a user-cancelled call.");
			}
			else
			{
				Response.HttpStatus = HttpResponse->GetResponseCode();
				const FString Body = HttpResponse->GetContentAsString();

				if (Response.HttpStatus == 200)
				{
					if (ParseSuccessBody(Body, Response))
					{
						Response.bSuccess = true;
					}
					else
					{
						Response.bSuccess = false;
						Response.ErrorMessage =
							TEXT("Received 200 but could not parse Anthropic response body.");
					}
				}
				else
				{
					Response.bSuccess = false;
					Response.ErrorMessage = ExtractErrorMessage(Body, Response.HttpStatus);
				}
			}

			if (bVerbose)
			{
				const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
				const int32 ResponseBytes = HttpResponse.IsValid() ? HttpResponse->GetContentAsString().Len() : 0;
				UE_LOG(LogDeltaCodeEditor, Log,
					TEXT("[Client] done  model=%s  status=%d  bytes=%d  elapsed=%.2fs  "
					     "tokens_in=%d  tokens_out=%d  stop=%s  success=%s"),
					*EffectiveModel,
					Response.HttpStatus,
					ResponseBytes,
					Elapsed,
					Response.InputTokens,
					Response.OutputTokens,
					*Response.StopReason,
					Response.bSuccess ? TEXT("true") : TEXT("false"));
			}

			OnComplete.ExecuteIfBound(Response);
		});

	HttpRequest->ProcessRequest();
	return HttpRequest;
}
