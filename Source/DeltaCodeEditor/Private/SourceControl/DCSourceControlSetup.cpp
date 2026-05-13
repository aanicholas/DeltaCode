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
#include "SourceControl/DCSourceControlSetup.h"

#include "HAL/PlatformProcess.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "DCSourceControlSetup"

DEFINE_LOG_CATEGORY_STATIC(LogDeltaCodeSCSetup, Log, All);

namespace DCSourceControlSetupPrivate
{
	/**
	 * Spawn a process synchronously, wait for it to finish, return the exit
	 * code via OutReturnCode. Standard out/err are not captured — git is
	 * quiet on success and verbose on failure, and we surface failures
	 * through return codes rather than parsing strings.
	 */
	static bool RunProcessSync(const FString& Executable,
	                           const FString& Args,
	                           const FString& WorkingDir,
	                           int32& OutReturnCode)
	{
		void* ReadPipe = nullptr;
		void* WritePipe = nullptr;
		FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

		FProcHandle Handle = FPlatformProcess::CreateProc(
			*Executable,
			*Args,
			/*bLaunchDetached=*/false,
			/*bLaunchHidden=*/true,
			/*bLaunchReallyHidden=*/true,
			/*OutProcessID=*/nullptr,
			/*PriorityModifier=*/0,
			WorkingDir.IsEmpty() ? nullptr : *WorkingDir,
			WritePipe);

		if (!Handle.IsValid())
		{
			FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
			OutReturnCode = -1;
			return false;
		}

		FPlatformProcess::WaitForProc(Handle);
		FPlatformProcess::GetProcReturnCode(Handle, &OutReturnCode);
		FPlatformProcess::CloseProc(Handle);
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		return true;
	}

	/**
	 * Canonical UE5 .gitignore baseline. Keeps Content/Config/Source/Plugins
	 * (sources only) under version control; excludes regenerable build
	 * artifacts and per-plugin binaries/intermediates. Conservative — adds
	 * Rider/.idea and macOS .DS_Store on top of the Visual Studio defaults
	 * since teams commonly hit both.
	 */
	static const TCHAR* GitignoreTemplate = TEXT(
		"# DeltaCode-managed .gitignore for Unreal Engine 5 projects.\n"
		"# Generated on first Danger Zone setup. Edit freely.\n"
		"\n"
		"# Build artifacts (large, regenerable)\n"
		"Binaries/\n"
		"Build/\n"
		"DerivedDataCache/\n"
		"Intermediate/\n"
		"Saved/\n"
		"\n"
		"# Per-plugin binaries / intermediates\n"
		"Plugins/*/Binaries/\n"
		"Plugins/*/Intermediate/\n"
		"\n"
		"# Visual Studio\n"
		".vs/\n"
		"*.VC.db\n"
		"*.opensdf\n"
		"*.opendb\n"
		"*.sdf\n"
		"*.suo\n"
		"\n"
		"# Rider / CLion\n"
		".idea/\n"
		"\n"
		"# macOS\n"
		".DS_Store\n"
		"\n"
		"# Generated headers\n"
		"*.generated.h\n"
		"*.generated.cpp\n"
		"\n"
		"# Local user settings — never commit\n"
		"*.user\n"
	);

	/**
	 * Multi-section markdown shown in the Response box when the user picks
	 * "Learn about source control options". No URLs (Markdown links rot
	 * across UE versions and can't be auto-verified) — installation hints
	 * direct users to canonical product names.
	 */
	static const TCHAR* ExplanationMarkdown = TEXT(
		"# Source Control for Unreal Engine 5\n"
		"\n"
		"DeltaCode strongly recommends source control before any Danger Zone "
		"operation. Build Mission clears and rebuilds level content — without "
		"version control, mistakes are unrecoverable. Three mainstream options "
		"work well with UE5:\n"
		"\n"
		"## Git (free, distributed)\n"
		"\n"
		"Best for: solo developers, small teams, plugin authors, anyone already "
		"comfortable with Git. Free, ubiquitous, integrates with GitHub / GitLab "
		"/ Bitbucket / self-hosted.\n"
		"\n"
		"Caveats:\n"
		"  - Large binary assets (textures, audio, meshes) bloat the repo over "
		"    time. Use Git LFS for any binary file over a few hundred KB.\n"
		"  - No asset locking — two team members editing the same .uasset will "
		"    collide and a merge is effectively impossible. For teams over ~3 "
		"    people working on the same content, consider Perforce or Plastic.\n"
		"\n"
		"Setup: install Git from the official Git project (git-scm.com). "
		"DeltaCode's \"Set up Git\" button runs `git init` for you and writes a "
		"UE5-appropriate .gitignore. For LFS, run `git lfs install` after init, "
		"then `git lfs track \"*.uasset\" \"*.umap\" \"*.fbx\" \"*.wav\" \"*.png\"`.\n"
		"\n"
		"## Perforce / Helix Core (paid, centralized)\n"
		"\n"
		"Best for: full game studios, teams with dedicated tech-art / content "
		"pipelines, projects with hundreds of GB of binary assets. Industry "
		"standard for AAA UE development. Asset locking prevents the most "
		"common multi-person collision pain.\n"
		"\n"
		"Caveats:\n"
		"  - Requires a Perforce server. Self-hosted is free for up to 5 users / "
		"    20 workspaces (\"Helix Core Free\") but you administer the server "
		"    yourself. Cloud-hosted via Perforce or Assembla incurs cost.\n"
		"  - Steeper learning curve than Git for first-time users.\n"
		"  - DeltaCode cannot automate Perforce setup — server provisioning, "
		"    user accounts, and depot layout are too project-specific. Configure "
		"    Perforce in UE5 via Source Control → Connect to Source Control → "
		"    Provider: Perforce.\n"
		"\n"
		"Setup: download Helix Core from Perforce (perforce.com), install P4V "
		"client and the server (P4D) for self-hosted, then connect from UE5.\n"
		"\n"
		"## Plastic SCM / Unity Version Control (free for small teams)\n"
		"\n"
		"Best for: small-to-medium teams who want Perforce-style asset locking "
		"without running a server. Free for up to 3 users / 5 GB; paid tiers for "
		"more. Acquired by Unity but still works fine with UE5.\n"
		"\n"
		"Caveats:\n"
		"  - Smaller community than Git or Perforce — fewer Stack Overflow "
		"    answers when you hit a problem.\n"
		"  - Cloud-only on the free tier; self-hosted server is a paid feature.\n"
		"\n"
		"Setup: download from plasticscm.com, create a cloud organisation, "
		"clone your repo, then connect from UE5 via Source Control → Provider: "
		"Plastic.\n"
		"\n"
		"## DeltaCode's recommendation\n"
		"\n"
		"  - Solo or 1–3 person team, mostly C++ / Blueprint with manageable "
		"    asset count → **Git + LFS**. DeltaCode's \"Set up Git\" button "
		"    handles the init step.\n"
		"  - 3+ person team or asset-heavy content → **Plastic SCM** if you want "
		"    no-server convenience, **Perforce** if you have a server budget and "
		"    a sysadmin.\n"
		"  - Studio with existing pipeline → match whatever the rest of the "
		"    team already uses.\n"
		"\n"
		"After configuring source control, re-click Build Mission. The gate "
		"will recognise the active provider and proceed straight to the "
		"destructive-action confirmation.\n"
	);
}

