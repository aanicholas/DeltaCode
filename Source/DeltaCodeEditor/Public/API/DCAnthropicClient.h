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
#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Interfaces/IHttpRequest.h"

// ─── Message (request side) ──────────────────────────────────────────────────

/**
 * A single turn in an Anthropic conversation. Plain-text content only; image
 * and tool-use blocks will be added when a gameplay feature needs them.
 */
struct DELTACODEEDITOR_API FDCAnthropicMessage
{
	/** "user" or "assistant". */
	FString Role;

	/** Plain-text content for this turn. */
	FString Content;

	FDCAnthropicMessage() = default;
	FDCAnthropicMessage(FString InRole, FString InContent)
		: Role(MoveTemp(InRole))
		, Content(MoveTemp(InContent))
	{}

	static FDCAnthropicMessage User(FString InContent)      { return FDCAnthropicMessage(TEXT("user"),      MoveTemp(InContent)); }
	static FDCAnthropicMessage Assistant(FString InContent) { return FDCAnthropicMessage(TEXT("assistant"), MoveTemp(InContent)); }
};

// ─── Request ─────────────────────────────────────────────────────────────────

/**
 * Parameters for a single /v1/messages call.
 *
 * Zero-valued fields fall back to UDeltaCodeSettings at Send time — so a caller
 * that only needs the default model + max tokens can just populate Messages.
 */
struct DELTACODEEDITOR_API FDCAnthropicRequest
{
	/** Model slug. Empty → UDeltaCodeSettings::ClaudeModel. */
	FString Model;

	/** Optional system prompt. */
	FString SystemPrompt;

	/** Conversation messages (at least one, ending on role="user"). */
	TArray<FDCAnthropicMessage> Messages;

	/** Cap on output tokens. 0 → UDeltaCodeSettings::MaxOutputTokens. */
	int32 MaxTokens = 0;

	/** Sampling temperature. 0 → Anthropic default (1.0). */
	float Temperature = 0.0f;

	/** HTTP timeout. 0 → UDeltaCodeSettings::RequestTimeoutSeconds. */
	float TimeoutSeconds = 0.0f;
};

// ─── Response ────────────────────────────────────────────────────────────────

/**
 * Result of a /v1/messages call. Always delivered to OnComplete on the game thread.
 *
 * Text is the concatenation of all top-level text content blocks — for simple
 * single-turn requests this is exactly the assistant's reply. Non-text blocks
 * (images, tool use) are ignored at this layer; extend the response shape when
 * a gameplay feature requires them.
 */
struct DELTACODEEDITOR_API FDCAnthropicResponse
{
	/** True if HTTP 200 and a parseable assistant message came back. */
	bool bSuccess = false;

	/** Raw HTTP status code. 0 if the connection never landed. */
	int32 HttpStatus = 0;

	/** Assistant text. Empty on failure. */
	FString Text;

	/** e.g. "end_turn", "max_tokens", "stop_sequence". */
	FString StopReason;

	/** Usage counters from Anthropic's `usage` block. */
	int32 InputTokens = 0;
	int32 OutputTokens = 0;

	/**
	 * User-facing error description populated on failure. Never contains the
	 * raw API key or the full response body.
	 */
	FString ErrorMessage;
};

/** Fired once per Send — success or failure. Always on the game thread. */
DECLARE_DELEGATE_OneParam(FDCOnAnthropicResponse, const FDCAnthropicResponse& /*Response*/);

// ─── Client ──────────────────────────────────────────────────────────────────

/**
 * Thin static wrapper around Anthropic's /v1/messages.
 *
 * The client deliberately has no instance state — each Send builds an HTTP
 * request from the passed-in parameters and the current settings, then hands
 * ownership to UE's HTTP module. Callers that need cancellation hold onto the
 * returned FHttpRequestPtr and call ->CancelRequest() on it. Callbacks are
 * robust to the underlying UObject (settings, widget) being torn down — no
 * captures of raw pointers into the callback lambda.
 *
 * [INPUT]  From: SDeltaCodeGeneratorPanel, Safe Mode advisor, Danger Zone pipeline
 * [OUTPUT] To:   Anthropic /v1/messages
 * [EVENT]  Fires: FDCOnAnthropicResponse on the game thread
 */
class DELTACODEEDITOR_API FDCAnthropicClient
{
public:
	/**
	 * Fire an async generation request.
	 *
	 * Returns the underlying HTTP handle so the caller can cancel mid-flight
	 * (Request->CancelRequest()). OnComplete is always invoked: success or
	 * failure, including the case where the key is missing or the request
	 * never left the process.
	 *
	 * Never logs the API key, the request body, or the response body. Verbose
	 * logging (gated on UDeltaCodeSettings::bVerboseLogging) emits metadata
	 * only: endpoint, model, byte counts, status, latency, token usage.
	 */
	static FHttpRequestPtr Send(const FDCAnthropicRequest& Request,
	                            FDCOnAnthropicResponse OnComplete);
};
