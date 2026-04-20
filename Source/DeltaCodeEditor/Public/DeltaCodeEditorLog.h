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
#include "Logging/LogMacros.h"

/**
 * Module-wide log category for the DeltaCode editor plugin.
 *
 * Use this for everything that touches the Anthropic API surface, Slate panels,
 * and the Python level-authoring pipeline. NEVER log API keys, request bodies,
 * or response bodies under this category (Hard Constraint #3). Metadata only —
 * endpoints, model slugs, byte counts, HTTP status, latency.
 */
DECLARE_LOG_CATEGORY_EXTERN(LogDeltaCodeEditor, Log, All);
