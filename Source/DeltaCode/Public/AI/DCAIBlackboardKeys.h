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
 * Canonical blackboard key names used by DeltaCode enemy AI.
 *
 * The BlackboardData asset authored in the editor must declare entries with
 * exactly these names — C++ tasks look them up via FName, so a typo silently
 * fails with "key not found". Centralising the constants here means authoring
 * mistakes surface as compile errors (in C++) and as predictable log spam
 * (on the editor side).
 *
 * Recommended blackboard asset: BB_DC_Enemy_Default under
 * Content/DeltaCode/AI/ with keys:
 *   - TargetActor        (Object : AActor)
 *   - LastKnownLocation  (Vector)
 *   - HomeLocation       (Vector)
 *   - bIsInvestigating   (Bool)
 */
namespace DCAIBlackboardKeys
{
	DELTACODE_API extern const FName TargetActor;
	DELTACODE_API extern const FName LastKnownLocation;
	DELTACODE_API extern const FName HomeLocation;
	DELTACODE_API extern const FName bIsInvestigating;
}
