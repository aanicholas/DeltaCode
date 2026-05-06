/*
 * DeltaCode - Unreal Engine Code Helper
 * Copyright (c) 2026 Andrew Nicholas
 *
 * License: GPL v3 or later. See repository LICENSE.
 */

#include "Types/DCDamageTypes.h"

FDCDamageConfig::FDCDamageConfig()
{
	TierMagnitudes.Add(EDCDamageTier::None,   0.0f);
	TierMagnitudes.Add(EDCDamageTier::Melee,  5.0f);
	TierMagnitudes.Add(EDCDamageTier::Light,  20.0f);
	TierMagnitudes.Add(EDCDamageTier::Medium, 35.0f);
	TierMagnitudes.Add(EDCDamageTier::Heavy,  75.0f);
	TierMagnitudes.Add(EDCDamageTier::Ultra,  150.0f);
	// Sentinel — actual kill is delivered by LethalDamageEffectClass,
	// not by clamping. Kept in the map so ResolveMagnitude is total.
	TierMagnitudes.Add(EDCDamageTier::Lethal, 999999.0f);

	// Lyra's GE_Damage_Basic_SetByCaller convention. Resolves to the
	// empty tag in non-Lyra projects; the SetByCaller call is then
	// skipped silently by ApplyTieredDamageViaASC.
	SetByCallerMagnitudeTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Data.Damage")), /*ErrorIfNotFound=*/false);
}

float FDCDamageConfig::ResolveMagnitude(EDCDamageTier Tier) const
{
	if (const float* Found = TierMagnitudes.Find(Tier))
	{
		return *Found;
	}
	return 0.0f;
}
