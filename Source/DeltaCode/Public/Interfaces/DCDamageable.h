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
#include "UObject/Interface.h"
#include "DCDamageable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UDCDamageable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for any actor that can receive damage.
 * Implemented by enemies, destructibles, and the player character.
 */
class DELTACODE_API IDCDamageable
{
	GENERATED_BODY()

public:
	// [INPUT]  From: UDCWeaponComponent::OnFirePressed() → hitscan or projectile hit
	// [INPUT]  From: ADCEnemyBase → melee attack on player
	// [OUTPUT] To: UDCHealthComponent::ApplyDamage() on the implementing actor
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DeltaCode|Damage")
	void ApplyDamage(float DamageAmount, AActor* DamageSource, AActor* DamageCauser);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DeltaCode|Damage")
	bool IsDead() const;
};
