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
#include "Settings/DeltaCodeSettings.h"
#include "API/DCAnthropicValidator.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "DeltaCodeSettings"

UDeltaCodeSettings::UDeltaCodeSettings()
{
}

void UDeltaCodeSettings::ValidateAPIKey()
{
	// Structural pre-checks — avoid burning a network round-trip on obvious mistakes.
	if (AnthropicAPIKey.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("ValidateEmpty",
				"No API key entered.\n\nPaste your Anthropic key into the "
				"'Anthropic API Key' field first."));
		return;
	}
	if (!HasValidAPIKey())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("ValidateFormat",
				"The key doesn't look like an Anthropic key.\n\nKeys start with "
				"'sk-ant-'. Get one at console.anthropic.com."));
		return;
	}

	FDCAnthropicValidator::PingAsync(
		AnthropicAPIKey,
		ValidationModel,
		RequestTimeoutSeconds,
		bVerboseLogging,
		FDCOnAnthropicPingComplete::CreateLambda(
			[](bool bSuccess, const FString& Message)
			{
				if (bSuccess)
				{
					FMessageDialog::Open(EAppMsgType::Ok,
						FText::Format(
							LOCTEXT("ValidateSuccess", "\u2705 {0}"),
							FText::FromString(Message)));
				}
				else
				{
					FMessageDialog::Open(EAppMsgType::Ok,
						FText::Format(
							LOCTEXT("ValidateFailure", "Validation failed.\n\n{0}"),
							FText::FromString(Message)));
				}
			}));
}

#undef LOCTEXT_NAMESPACE
