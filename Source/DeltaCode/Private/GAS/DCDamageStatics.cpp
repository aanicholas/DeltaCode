/*
 * DeltaCode - Unreal Engine Code Helper
 * Copyright (c) 2026 Andrew Nicholas
 *
 * License: GPL v3 or later. See repository LICENSE.
 */

#include "GAS/DCDamageStatics.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Types/DCDamageable.h"

float UDCDamageStatics::ApplyTieredDamage(AActor* Target,
                                          EDCDamageTier Tier,
                                          const FGameplayTagContainer& SourceTags,
                                          AActor* Instigator,
                                          AActor* Causer,
                                          const FDCDamageConfig& Config)
{
	if (!IsValid(Target) || Tier == EDCDamageTier::None)
	{
		return 0.0f;
	}

	// Path 1: actor implements IDCDamageable. Cheap veto first.
	if (Target->Implements<UDCDamageable>())
	{
		if (!IDCDamageable::Execute_CanReceiveDamage(Target))
		{
			return 0.0f;
		}
		return IDCDamageable::Execute_ApplyDamageTier(
			Target, Tier, SourceTags, Instigator, Causer);
	}

	// Path 2: actor exposes UAbilitySystemComponent — apply directly.
	if (UAbilitySystemComponent* ASC =
	    UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
	{
		return ApplyTieredDamageViaASC(ASC, Tier, SourceTags, Instigator, Causer, Config);
	}

	// No path to deliver damage.
	return 0.0f;
}

float UDCDamageStatics::ApplyDefaultTieredDamage(AActor* Target,
                                                 EDCDamageTier Tier,
                                                 const FGameplayTagContainer& SourceTags,
                                                 AActor* Instigator,
                                                 AActor* Causer)
{
	const FDCDamageConfig DefaultConfig;
	return ApplyTieredDamage(Target, Tier, SourceTags, Instigator, Causer, DefaultConfig);
}

float UDCDamageStatics::ApplyTieredDamageViaASC(UAbilitySystemComponent* TargetASC,
                                                EDCDamageTier Tier,
                                                const FGameplayTagContainer& SourceTags,
                                                AActor* Instigator,
                                                AActor* Causer,
                                                const FDCDamageConfig& Config)
{
	if (!IsValid(TargetASC) || Tier == EDCDamageTier::None)
	{
		return 0.0f;
	}

	const TSubclassOf<UGameplayEffect> EffectClass = ResolveDamageEffectClass(Tier, Config);
	if (!EffectClass)
	{
		return 0.0f;
	}

	FGameplayEffectContextHandle Ctx = TargetASC->MakeEffectContext();
	Ctx.AddInstigator(Instigator, Causer);

	FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(
		EffectClass, /*Level=*/1.0f, Ctx);
	if (!SpecHandle.IsValid())
	{
		return 0.0f;
	}

	// Dynamic asset tags so cues / listeners can discriminate by source.
	SpecHandle.Data->DynamicAssetTags.AppendTags(SourceTags);

	const float Magnitude = Config.ResolveMagnitude(Tier);
	if (Tier != EDCDamageTier::Lethal && Config.SetByCallerMagnitudeTag.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(
			Config.SetByCallerMagnitudeTag, Magnitude);
	}

	TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
	return Magnitude;
}

TSubclassOf<UGameplayEffect>
UDCDamageStatics::ResolveDamageEffectClass(EDCDamageTier Tier,
                                           const FDCDamageConfig& Config)
{
	const TSoftClassPtr<UGameplayEffect>& Soft =
		(Tier == EDCDamageTier::Lethal)
			? Config.LethalDamageEffectClass
			: Config.DefaultDamageEffectClass;

	return Soft.IsNull() ? nullptr : Soft.LoadSynchronous();
}
