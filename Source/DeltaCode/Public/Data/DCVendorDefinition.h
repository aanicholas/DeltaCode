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
#include "Types/DCDataDefinitions.h"
#include "DCVendorDefinition.generated.h"

/**
 * Vendor definition data asset.
 * Created as DA_DC_Vendor_[Name] in Content/DeltaCode/Data/Vendors/.
 *
 * [INPUT]  From: UDCNPCDefinition — referenced when NPC is a vendor role
 * [OUTPUT] To:   UDCVendorComponent::BuildStock() — provides stock template
 * [OUTPUT] To:   W_DC_VendorPanel — display items, prices, buyback preview
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API UDCVendorDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AssetRegistrySearchable,
	          Category = "DeltaCode|Vendor")
	FName VendorID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Vendor")
	FText DisplayName;

	// Default currency the vendor accepts
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Vendor")
	FName CurrencyType = TEXT("Gold");

	// Buyback multiplier when player sells to vendor. 0.5 = vendor pays 50% of BaseValue.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Vendor",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BuybackMultiplier = 0.5f;

	// Whether the vendor buys items from the player
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Vendor")
	bool bAcceptsSales = true;

	// Item types the vendor refuses to buy (e.g., quest items)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Vendor")
	TArray<EDCItemType> RestrictedItemTypes;

	// Default stock — copied into UDCVendorComponent instance at BuildStock time
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Vendor",
	          meta = (TitleProperty = "ItemHandle"))
	TArray<FDCEconomyEntry> DefaultStock;
};
