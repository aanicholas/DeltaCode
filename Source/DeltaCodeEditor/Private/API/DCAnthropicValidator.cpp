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
#include "API/DCAnthropicValidator.h"
#include "DeltaCodeEditorLog.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace DCAnthropicValidatorPrivate
{
	constexpr const TCHAR* MessagesEndpoint = TEXT("https://api.anthropic.com/v1/messages");
	constexpr const TCHAR* AnthropicVersion = TEXT("2023-06-01");

	/** Builds the minimal JSON body for a validation ping. */
	static FString BuildPingBody(const FString& Model)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("model"), Model);
		Root->SetNumberField(TEXT("max_tokens"), 8);

		const TSharedRef<FJsonObject> UserMsg = MakeShared<FJsonObject>();
		UserMsg->SetStringField(TEXT("role"), TEXT("user"));
		UserMsg->SetStringField(TEXT("content"), TEXT("ping"));

		TArray<TSharedPtr<FJsonValue>> Messages;
		Messages.Add(MakeShared<FJsonValueObject>(UserMsg));
		Root->SetArrayField(TEXT("messages"), Messages);

		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root, Writer);
		return Out;
	}

	/**
	 * Maps a response (or connection failure) to a user-facing (bSuccess, Message) pair.
	 * Never returns the API key. On HTTP error, extracts the Anthropic "error.message"
	 * field when present so users see the real cause (bad key, overloaded, etc.).
	 */
	static void InterpretResponse(bool bConnectedOk,
	                              FHttpResponsePtr Response,
	                              bool& OutSuccess,
	                              FString& OutMessage)
	{
		if (!bConnectedOk || !Response.IsValid())
		{
			OutSuccess = false;
			OutMessage = TEXT("Network error — could not reach api.anthropic.com. "
			                  "Check your internet connection or firewall.");
			return;
		}

		const int32 Code = Response->GetResponseCode();
		const FString Body = Response->GetContentAsString();

		if (Code == 200)
		{
			// Successful ping — don't bother parsing content, just confirm shape.
			OutSuccess = true;
			OutMessage = TEXT("API key validated. Anthropic responded successfully.");
			return;
		}

		// Try to pull the Anthropic-formatted error message out of the body.
		FString DetailedError;
		TSharedPtr<FJsonObject> JsonBody;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (FJsonSerializer::Deserialize(Reader, JsonBody) && JsonBody.IsValid())
		{
			const TSharedPtr<FJsonObject>* ErrorObj = nullptr;
			if (JsonBody->TryGetObjectField(TEXT("error"), ErrorObj) && ErrorObj && ErrorObj->IsValid())
			{
				(*ErrorObj)->TryGetStringField(TEXT("message"), DetailedError);
			}
		}

		OutSuccess = false;
		if (Code == 401)
		{
			OutMessage = FString::Printf(TEXT("Authentication failed (401). %s"),
				DetailedError.IsEmpty()
					? TEXT("The key was rejected by Anthropic — check for typos or revoke/reissue at console.anthropic.com.")
					: *DetailedError);
		}
		else if (Code == 429)
		{
			OutMessage = FString::Printf(TEXT("Rate limited (429). %s"),
				DetailedError.IsEmpty()
					? TEXT("Try again in a moment.")
					: *DetailedError);
		}
		else
		{
			OutMessage = FString::Printf(TEXT("Anthropic returned HTTP %d. %s"),
				Code,
				DetailedError.IsEmpty() ? TEXT("See https://docs.anthropic.com for status codes.")
				                        : *DetailedError);
		}
	}
}

void FDCAnthropicValidator::PingAsync(const FString& APIKey,
                                      const FString& Model,
                                      float TimeoutSeconds,
                                      bool bVerboseLogging,
                                      FDCOnAnthropicPingComplete OnComplete)
{
	using namespace DCAnthropicValidatorPrivate;

	if (APIKey.IsEmpty())
	{
		OnComplete.ExecuteIfBound(false, TEXT("API key is empty."));
		return;
	}
	if (Model.IsEmpty())
	{
		OnComplete.ExecuteIfBound(false, TEXT("Validation model is empty — set one in Project Settings."));
		return;
	}

	const FString Body = BuildPingBody(Model);
	const double StartSeconds = FPlatformTime::Seconds();

	// NEVER log APIKey, request Body, or response Body. Only safe metadata.
	if (bVerboseLogging)
	{
		UE_LOG(LogDeltaCodeEditor, Log,
			TEXT("[Validator] POST %s  model=%s  body_bytes=%d  timeout=%.1fs"),
			MessagesEndpoint, *Model, Body.Len(), TimeoutSeconds);
	}

	const TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(MessagesEndpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("x-api-key"), APIKey);
	Request->SetHeader(TEXT("anthropic-version"), AnthropicVersion);
	Request->SetHeader(TEXT("content-type"), TEXT("application/json"));
	Request->SetTimeout(FMath::Clamp(TimeoutSeconds, 5.0f, 600.0f));
	Request->SetContentAsString(Body);

	Request->OnProcessRequestComplete().BindLambda(
		[OnComplete, bVerboseLogging, StartSeconds]
		(FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bConnectedOk)
		{
			bool bSuccess = false;
			FString Message;
			InterpretResponse(bConnectedOk, Response, bSuccess, Message);

			if (bVerboseLogging)
			{
				const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
				const int32 Code = Response.IsValid() ? Response->GetResponseCode() : 0;
				const int32 ResponseBytes = Response.IsValid() ? Response->GetContentAsString().Len() : 0;
				UE_LOG(LogDeltaCodeEditor, Log,
					TEXT("[Validator] done  status=%d  bytes=%d  elapsed=%.2fs  success=%s"),
					Code, ResponseBytes, Elapsed, bSuccess ? TEXT("true") : TEXT("false"));
			}

			OnComplete.ExecuteIfBound(bSuccess, Message);
		});

	Request->ProcessRequest();
}
