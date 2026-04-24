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
#include "Settings/DeltaCodeSettings.h" // for EDCMissionTemplate

/**
 * C++ bridge that drives the Danger Zone Python level-authoring pipeline.
 *
 * The bridge is the single chokepoint through which all destructive level
 * operations flow. Safe Mode code never touches it. The Editor Slate layer
 * shows a confirmation modal (Hard Constraint #2) before calling
 * ExecuteDangerZoneScript; this class does not prompt.
 *
 * [INPUT]  From: SDeltaCodeGeneratorPanel "Build Mission" button (after user confirms)
 * [OUTPUT] To:   UE embedded Python interpreter → dc_danger_zone.py → level actors
 */
class DELTACODEEDITOR_API FDCLevelScriptingBridge
{
public:
	/** True if the Python Script Plugin is loaded and initialized. */
	static bool IsPythonAvailable();

	/**
	 * Run dc_danger_zone.py with the given mission template. Blocks the game
	 * thread for the duration of the Python call — UE's embedded interpreter
	 * runs synchronously. Expect the call to take a few seconds for large
	 * missions.
	 *
	 * @param Template       Mission template to build.
	 * @param OutMessage     Human-readable result line for the panel status bar.
	 * @return               True if the script executed without raising.
	 */
	static bool ExecuteDangerZoneScript(EDCMissionTemplate Template, FString& OutMessage);

	/**
	 * Create required Blueprint assets under /Game/DeltaCode/Core/ if they
	 * don't already exist. Non-destructive — safe to call from either mode.
	 */
	static bool CreateCoreAssets(FString& OutMessage);

	/** Absolute path to dc_danger_zone.py inside the plugin's Content/Python folder. */
	static FString GetScriptPath();

	/** Short stable slug for a template — "extraction", "arena", "questhub", "reactivestory". */
	static FString TemplateSlug(EDCMissionTemplate Template);
};
