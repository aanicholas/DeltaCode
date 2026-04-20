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
 * Extracts fenced code blocks from Anthropic response text.
 *
 * Recognises:
 *   - ```lang\n...```              — language only
 *   - ```lang:path/to/file.ext\n...``` — language + inline path
 *   - ```lang\n// path/to/file.ext\n...``` — path in first-line C/C++ comment
 *   - ```lang\n# path/to/file.ext\n...```  — path in first-line script comment
 *
 * Blocks without a recognisable path are still returned (so the UI can show
 * them), but cannot be applied to disk by FDCFileWriter.
 *
 * Deliberately does not use regex — UE's FRegexPattern would pull in an extra
 * dependency, and the fence grammar is trivial enough for manual scanning.
 *
 * [INPUT]  From: FDCAnthropicResponse::Text
 * [OUTPUT] To:   FDCFileWriter::Apply / panel list view
 */
class DELTACODEEDITOR_API FDCResponseParser
{
public:
	/** Parse every fenced block in the response. Order is preserved. */
	static TArray<FDCCodeBlock> Parse(const FString& ResponseText);
};
