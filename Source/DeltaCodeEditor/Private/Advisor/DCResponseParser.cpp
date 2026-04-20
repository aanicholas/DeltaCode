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
#include "Advisor/DCResponseParser.h"

namespace DCResponseParserPrivate
{
	static const TCHAR* Fence = TEXT("```");

	/** True when the line plausibly names a file — has a slash and a dotted extension. */
	static bool LooksLikePath(const FString& Candidate)
	{
		if (Candidate.IsEmpty() || Candidate.Len() > 512)
		{
			return false;
		}
		if (!Candidate.Contains(TEXT("/")))
		{
			return false;
		}
		const int32 LastDot = Candidate.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (LastDot == INDEX_NONE || LastDot == Candidate.Len() - 1)
		{
			return false;
		}
		// Reject obvious sentences ("some text. Here is something") — path-like
		// strings don't embed spaces.
		return !Candidate.Contains(TEXT(" "));
	}

	/** Try to read a path out of a first-line comment. Returns empty if not found. */
	static FString ExtractPathFromFirstLine(const FString& Content)
	{
		int32 EndOfFirst = Content.Find(TEXT("\n"));
		const FString FirstLine = (EndOfFirst == INDEX_NONE)
			? Content
			: Content.Left(EndOfFirst);
		FString Trimmed = FirstLine.TrimStartAndEnd();

		// C/C++ style: "// path"
		if (Trimmed.StartsWith(TEXT("//")))
		{
			FString Rest = Trimmed.RightChop(2).TrimStartAndEnd();
			if (LooksLikePath(Rest))
			{
				return Rest;
			}
		}
		// Script style: "# path"  — careful not to match "#include" (but LooksLikePath rejects that since no slash)
		if (Trimmed.StartsWith(TEXT("#")))
		{
			FString Rest = Trimmed.RightChop(1).TrimStartAndEnd();
			if (LooksLikePath(Rest))
			{
				return Rest;
			}
		}
		// HTML/XML style: "<!-- path -->"
		if (Trimmed.StartsWith(TEXT("<!--")) && Trimmed.EndsWith(TEXT("-->")))
		{
			FString Rest = Trimmed.Mid(4, Trimmed.Len() - 7).TrimStartAndEnd();
			if (LooksLikePath(Rest))
			{
				return Rest;
			}
		}
		return FString();
	}
}

TArray<FDCCodeBlock> FDCResponseParser::Parse(const FString& ResponseText)
{
	using namespace DCResponseParserPrivate;

	TArray<FDCCodeBlock> Out;
	int32 Cursor = 0;
	const int32 Length = ResponseText.Len();

	while (Cursor < Length)
	{
		const int32 FenceStart = ResponseText.Find(Fence, ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
		if (FenceStart == INDEX_NONE)
		{
			break;
		}

		const int32 HeaderStart = FenceStart + 3;
		const int32 HeaderEnd = ResponseText.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, HeaderStart);
		if (HeaderEnd == INDEX_NONE)
		{
			// Unterminated fence header — nothing useful left to parse.
			break;
		}

		FString Header = ResponseText.Mid(HeaderStart, HeaderEnd - HeaderStart).TrimStartAndEnd();

		const int32 ContentStart = HeaderEnd + 1;
		const int32 FenceEnd = ResponseText.Find(Fence, ESearchCase::CaseSensitive, ESearchDir::FromStart, ContentStart);
		if (FenceEnd == INDEX_NONE)
		{
			// Unterminated block — leave the remainder alone and stop.
			break;
		}

		FString Content = ResponseText.Mid(ContentStart, FenceEnd - ContentStart);
		// Drop exactly the terminating newline if present — preserve intentional trailing blank lines.
		if (Content.EndsWith(TEXT("\n")))
		{
			Content.LeftChopInline(1, EAllowShrinking::No);
		}

		FDCCodeBlock Block;

		// Header may be "lang" or "lang:path" or empty.
		int32 ColonIndex = INDEX_NONE;
		if (Header.FindChar(':', ColonIndex))
		{
			Block.Language = Header.Left(ColonIndex).TrimStartAndEnd();
			Block.ProposedPath = Header.Mid(ColonIndex + 1).TrimStartAndEnd();
			Block.ParseNote = TEXT("Path from fence header.");
		}
		else
		{
			Block.Language = Header;
		}

		if (Block.ProposedPath.IsEmpty())
		{
			const FString InferredPath = ExtractPathFromFirstLine(Content);
			if (!InferredPath.IsEmpty())
			{
				Block.ProposedPath = InferredPath;
				Block.ParseNote = TEXT("Path inferred from first-line comment.");
			}
			else
			{
				Block.ParseNote = TEXT("No file path found — informational only.");
			}
		}

		Block.Content = MoveTemp(Content);
		Out.Add(MoveTemp(Block));

		Cursor = FenceEnd + 3;
	}

	return Out;
}
