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
#include "Data/DCItemDefinition.h"

FPrimaryAssetId UDCItemDefinition::GetPrimaryAssetId() const
{
	// Use the ItemID as the primary asset name when set; otherwise fall back to
	// the default (asset package path). Enables AssetManager discovery by ID.
	if (!ItemID.IsNone())
	{
		return FPrimaryAssetId(FPrimaryAssetType(TEXT("DCItem")), ItemID);
	}
	return Super::GetPrimaryAssetId();
}
