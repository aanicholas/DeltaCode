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

/**
 * Result callback for an Anthropic validation ping.
 *
 * @param bSuccess  True if the API returned a 200 with a well-formed message.
 * @param Message   On success: a short human-readable "OK" string. On failure:
 *                  a user-facing error describing *why* (bad key, network, rate
 *                  limit, etc.). Never contains the raw API key.
 */
DECLARE_DELEGATE_TwoParams(FDCOnAnthropicPingComplete, bool /*bSuccess*/, const FString& /*Message*/);

/**
 * One-shot validator for Anthropic API keys.
 *
 * Fires a minimal /v1/messages request so the user can confirm their key is live
 * before committing to a real generation. Uses the cheapest model available so
 * pings cost effectively nothing.
 *
 * This is a deliberately narrow surface — the full message client lives in the
 * next layer (FDCAnthropicClient). We keep validation separate so the settings
 * screen can self-test without pulling in the heavier client dependencies.
 *
 * [INPUT]  From: UDeltaCodeSettings::ValidateAPIKey (CallInEditor button)
 * [OUTPUT] To:   FMessageDialog via FDCOnAnthropicPingComplete
 */
class DELTACODEEDITOR_API FDCAnthropicValidator
{
public:
	/**
	 * Fires a single validation request. Safe to call from the editor main thread.
	 * OnComplete is invoked on the game/main thread once the response arrives or
	 * the request errors out. Does not log the API key.
	 *
	 * @param APIKey            The user's Anthropic key (checked for prefix upstream).
	 * @param Model             Model slug for the ping (e.g. claude-haiku-4-5-20251001).
	 * @param TimeoutSeconds    HTTP timeout; clamped to a sane range internally.
	 * @param bVerboseLogging   If true, emits non-sensitive metadata via UE_LOG.
	 * @param OnComplete        Result callback.
	 */
	static void PingAsync(const FString& APIKey,
	                      const FString& Model,
	                      float TimeoutSeconds,
	                      bool bVerboseLogging,
	                      FDCOnAnthropicPingComplete OnComplete);
};
