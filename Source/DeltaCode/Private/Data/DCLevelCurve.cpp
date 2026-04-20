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
#include "Data/DCLevelCurve.h"

int32 UDCLevelCurve::GetXPThresholdForLevel(int32 TargetLevel) const
{
	if (TargetLevel <= 1)
	{
		return 0;
	}
	for (const FDCLevelUpBonus& Entry : Levels)
	{
		if (Entry.Level == TargetLevel)
		{
			return Entry.XPThreshold;
		}
	}
	return MAX_int32;
}

bool UDCLevelCurve::GetBonusForLevel(int32 TargetLevel, FDCLevelUpBonus& OutBonus) const
{
	for (const FDCLevelUpBonus& Entry : Levels)
	{
		if (Entry.Level == TargetLevel)
		{
			OutBonus = Entry;
			return true;
		}
	}
	return false;
}

int32 UDCLevelCurve::GetMaxLevel() const
{
	int32 Max = 1;
	for (const FDCLevelUpBonus& Entry : Levels)
	{
		if (Entry.Level > Max)
		{
			Max = Entry.Level;
		}
	}
	return Max;
}
