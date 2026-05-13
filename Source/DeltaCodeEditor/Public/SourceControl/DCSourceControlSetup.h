/*
 * DeltaCode - Unreal Engine Code Helper
 * Copyright (c) 2026 Andrew Nicholas
 *
 * This program is free software: you can redistribute
 * it and/or modify it under the terms of the GNU
 * General Public License as published by the Free
 * Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 */
#pragma once

#include "CoreMinimal.h"

/**
 * Source-control awareness used by the Danger Zone Build Mission gate.
 *
 * The gate fires only for destructive level operations (Build Mission today).
 * CreateCoreAssets is additive and Generate produces text the user manually
 * applies, so neither requires a source-control check.
 *
 * [INPUT]  From: SDeltaCodeGeneratorPanel::OnBuildMissionClicked — fires the gate
 * [OUTPUT] To:   Editor source control system (UE ISourceControlModule), git binary
 */
class DELTACODEEDITOR_API FDCSourceControlSetup
{
public:
	/**
	 * True when UE5's Source Control module reports an active provider
	 * (Git, Perforce, Plastic, Subversion — any provider UE recognizes).
	 * This respects whatever the user configured in Editor Preferences →
	 * Source Control regardless of whether a .git/ directory exists on disk.
	 */
	static bool IsSourceControlConfigured();

	/**
	 * Run `git init` in the project root, write a UE5-appropriate
	 * .gitignore, and on Windows set core.longpaths=true. Idempotent —
	 * safe to re-run if .git/ already exists.
	 *
	 * Returns false with a descriptive message in OutMessage if the git
	 * binary is missing, the init fails, or the .gitignore write fails.
	 * On success, OutMessage contains a one-line summary suitable for
	 * the panel status bar.
	 */
	static bool RunGitInit(FString& OutMessage);

	/**
	 * Hardcoded markdown explaining Git / Perforce / Plastic SCM trade-offs
	 * and setup steps for UE5 projects. Returned by const reference —
	 * static content, no allocation per call. Drops into the panel's
	 * Response box when the user picks "Learn about source control options".
	 */
	static const FString& GetSourceControlExplanation();
};
