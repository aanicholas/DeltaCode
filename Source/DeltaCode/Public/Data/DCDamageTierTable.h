/*
 * DeltaCode - Unreal Engine Code Helper
 * Copyright (c) 2026 Andrew Nicholas
 *
 * License: GPL v3 or later. See repository LICENSE.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/DCDamageTypes.h"
#include "DCDamageTierTable.generated.h"

/**
 * Project-default damage configuration shipped as a UPrimaryDataAsset.
 * Wraps FDCDamageConfig so designers can edit the per-tier magnitude
 * map and the GE class refs in one canonical place (DA_DC_DamageTiers).
 *
 * Created by dc_setup_lyra.py on Lyra installs; can be referenced as a
 * fallback by any system that emits damage but has no per-instance
 * FDCDamageConfig of its own.
 */
UCLASS(BlueprintType)
class DELTACODE_API UDCDamageTierTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Damage")
	FDCDamageConfig DamageConfig;

	/** Convenience accessor — same as reading DamageConfig directly, but
	 *  pure so it shows up cleanly in Blueprint graphs. */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Damage")
	const FDCDamageConfig& GetConfig() const { return DamageConfig; }
};
