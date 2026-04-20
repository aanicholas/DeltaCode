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
#include "Types/DCGameplayTags.h"

namespace FDCGameplayTags
{
	// ── Input tags ──────────────────────────────────────────────────────────
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Move,
		"Input.Move",
		"2D movement input (WASD / left stick).");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Look,
		"Input.Look",
		"2D camera look input (mouse / right stick).");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Jump,
		"Input.Jump",
		"Jump input (space / A button).");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Interact,
		"Input.Interact",
		"Interact with world object under focus (E / X button).");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Fire,
		"Input.Fire",
		"Primary weapon fire input (LMB / RT).");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Reload,
		"Input.Reload",
		"Weapon reload input (R / X button).");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_TogglePerspective,
		"Input.TogglePerspective",
		"Toggle between third- and first-person views on dual-perspective pawns.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Pause,
		"Input.Pause",
		"Open / close the pause menu (Esc / Start button).");

	// ── UI event tags ───────────────────────────────────────────────────────
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Prompt_Interact,
		"UI.Prompt.Interact",
		"HUD should show the interact prompt for the focused target.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Notification_General,
		"UI.Notification.General",
		"Generic on-screen notification (item picked up, objective updated).");

	// ── Interaction channel tags ────────────────────────────────────────────
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interaction_Pickup,
		"Interaction.Pickup",
		"Target is a pickup item actor.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interaction_NPC,
		"Interaction.NPC",
		"Target is an interactable NPC (dialogue-capable).");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interaction_Door,
		"Interaction.Door",
		"Target is a door or similar passage.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interaction_Vendor,
		"Interaction.Vendor",
		"Target is a vendor — opens trade UI.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interaction_QuestGiver,
		"Interaction.QuestGiver",
		"Target is a quest-giving NPC.");
}
