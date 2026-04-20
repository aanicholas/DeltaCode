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
#include "Data/DCFactionDefinition.h"

FPrimaryAssetId UDCFactionDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("DCFaction"),
		FactionID.IsNone() ? GetFName() : FactionID);
}

EDCFactionStance UDCFactionDefinition::GetDefaultStanceToward(
	const FGameplayTag& OtherFactionTag) const
{
	if (!OtherFactionTag.IsValid())
	{
		return FallbackStance;
	}
	for (const FDCFactionDefaultStance& Row : DefaultStances)
	{
		if (Row.OtherFaction == OtherFactionTag)
		{
			return Row.Stance;
		}
	}
	return FallbackStance;
}

EDCFactionStance UDCFactionDefinition::ResolvePlayerStance(int32 Reputation) const
{
	if (Reputation >= AlliedThreshold)   return EDCFactionStance::Allied;
	if (Reputation >= FriendlyThreshold) return EDCFactionStance::Friendly;
	if (Reputation <= HostileThreshold)  return EDCFactionStance::Hostile;
	if (Reputation <= UnfriendlyThreshold) return EDCFactionStance::Unfriendly;
	return EDCFactionStance::Neutral;
}
