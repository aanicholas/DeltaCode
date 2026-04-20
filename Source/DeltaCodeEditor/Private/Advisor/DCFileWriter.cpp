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
#include "Advisor/DCFileWriter.h"
#include "DeltaCodeEditorLog.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "DCFileWriter"

namespace DCFileWriterPrivate
{
	/** Extensions we allow Safe Mode to write. Deliberately excludes .uasset/.umap/binaries. */
	static const TArray<FString>& AllowedExtensions()
	{
		static const TArray<FString> List = {
			TEXT("cpp"), TEXT("h"), TEXT("hpp"), TEXT("inl"),
			TEXT("cs"),            // Build.cs / Target.cs
			TEXT("py"),            // Danger Zone scripts, tooling
			TEXT("json"), TEXT("ini"), TEXT("txt"), TEXT("md"),
		};
		return List;
	}

	/** Normalise separators and collapse redundant elements — never returns a path with ".." intact. */
	static FString NormaliseRelative(const FString& In)
	{
		FString Out = In;
		FPaths::NormalizeFilename(Out); // flips \ to /, removes duplicate slashes
		// Strip leading "./" if present.
		while (Out.StartsWith(TEXT("./")))
		{
			Out.RightChopInline(2, EAllowShrinking::No);
		}
		// Strip leading "/" — we want a relative path.
		while (Out.StartsWith(TEXT("/")))
		{
			Out.RightChopInline(1, EAllowShrinking::No);
		}
		return Out;
	}

	/** True iff Path contains a ".." path segment after normalisation. */
	static bool HasParentTraversal(const FString& NormalisedPath)
	{
		TArray<FString> Segments;
		NormalisedPath.ParseIntoArray(Segments, TEXT("/"), /*CullEmpty*/ true);
		for (const FString& Seg : Segments)
		{
			if (Seg == TEXT(".."))
			{
				return true;
			}
		}
		return false;
	}

	/** True iff AbsPath resolves under Root (both must be absolute, same separator style). */
	static bool IsUnder(const FString& AbsPath, const FString& Root)
	{
		FString NormPath = AbsPath;
		FString NormRoot = Root;
		FPaths::NormalizeFilename(NormPath);
		FPaths::NormalizeFilename(NormRoot);
		if (!NormRoot.EndsWith(TEXT("/")))
		{
			NormRoot.AppendChar(TEXT('/'));
		}
		// Case-insensitive comparison — macOS/Windows filesystems are typically case-insensitive.
		return NormPath.StartsWith(NormRoot, ESearchCase::IgnoreCase);
	}

	static FString ProjectSourceRoot()   { return FPaths::ConvertRelativePathToFull(FPaths::GameSourceDir()); }
	static FString DeltaCodeContentRoot()
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectContentDir(), TEXT("DeltaCode")));
	}
	static FString ProtectedCoreRoot()
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectContentDir(), TEXT("DeltaCode"), TEXT("Core")));
	}
}

