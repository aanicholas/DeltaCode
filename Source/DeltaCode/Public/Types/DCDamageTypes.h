/*
 * DeltaCode - Unreal Engine Code Helper
 * Copyright (c) 2026 Andrew Nicholas
 *
 * License: GPL v3 or later. See repository LICENSE.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayEffect.h"
#include "DCDamageTypes.generated.h"

/**
 * Severity tiers DeltaCode systems use to communicate damage intent.
 * Tiers are SEVERITY buckets, not source labels — source kind is encoded
 * separately by the GE class and the SourceTags container passed alongside.
 *
 * Lethal is a sentinel — handled by a dedicated GE (FDCDamageConfig::
 * LethalDamageEffectClass) that applies the Damage.Lethal tag, rather
 * than relying on health clamping.
 */
UENUM(BlueprintType)
enum class EDCDamageTier : uint8
{
	None    UMETA(DisplayName = "None — no damage"),
	Melee   UMETA(DisplayName = "Melee — 5 HP, scuff/jab"),
	Light   UMETA(DisplayName = "Light — 20 HP, pistol round"),
	Medium  UMETA(DisplayName = "Medium — 35 HP, rifle round"),
	Heavy   UMETA(DisplayName = "Heavy — 75 HP, shotgun/grenade"),
	Ultra   UMETA(DisplayName = "Ultra — 150 HP, boss special"),
	Lethal  UMETA(DisplayName = "Lethal — instant kill regardless of HP")
};

/**
 * Tunable damage configuration. Lives on systems that emit damage —
 * weapons, hazards, enemy abilities — so each can override per-tier
 * magnitude or the GE template without changing the contract.
 *
 * Defaults align with Lyra's GE_Damage_Basic_SetByCaller / Data.Damage
 * convention so DeltaCode-emitted damage rides Lyra's existing damage
 * pipeline (cues, attribute clamping, immunity tags) when an ASC is
 * present.
 */
USTRUCT(BlueprintType)
struct DELTACODE_API FDCDamageConfig
{
	GENERATED_BODY()

	FDCDamageConfig();

	/** Magnitude (HP) per tier. Lethal entry is a sentinel — not used by
	 *  the Lethal route, which fires LethalDamageEffectClass directly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Damage")
	TMap<EDCDamageTier, float> TierMagnitudes;

	/** GE applied for non-Lethal tiers. Should accept SetByCaller
	 *  magnitude keyed by SetByCallerMagnitudeTag — Lyra's
	 *  GE_Damage_Basic_SetByCaller is the intended default reference. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Damage")
	TSoftClassPtr<UGameplayEffect> DefaultDamageEffectClass;

	/** GE applied for the Lethal tier. Carries the Damage.Lethal tag so
	 *  cues / listeners distinguish executions from clamped-to-zero hits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Damage")
	TSoftClassPtr<UGameplayEffect> LethalDamageEffectClass;

	/** SetByCaller tag the default GE reads its magnitude from.
	 *  Defaults to Data.Damage — matches Lyra's convention. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Damage")
	FGameplayTag SetByCallerMagnitudeTag;

	/** Returns the float magnitude for a tier. None → 0. */
	float ResolveMagnitude(EDCDamageTier Tier) const;
};
