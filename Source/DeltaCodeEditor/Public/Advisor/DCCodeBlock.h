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

/**
 * One parsed fenced code block from an Anthropic response.
 *
 * Populated by FDCResponseParser, consumed by FDCFileWriter and the panel's
 * parsed-files list. Kept as a plain C++ struct so it travels through Slate
 * lambdas and lists without USTRUCT/UObject overhead.
 */
struct DELTACODEEDITOR_API FDCCodeBlock
{
	/** Fence language hint — "cpp", "h", "py", "json", "md", etc. May be empty. */
	FString Language;

	/**
	 * Relative path the model proposed for this block. Relative to the host
	 * project root (FPaths::ProjectDir()). May be empty when the model did not
	 * name a file; in that case the block is informational only.
	 */
	FString ProposedPath;

	/** Raw code content, with trailing fence newline stripped. */
	FString Content;

	/** Short human-readable hint describing how the path was obtained. */
	FString ParseNote;
};

/** Outcome of attempting to write a code block to disk. */
enum class EDCApplyResult : uint8
{
	Success,
	FailedNoPath,              // Block had no proposed path
	FailedPathTraversal,       // Path contained ".." or similar
	FailedPathNotAllowed,      // Path did not resolve under an allowed root
	FailedProtectedCore,       // Path lands in Content/DeltaCode/Core/ (protected)
	FailedExtensionNotAllowed, // File extension not in the Safe Mode allowlist
	FailedAlreadyExists,       // Target file already exists (Safe Mode is additive only)
	FailedWriteError,          // Disk write failed (permission, full, etc.)
};
