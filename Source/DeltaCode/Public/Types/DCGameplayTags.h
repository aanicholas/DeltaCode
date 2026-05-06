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
#include "NativeGameplayTags.h"

/**
 * Native gameplay tags for DeltaCode.
 * Declared here and defined in DCGameplayTags.cpp via UE_DEFINE_GAMEPLAY_TAG_COMMENT.
 * Access pattern: FDCGameplayTags::Input_Interact
 *
 * Adding a new tag requires:
 *   1. UE_DECLARE_GAMEPLAY_TAG_EXTERN here
 *   2. UE_DEFINE_GAMEPLAY_TAG_COMMENT in the .cpp
 */
namespace FDCGameplayTags
{
	// ── Input tags — matched against UDCInputConfig.ActionBindings ───────────
	DELTACODE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Move);
	DELTACODE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Look);
	DELTACODE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Jump);
	DELTACODE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Interact);
	DELTACODE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Fire);
	DELTACODE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Reload);
	DELTACODE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_TogglePerspective);
	DELTACODE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Pause);

	// ── UI event tags — broadcast from gameplay systems, consumed by widgets ─
	DELTACODE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Prompt_Interact);
	DELTACODE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Notification_General);

	// ── Interaction channel tags — categorize what a target is ──────────────
	DELTACODE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interaction_Pickup);
	DELTACODE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interaction_NPC);
	DELTACODE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interaction_Door);
	DELTACODE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interaction_Vendor);
	DELTACODE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interaction_QuestGiver);

	// ── Damage tags — applied by gameplay effects ───────────────────────────
	DELTACODE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Lethal);
}
