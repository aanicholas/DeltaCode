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
#include "Settings/DeltaCodeSettings.h" // for EDCGenerationMode / EDCMissionTemplate

/**
 * Builds system prompts for DeltaCode generation requests.
 *
 * This is the seam where Safe Mode guardrails (additive-only, no deletions) and
 * Danger Zone mission-template scaffolding get injected. The v1 implementation
 * returns a minimal placeholder that announces the mode + template — the real
 * advisor logic lands in 5A-Editor-4 (Safe Mode advisor) and 5A-Editor-5
 * (Danger Zone mission templates).
 *
 * [INPUT]  From: SDeltaCodeGeneratorPanel::OnGenerateClicked
 * [OUTPUT] To:   FDCAnthropicRequest::SystemPrompt
 */
namespace FDCPromptBuilder
{
	/**
	 * Build the system prompt that introduces the model to DeltaCode's rules
	 * and the requested mode + template context.
	 */
	DELTACODEEDITOR_API FString BuildSystemPrompt(EDCGenerationMode Mode,
	                                              EDCMissionTemplate Template);

	/** Human-readable name for a generation mode — used in the panel status bar. */
	DELTACODEEDITOR_API FText ModeDisplayName(EDCGenerationMode Mode);

	/** Human-readable name for a mission template. */
	DELTACODEEDITOR_API FText TemplateDisplayName(EDCMissionTemplate Template);
}
