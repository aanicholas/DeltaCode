/*
 * DeltaCode - Unreal Engine Code Helper
 * Copyright (c) 2026 Andrew Nicholas
 *
 * License: GPL v3 or later. See repository LICENSE.
 */
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/DCDamageTypes.h"
#include "DCDamageable.generated.h"

UINTERFACE(MinimalAPI, BlueprintType,
           meta = (CannotImplementInterfaceInBlueprint))
class UDCDamageable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Universal damage contract. Implemented by anything DeltaCode-generated
 * that can take a hit — characters, destructibles, hazardous environments.
 *
 * Implementations choose how to handle the tier:
 *   - GAS-aware actors forward via UDCDamageStatics (SetByCaller GE for
 *     non-Lethal; dedicated Lethal GE for Lethal).
 *   - Non-GAS actors resolve magnitude via FDCDamageConfig and forward
 *     to UDCHealthComponent::ApplyDamage.
 *
 * [INPUT]  From: ADCWeaponBase via UDCDamageStatics::ApplyTieredDamage
 * [INPUT]  From: ADCEnemyBase melee abilities (GA_*)
 * [INPUT]  From: Hazard volumes / triggers
 * [OUTPUT] To:   UAbilitySystemComponent (GAS route) or UDCHealthComponent
 *                (HP route) on the implementing actor
 */
class DELTACODE_API IDCDamageable
{
	GENERATED_BODY()

public:
	/** Apply tier-scaled damage. SourceTags carry the kind
	 *  (Damage.Source.Bullet, Damage.Source.Hazard.Lava, …). Returns the
	 *  actual HP delta applied — 0 means blocked, immune, or already dead. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DeltaCode|Damage")
	float ApplyDamageTier(EDCDamageTier Tier,
	                      const FGameplayTagContainer& SourceTags,
	                      AActor* Instigator,
	                      AActor* Causer);

	/** Cheap query — used by AI perception and pickup interaction to skip
	 *  dead or invulnerable targets without firing a full apply. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DeltaCode|Damage")
	bool CanReceiveDamage() const;
};
