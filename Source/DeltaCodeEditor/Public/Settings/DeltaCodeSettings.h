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
#include "Engine/DeveloperSettings.h"
#include "Types/DCCoreTypes.h"
#include "DeltaCodeSettings.generated.h"

// ─── Generation mode ─────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EDCGenerationMode : uint8
{
	// Additive only — never deletes, replaces, or modifies existing level actors.
	Safe    UMETA(DisplayName = "Safe Mode"),

	// Full authoring — can clear + rebuild levels. Always prompts for confirmation.
	Danger  UMETA(DisplayName = "Danger Zone"),
};

// ─── Mission templates (Danger Zone level authoring) ─────────────────────────

UENUM(BlueprintType)
enum class EDCMissionTemplate : uint8
{
	Extraction     UMETA(DisplayName = "Extraction Zone"),
	Arena          UMETA(DisplayName = "Arena Gauntlet"),
	QuestHub       UMETA(DisplayName = "Quest Hub World"),
	ReactiveStory  UMETA(DisplayName = "Reactive Story World"),
};

/**
 * Per-user settings for DeltaCode.
 *
 * Uses Config=EditorPerProjectUserSettings so changes land in
 * Saved/Config/.../EditorPerProjectUserSettings.ini — per-user, gitignored by
 * default UE project templates. This is deliberate: the Anthropic API key must
 * never be committed (Hard Constraint #3), so we cannot use `defaultconfig`
 * (which writes to the source-controlled Config/Default*.ini).
 *
 * Accessible via Project Settings > Plugins > DeltaCode.
 *
 * [INPUT]  From: user input via Project Settings UI
 * [OUTPUT] To:   FDCAnthropicClient (API key, model), SDeltaCodeGeneratorPanel (defaults)
 */
UCLASS(Config=Game, meta=(DisplayName="DeltaCode"))
class DELTACODEEDITOR_API UDeltaCodeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDeltaCodeSettings();

	virtual FName GetContainerName() const override { return FName("Project"); }
	virtual FName GetCategoryName() const override { return FName("Plugins"); }
	virtual FName GetSectionName() const override { return FName("DeltaCode"); }

	// ── API credentials / models ────────────────────────────────────────────

	/**
	 * The Anthropic API key. Stored in Saved/Config — never committed to source control.
	 * Each user must enter their own key obtained from console.anthropic.com.
	 * Never logged, never embedded in source (Hard Constraint #3).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "DeltaCode|API",
	          meta = (DisplayName = "Anthropic API Key",
	                  Tooltip = "Your personal Anthropic API key. Get one at console.anthropic.com"))
	FString AnthropicAPIKey;

	/** Primary model for code and level generation. */
	UPROPERTY(Config, EditAnywhere, Category = "DeltaCode|API",
	          meta = (DisplayName = "Generation Model"))
	FString ClaudeModel = TEXT("claude-sonnet-4-20250514");

	/** Lightweight model used for the "Validate API Key" ping. */
	UPROPERTY(Config, EditAnywhere, Category = "DeltaCode|API",
	          meta = (DisplayName = "Validation Model",
	                  Tooltip = "Cheap model used by the Validate API Key button. Defaults to a Haiku-class model so pings cost ~nothing."))
	FString ValidationModel = TEXT("claude-haiku-4-5-20251001");

	/** HTTP request timeout for Anthropic calls. */
	UPROPERTY(Config, EditAnywhere, Category = "DeltaCode|API",
	          meta = (ClampMin = "5.0", ClampMax = "600.0",
	                  DisplayName = "Request Timeout (seconds)"))
	float RequestTimeoutSeconds = 60.0f;

	/** Upper bound on output tokens per generation request. */
	UPROPERTY(Config, EditAnywhere, Category = "DeltaCode|API",
	          meta = (ClampMin = "64", ClampMax = "64000",
	                  DisplayName = "Max Output Tokens"))
	int32 MaxOutputTokens = 4096;

	/**
	 * When true, logs HTTP request/response metadata (status, byte counts, model, latency).
	 * NEVER logs the API key, request bodies, or response bodies.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "DeltaCode|API",
	          meta = (DisplayName = "Verbose HTTP Logging"))
	bool bVerboseLogging = false;

	// ── Generation defaults ─────────────────────────────────────────────────

	/** Default generation mode for the plugin panel. Users can override per-request. */
	UPROPERTY(Config, EditAnywhere, Category = "DeltaCode|Generation",
	          meta = (DisplayName = "Default Generation Mode"))
	EDCGenerationMode DefaultGenerationMode = EDCGenerationMode::Safe;

	/** Default mission template for Danger Zone level authoring. */
	UPROPERTY(Config, EditAnywhere, Category = "DeltaCode|Generation",
	          meta = (DisplayName = "Default Mission Template"))
	EDCMissionTemplate DefaultMissionTemplate = EDCMissionTemplate::Extraction;

	/**
	 * Default camera perspective for generated player characters.
	 * Generation code picks ADCThirdPersonCharacter, ADCFirstPersonCharacter, or
	 * ADCDualPerspectiveCharacter as the project's default pawn class based on
	 * this setting. Changing it after generation does not re-point existing
	 * game modes — it only affects new Safe-Mode / Danger-Zone scaffolds.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "DeltaCode|Generation",
	          meta = (DisplayName = "Default Camera Mode"))
	EDCCameraMode DefaultCameraMode = EDCCameraMode::ThirdPerson;

	// ── Accessors ───────────────────────────────────────────────────────────

	static UDeltaCodeSettings* Get()
	{
		return GetMutableDefault<UDeltaCodeSettings>();
	}

	/** Structural check — key is non-empty and has the Anthropic prefix. Does not hit the network. */
	bool HasValidAPIKey() const
	{
		return !AnthropicAPIKey.IsEmpty() && AnthropicAPIKey.StartsWith(TEXT("sk-ant-"));
	}

	// ── Editor actions ──────────────────────────────────────────────────────

	/**
	 * Sends a minimal request to the Anthropic Messages API using the validation model,
	 * then reports success/failure via FMessageDialog. Confirms the key is live without
	 * costing a real generation. Never logs the key.
	 */
	UFUNCTION(CallInEditor, Category = "DeltaCode|API",
	          meta = (DisplayName = "Validate API Key"))
	void ValidateAPIKey();
};
