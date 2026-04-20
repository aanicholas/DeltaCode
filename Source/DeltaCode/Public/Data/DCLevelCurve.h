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
#include "Types/DCCoreTypes.h"
#include "DCLevelCurve.generated.h"

/**
 * Per-level bonus applied when a character levels up.
 * Stacks additively onto the attribute set and the owning character's FDCStatBlock.
 */
USTRUCT(BlueprintType)
struct DELTACODE_API FDCLevelUpBonus
{
	GENERATED_BODY()

	// Target level this bonus fires at (e.g. 2, 3, 4 ...). Level 1 is the starting level.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Progression",
	          meta = (ClampMin = "2"))
	int32 Level = 2;

	// XP required to reach this level (absolute total, not delta)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Progression",
	          meta = (ClampMin = "0"))
	int32 XPThreshold = 100;

	// Additive bonuses applied to the character's live stats on level-up
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Progression")
	float HealthBonus = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Progression")
	float StaminaBonus = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Progression")
	float AttackPowerBonus = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Progression")
	float DefenseBonus = 0.5f;

	// Unspent skill/attribute points awarded for this level
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Progression",
	          meta = (ClampMin = "0"))
	int32 SkillPointsAwarded = 1;
};

/**
 * XP thresholds and per-level bonuses for DeltaCode progression.
 * Created as DA_DC_LevelCurve_[Name] in Content/DeltaCode/Data/Progression/.
 *
 * [INPUT]  From: UDCProgressionComponent — reads XP thresholds on GrantXP()
 * [OUTPUT] To:   UDCProgressionComponent — emits bonuses on level-up
 *
 * The table must be sorted by Level ascending. Level 1 requires 0 XP implicitly;
 * the first entry should be Level 2 with its threshold and bonuses.
 */
UCLASS(BlueprintType)
class DELTACODE_API UDCLevelCurve : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Sorted ascending by Level. Max level = 1 + Levels.Num().
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Progression",
	          meta = (TitleProperty = "Level"))
	TArray<FDCLevelUpBonus> Levels;

	/** Returns XP required to reach the given level (absolute), or INT32_MAX if unreachable. */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Progression")
	int32 GetXPThresholdForLevel(int32 TargetLevel) const;

	/** Writes the bonus entry for a given level. Returns false if out of range. */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Progression")
	bool GetBonusForLevel(int32 TargetLevel, FDCLevelUpBonus& OutBonus) const;

	/** Highest level defined in the table (1 if the table is empty). */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Progression")
	int32 GetMaxLevel() const;
};