EDCApplyResult FDCFileWriter::Apply(const FDCCodeBlock& Block,
                                    FString& OutAbsolutePath,
                                    FString& OutErrorDetail)
{
	using namespace DCFileWriterPrivate;

	OutAbsolutePath.Reset();
	OutErrorDetail.Reset();

	if (Block.ProposedPath.IsEmpty())
	{
		OutErrorDetail = TEXT("block has no proposed path");
		return EDCApplyResult::FailedNoPath;
	}

	const FString Relative = NormaliseRelative(Block.ProposedPath);

	if (HasParentTraversal(Relative))
	{
		OutErrorDetail = TEXT("path contains '..' traversal");
		return EDCApplyResult::FailedPathTraversal;
	}

	// Extension check
	const FString Extension = FPaths::GetExtension(Relative).ToLower();
	if (Extension.IsEmpty() || !AllowedExtensions().Contains(Extension))
	{
		OutErrorDetail = FString::Printf(TEXT("extension '%s' not permitted in Safe Mode"),
			Extension.IsEmpty() ? TEXT("<none>") : *Extension);
		return EDCApplyResult::FailedExtensionNotAllowed;
	}

	// Resolve to absolute and test against allowed roots.
	const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FString AbsCandidate = FPaths::ConvertRelativePathToFull(FPaths::Combine(ProjectDir, Relative));
	FPaths::CollapseRelativeDirectories(AbsCandidate);

	const FString SourceRoot   = ProjectSourceRoot();
	const FString ContentRoot  = DeltaCodeContentRoot();
	const FString CoreProtected = ProtectedCoreRoot();

	const bool bUnderSource  = IsUnder(AbsCandidate, SourceRoot);
	const bool bUnderContent = IsUnder(AbsCandidate, ContentRoot);

	if (!bUnderSource && !bUnderContent)
	{
		OutErrorDetail = TEXT("path is outside Source/ and Content/DeltaCode/");
		return EDCApplyResult::FailedPathNotAllowed;
	}

	if (bUnderContent && IsUnder(AbsCandidate, CoreProtected))
	{
		OutErrorDetail = TEXT("Content/DeltaCode/Core/ is protected");
		return EDCApplyResult::FailedProtectedCore;
	}

	// Additive-only: refuse to overwrite.
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.FileExists(*AbsCandidate))
	{
		OutErrorDetail = TEXT("file already exists (Safe Mode never overwrites)");
		return EDCApplyResult::FailedAlreadyExists;
	}

	// Ensure parent directory exists before writing.
	const FString ParentDir = FPaths::GetPath(AbsCandidate);
	if (!PlatformFile.DirectoryExists(*ParentDir))
	{
		if (!PlatformFile.CreateDirectoryTree(*ParentDir))
		{
			OutErrorDetail = TEXT("could not create parent directory");
			return EDCApplyResult::FailedWriteError;
		}
	}

	if (!FFileHelper::SaveStringToFile(Block.Content, *AbsCandidate,
	                                   FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutErrorDetail = TEXT("FFileHelper::SaveStringToFile returned false");
		return EDCApplyResult::FailedWriteError;
	}

	UE_LOG(LogDeltaCodeEditor, Log,
		TEXT("[FileWriter] wrote %s (%d bytes)"), *AbsCandidate, Block.Content.Len());

	OutAbsolutePath = AbsCandidate;
	return EDCApplyResult::Success;
}

FText FDCFileWriter::ResultToText(EDCApplyResult Result, const FString& Detail)
{
	switch (Result)
	{
	case EDCApplyResult::Success:
		return FText::Format(LOCTEXT("ApplyOkFmt", "Applied → {0}"), FText::FromString(Detail));

	case EDCApplyResult::FailedNoPath:
		return LOCTEXT("ApplyNoPath", "No file path on this block.");

	case EDCApplyResult::FailedPathTraversal:
		return LOCTEXT("ApplyTraversal", "Rejected — path traversal (..).");

	case EDCApplyResult::FailedPathNotAllowed:
		return LOCTEXT("ApplyBadRoot",
			"Rejected — Safe Mode only writes under Source/ or Content/DeltaCode/.");

	case EDCApplyResult::FailedProtectedCore:
		return LOCTEXT("ApplyCore",
			"Rejected — Content/DeltaCode/Core/ is protected from Safe Mode writes.");

	case EDCApplyResult::FailedExtensionNotAllowed:
		return FText::Format(
			LOCTEXT("ApplyExt", "Rejected — {0}"),
			FText::FromString(Detail));

	case EDCApplyResult::FailedAlreadyExists:
		return LOCTEXT("ApplyExists",
			"Rejected — file exists. Safe Mode never overwrites.");

	case EDCApplyResult::FailedWriteError:
		return FText::Format(
			LOCTEXT("ApplyWrite", "Write failed — {0}"),
			FText::FromString(Detail));

	default:
		return LOCTEXT("ApplyUnknown", "Unknown result.");
	}
}

#undef LOCTEXT_NAMESPACE
