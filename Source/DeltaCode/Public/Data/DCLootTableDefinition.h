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
#include "Types/DCCoreTypes.h" // FDCItemHandle, FDCInventoryEntry
#include "DCLootTableDefinition.generated.h"

/**
 * A single guaranteed-or-chance entry. Each entry in a loot table's guaranteed
 * list is evaluated independently: roll Chance, if it passes spawn a quantity
 * picked uniformly from [QuantityMin, QuantityMax].
 *
 * Set Chance=1.0 for always-drops. Set QuantityMin==QuantityMax for fixed counts.
 */
USTRUCT(BlueprintType)
struct DELTACODE_API FDCLootEntry
{
	GENERATED_BODY()

	/** Item to spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Loot")
	FDCItemHandle ItemHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Loot",
	          meta = (ClampMin = "1"))
	int32 QuantityMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Loot",
	          meta = (ClampMin = "1"))
	int32 QuantityMax = 1;

	/** 0..1. Rolled once per drop attempt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Loot",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Chance = 1.0f;
};

/**
 * A weighted-pool entry. The owning table performs a fixed number of picks
 * across the pool; each pick lands on an entry with probability Weight / TotalWeight.
 * Weight <= 0 entries are skipped.
 */
USTRUCT(BlueprintType)
struct DELTACODE_API FDCWeightedLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Loot")
	FDCItemHandle ItemHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Loot",
	          meta = (ClampMin = "1"))
	int32 QuantityMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Loot",
	          meta = (ClampMin = "1"))
	int32 QuantityMax = 1;

	/** Relative weight. Higher = more likely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Loot",
	          meta = (ClampMin = "0.0"))
	float Weight = 1.0f;
};

/**
 * Shared, reusable loot table. Authored as DA_DC_LootTable_<Name> under
 * Content/DeltaCode/Data/Loot/. Referenced by multiple enemy archetypes,
 * chests, quest rewards, etc. so drops can be balanced in one place.
 *
 * Evaluation order inside RollDrops():
 *   1. Guaranteed list — each entry rolled independently by its Chance.
 *   2. Weighted pool   — NumPicks = RandRange(MinRolls, MaxRolls) picks,
 *                        each selected by weight.
 *
 * Output is a list of FDCInventoryEntry pairs so callers can feed them
 * straight into UDCInventoryComponent or spawn pickups from them.
 *
 * [INPUT]  From: ADCEnemyBase (SharedLootTable), chests, quest reward hooks
 * [OUTPUT] To:   ADCPickupBase::InitializeFromHandle / UDCInventoryComponent::AddItem
 *
 * [DEPENDS ON] FDCItemHandle (DCCoreTypes)
 */
UCLASS(BlueprintType)
class DELTACODE_API UDCLootTableDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// ── Guaranteed / chance-based entries ───────────────────────────────────

	/** Entries evaluated one-by-one. Use Chance=1.0 for always-drops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Loot")
	TArray<FDCLootEntry> GuaranteedDrops;

	// ── Weighted pool ───────────────────────────────────────────────────────

	/** Pool from which NumPicks weighted selections are taken. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Loot")
	TArray<FDCWeightedLootEntry> WeightedPool;

	/** Minimum number of picks from the weighted pool. Clamped to >= 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Loot",
	          meta = (ClampMin = "0"))
	int32 MinRolls = 0;

	/** Maximum number of picks from the weighted pool. Clamped to >= MinRolls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Loot",
	          meta = (ClampMin = "0"))
	int32 MaxRolls = 0;

	/**
	 * When false, an entry that gets picked is removed from subsequent picks in
	 * the same RollDrops call. Useful for "pick 3 distinct rewards" tables.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Loot")
	bool bAllowDuplicateWeightedPicks = true;

	// ── Rolling ─────────────────────────────────────────────────────────────

	/**
	 * Roll this table and append the resulting drops to OutDrops.
	 * Does not clear OutDrops — callers may compose multiple tables.
	 *
	 * @param OutDrops   Appended list of (ItemHandle, Quantity) pairs.
	 */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Loot")
	void RollDrops(TArray<FDCInventoryEntry>& OutDrops) const;

	/** Convenience — returns a fresh array. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Loot")
	TArray<FDCInventoryEntry> Roll() const;
};