bool FDCSourceControlSetup::IsSourceControlConfigured()
{
	ISourceControlModule& SCModule = ISourceControlModule::Get();
	if (!SCModule.IsEnabled())
	{
		return false;
	}
	return SCModule.GetProvider().IsAvailable();
}

bool FDCSourceControlSetup::RunGitInit(FString& OutMessage)
{
	using namespace DCSourceControlSetupPrivate;

	const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	// Step 1: git init.  Idempotent — re-init in an existing repo is a no-op.
	int32 InitRetCode = -1;
	const bool bSpawnedInit = RunProcessSync(
		TEXT("git"), TEXT("init"), ProjectDir, InitRetCode);

	if (!bSpawnedInit)
	{
		OutMessage = TEXT(
			"Could not launch `git`. Install Git from git-scm.com and ensure "
			"it is on your PATH, then try again.");
		UE_LOG(LogDeltaCodeSCSetup, Error, TEXT("%s"), *OutMessage);
		return false;
	}
	if (InitRetCode != 0)
	{
		OutMessage = FString::Printf(
			TEXT("`git init` exited with code %d. Check the Output Log for details."),
			InitRetCode);
		UE_LOG(LogDeltaCodeSCSetup, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	// Step 2: write .gitignore.  Overwrite is intentional — the user can edit
	// afterwards, and a stale partial .gitignore is worse than the canonical
	// baseline.  If they need to merge with existing rules they can do that
	// before their first commit.
	const FString GitignorePath = FPaths::Combine(ProjectDir, TEXT(".gitignore"));
	if (!FFileHelper::SaveStringToFile(GitignoreTemplate, *GitignorePath))
	{
		OutMessage = FString::Printf(
			TEXT("`git init` succeeded but failed to write %s. Check folder permissions."),
			*GitignorePath);
		UE_LOG(LogDeltaCodeSCSetup, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	// Step 3: Windows long-paths.  UE5's deeply nested folder structure
	// (Plugins/<Name>/Source/<Module>/...) trips Windows' 260-char path
	// limit.  Setting core.longpaths=true is the canonical fix.  Best-effort:
	// a failure here doesn't undo the init, just warn.
#if PLATFORM_WINDOWS
	int32 LongPathsRetCode = -1;
	const bool bSpawnedLP = RunProcessSync(
		TEXT("git"),
		TEXT("config --global core.longpaths true"),
		FString(),
		LongPathsRetCode);
	if (!bSpawnedLP || LongPathsRetCode != 0)
	{
		UE_LOG(LogDeltaCodeSCSetup, Warning,
			TEXT("Could not set git core.longpaths=true (Windows). "
			     "If you hit path-length errors, run "
			     "`git config --global core.longpaths true` manually."));
	}
#endif

	OutMessage = TEXT(
		"Git initialized. Remember to commit before running Danger Zone operations.");
	UE_LOG(LogDeltaCodeSCSetup, Log, TEXT("%s"), *OutMessage);
	return true;
}

const FString& FDCSourceControlSetup::GetSourceControlExplanation()
{
	using namespace DCSourceControlSetupPrivate;
	static const FString Cached(ExplanationMarkdown);
	return Cached;
}

#undef LOCTEXT_NAMESPACE
