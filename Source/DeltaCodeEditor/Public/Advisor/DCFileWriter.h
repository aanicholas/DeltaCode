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
#include "Advisor/DCCodeBlock.h"

/**
 * Writes parsed code blocks to disk under Safe Mode guardrails.
 *
 * Allowed destinations (all rooted at FPaths::ProjectDir()):
 *   - Source/                       — C++ source, Build.cs, Target.cs
 *   - Content/DeltaCode/            — plugin-namespaced content (text only)
 *
 * Destination rejections:
 *   - ".." path traversal
 *   - Anything outside the allowed roots
 *   - Content/DeltaCode/Core/ (protected — Lyra-style base content)
 *   - Extensions not in the Safe Mode allowlist
 *   - Files that already exist (Hard Constraint #1 — Safe Mode is additive only)
 *
 * [INPUT]  From: SDeltaCodeGeneratorPanel Apply button
 * [OUTPUT] To:   disk (via FFileHelper::SaveStringToFile)
 */
class DELTACODEEDITOR_API FDCFileWriter
{
public:
	/**
	 * Attempt to write Block to disk.
	 *
	 * @param Block             The parsed code block.
	 * @param OutAbsolutePath   On success, populated with the absolute path written.
	 * @param OutErrorDetail    On failure, a short human-readable explanation.
	 * @return                  EDCApplyResult::Success on write, otherwise a specific failure.
	 */
	static EDCApplyResult Apply(const FDCCodeBlock& Block,
	                            FString& OutAbsolutePath,
	                            FString& OutErrorDetail);

	/** Stringify a result code for the UI. Detail is appended for non-success cases. */
	static FText ResultToText(EDCApplyResult Result, const FString& Detail);
};
