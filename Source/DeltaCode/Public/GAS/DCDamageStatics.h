/*
 * DeltaCode - Unreal Engine Code Helper
 * Copyright (c) 2026 Andrew Nicholas
 *
 * License: GPL v3 or later. See repository LICENSE.
 */
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Types/DCDamageTypes.h"
#include "DCDamageStatics.generated.h"

class UAbilitySystemComponent;

/**
 * Stateless helpers for DeltaCode's tier-based damage. Centralizes the
 * GAS-vs-HP routing so every weapon, ability, and hazard fires damage
 * the same way.
 *
 * Use ApplyTieredDamage() as the default entry point — it tries
 * IDCDamageable on the target first (lets implementations veto via
 * CanReceiveDamage), then falls back to direct ASC application if the
 * actor exposes an ASC but doesn't implement the interface.
 */
UCLASS()
class DELTACODE_API UDCDamageStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Universal entry point. Resolution order:
	 *   1. Target implements IDCDamageable AND CanReceiveDamage() → call
	 *      Execute_ApplyDamageTier.
	 *   2. Else Target has a UAbilitySystemComponent → ApplyTieredDamageViaASC.
	 *   3. Else → return 0.
	 *
	 * [OUTPUT] To: IDCDamageable::Execute_ApplyDamageTier on target
	 * [OUTPUT] To: UAbilitySystemComponent::ApplyGameplayEffectSpecToSelf
	 */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Damage",
	          meta = (DefaultToSelf = "Causer", AdvancedDisplay = "Causer"))
	static float ApplyTieredDamage(AActor* Target,
	                               EDCDamageTier Tier,
	                               const FGameplayTagContainer& SourceTags,
	                               AActor* Instigator,
	                               AActor* Causer,
	                               const FDCDamageConfig& Config);

	/**
	 * Convenience: ApplyTieredDamage with a default-constructed
	 * FDCDamageConfig. Use when a system doesn't carry its own config.
	 */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Damage",
	          meta = (DefaultToSelf = "Causer"))
	static float ApplyDefaultTieredDamage(AActor* Target,
	                                      EDCDamageTier Tier,
	                                      const FGameplayTagContainer& SourceTags,
	                                      AActor* Instigator,
	                                      AActor* Causer);

	/**
	 * GAS route. Builds a GE spec from Config (default GE for non-Lethal,
	 * Lethal GE for Lethal), sets the SetByCaller magnitude, attaches
	 * SourceTags as dynamic asset tags, and applies to TargetASC.
	 */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Damage")
	static float ApplyTieredDamageViaASC(UAbilitySystemComponent* TargetASC,
	                                     EDCDamageTier Tier,
	                                     const FGameplayTagContainer& SourceTags,
	                                     AActor* Instigator,
	                                     AActor* Causer,
	                                     const FDCDamageConfig& Config);

	/** Resolve which GE class fires for a tier (Lethal vs default). */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Damage")
	static TSubclassOf<UGameplayEffect> ResolveDamageEffectClass(
		EDCDamageTier Tier,
		const FDCDamageConfig& Config);
};
