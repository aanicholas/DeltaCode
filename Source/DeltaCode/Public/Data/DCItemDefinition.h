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
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Types/DCCoreTypes.h"
#include "DCItemDefinition.generated.h"

class UTexture2D;
class UStaticMesh;

/**
 * Base data asset for every item in the game.
 * Created as DA_DC_Item_[Name] in Content/DeltaCode/Data/Items/.
 *
 * [OUTPUT] To: UDCInventoryComponent — resolved via FDCItemHandle → ItemID
 * [OUTPUT] To: ADCPickupBase — loads WorldMesh when placed in level
 * [OUTPUT] To: UDCVendorComponent — stock rows reference ItemID
 *
 * All inventory items MUST have a unique ItemID. The registry enforces this on load.
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API UDCItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Asset manager primary type — used by registry discovery
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// ── Identity ────────────────────────────────────────────────────────────

	// Unique identifier — matches FDCItemHandle::ItemID
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AssetRegistrySearchable,
	          Category = "DeltaCode|Item")
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Item",
	          meta = (MultiLine = "true"))
	FText Description;

	// ── Classification ──────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AssetRegistrySearchable,
	          Category = "DeltaCode|Item")
	EDCItemType ItemType = EDCItemType::Miscellaneous;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Item")
	EDCItemRarity Rarity = EDCItemRarity::Common;

	// Stacking rules
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Item",
	          meta = (ClampMin = "1"))
	int32 MaxStackSize = 1;

	// If true, the player can hold only one instance — disables stack and pickup duplication
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Item")
	bool bIsUnique = false;

	// Encumbrance (kg). 0 = weightless.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Item",
	          meta = (ClampMin = "0.0"))
	float Weight = 0.0f;

	// Base value — vendor buy price is modified by reputation / barter later
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Item",
	          meta = (ClampMin = "0"))
	int32 BaseValue = 0;

	// ── Presentation ────────────────────────────────────────────────────────

	// Icon displayed in inventory, vendor, quest log
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Item|Visual")
	TSoftObjectPtr<UTexture2D> Icon;

	// World-placed mesh when this item is a pickup in the level
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Item|Visual")
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	// ── Tags ────────────────────────────────────────────────────────────────

	// Gameplay tags — used by quest conditions, ability filters, loot tables
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Item")
	FGameplayTagContainer ItemTags;

	// ── Accessors ───────────────────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Item")
	FDCItemHandle GetItemHandle() const { return FDCItemHandle(ItemID); }
};
