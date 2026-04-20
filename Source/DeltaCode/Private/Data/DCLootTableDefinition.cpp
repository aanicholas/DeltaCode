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
#include "Data/DCLootTableDefinition.h"

namespace DCLootTablePrivate
{
	/** Uniform int in [Min, Max] — clamped so QuantityMax < QuantityMin can't crash. */
	static int32 RollQuantity(int32 Min, int32 Max)
	{
		const int32 Lo = FMath::Max(1, Min);
		const int32 Hi = FMath::Max(Lo, Max);
		return FMath::RandRange(Lo, Hi);
	}

	/**
	 * Pick an index from the weighted pool using the sum of weights as the
	 * discriminator. SkipMask lets callers exclude already-picked indices
	 * without mutating the array (for bAllowDuplicateWeightedPicks=false).
	 *
	 * Returns INDEX_NONE when no entry has positive weight.
	 */
	static int32 PickWeightedIndex(const TArray<FDCWeightedLootEntry>& Pool,
	                               const TBitArray<>& SkipMask)
	{
		float TotalWeight = 0.0f;
		for (int32 i = 0; i < Pool.Num(); ++i)
		{
			if (SkipMask.IsValidIndex(i) && SkipMask[i]) { continue; }
			if (Pool[i].Weight > 0.0f && Pool[i].ItemHandle.IsValid())
			{
				TotalWeight += Pool[i].Weight;
			}
		}
		if (TotalWeight <= 0.0f)
		{
			return INDEX_NONE;
		}

		float Roll = FMath::FRandRange(0.0f, TotalWeight);
		for (int32 i = 0; i < Pool.Num(); ++i)
		{
			if (SkipMask.IsValidIndex(i) && SkipMask[i]) { continue; }
			if (!(Pool[i].Weight > 0.0f) || !Pool[i].ItemHandle.IsValid()) { continue; }
			Roll -= Pool[i].Weight;
			if (Roll <= 0.0f)
			{
				return i;
			}
		}
		// Float drift fallback — walk back to the last eligible entry.
		for (int32 i = Pool.Num() - 1; i >= 0; --i)
		{
			if (SkipMask.IsValidIndex(i) && SkipMask[i]) { continue; }
			if (Pool[i].Weight > 0.0f && Pool[i].ItemHandle.IsValid())
			{
				return i;
			}
		}
		return INDEX_NONE;
	}
}

FPrimaryAssetId UDCLootTableDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("DCLootTable"), GetFName());
}

void UDCLootTableDefinition::RollDrops(TArray<FDCInventoryEntry>& OutDrops) const
{
	using namespace DCLootTablePrivate;

	// ── Guaranteed lane ────────────────────────────────────────────────────
	for (const FDCLootEntry& Entry : GuaranteedDrops)
	{
		if (!Entry.ItemHandle.IsValid())
		{
			continue;
		}
		if (Entry.Chance < 1.0f && FMath::FRand() > Entry.Chance)
		{
			continue;
		}

		FDCInventoryEntry Drop;
		Drop.ItemHandle = Entry.ItemHandle;
		Drop.StackCount = RollQuantity(Entry.QuantityMin, Entry.QuantityMax);
		OutDrops.Add(Drop);
	}

	// ── Weighted lane ──────────────────────────────────────────────────────
	const int32 ClampedMin = FMath::Max(0, MinRolls);
	const int32 ClampedMax = FMath::Max(ClampedMin, MaxRolls);
	const int32 NumPicks = (ClampedMax > 0) ? FMath::RandRange(ClampedMin, ClampedMax) : 0;

	if (NumPicks <= 0 || WeightedPool.Num() == 0)
	{
		return;
	}

	// SkipMask tracks entries already consumed when duplicates are disallowed.
	TBitArray<> SkipMask(false, WeightedPool.Num());

	for (int32 Pick = 0; Pick < NumPicks; ++Pick)
	{
		const int32 Index = PickWeightedIndex(WeightedPool, SkipMask);
		if (Index == INDEX_NONE)
		{
			// Pool exhausted — stop early rather than spinning.
			break;
		}

		const FDCWeightedLootEntry& Entry = WeightedPool[Index];
		FDCInventoryEntry Drop;
		Drop.ItemHandle = Entry.ItemHandle;
		Drop.StackCount = RollQuantity(Entry.QuantityMin, Entry.QuantityMax);
		OutDrops.Add(Drop);

		if (!bAllowDuplicateWeightedPicks)
		{
			SkipMask[Index] = true;
		}
	}
}

TArray<FDCInventoryEntry> UDCLootTableDefinition::Roll() const
{
	TArray<FDCInventoryEntry> Drops;
	RollDrops(Drops);
	return Drops;
}
