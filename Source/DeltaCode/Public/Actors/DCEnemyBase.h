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
#include "Actors/DCCharacterBase.h"
#include "Types/DCCoreTypes.h"
#include "DCEnemyBase.generated.h"

class UBehaviorTree;
class UDCHealthComponent;
class UDCLootTableDefinition;

USTRUCT(BlueprintType)
struct DELTACODE_API FDCLootDrop
{
	GENERATED_BODY()

	// Item to spawn on death
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Loot")
	FDCItemHandle ItemHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Loot",
	          meta = (ClampMin = "1"))
	int32 Quantity = 1;

	// 0–1 probability of rolling this entry
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Loot",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Chance = 1.0f;
};

/**
 * Base enemy character. Inherits player-adjacent features (GAS, damage,
 * equipment) from ADCCharacterBase and layers on enemy-specific behaviour:
 * AI controller assignment, loot drops on death, hit-react / death hooks.
 *
 * [INPUT]  From: ADCWeaponBase::PerformHitscan → IDCDamageable::ApplyDamage
 *              → UDCHealthComponent → HandleDeath
 * [OUTPUT] To:   ADCPickupBase spawns on death (loot table roll)
 * [OUTPUT] To:   ADCEnemyAIController — possesses on spawn
 *
 * [DEPENDS ON] UDCHealthComponent (inherited via ADCCharacterBase)
 * [DEPENDS ON] ADCEnemyAIController
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API ADCEnemyBase : public ADCCharacterBase
{
	GENERATED_BODY()

public:
	ADCEnemyBase();

	// Inline per-enemy loot. Each entry is evaluated independently by Chance.
	// Use for cheap one-off drops unique to this enemy; use SharedLootTable for
	// pools balanced across many enemy archetypes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Enemy|Loot")
	TArray<FDCLootDrop> LootTable;

	// Shared, reusable loot table asset (DA_DC_LootTable_*). Rolled in addition
	// to the inline LootTable on death — both sources may contribute. Soft-ref
	// so the archetype doesn't hard-load the table until it actually dies.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Enemy|Loot")
	TSoftObjectPtr<UDCLootTableDefinition> SharedLootTable;

	// Behaviour tree run by ADCEnemyAIController on possession. The tree must
	// reference a BlackboardAsset containing the keys declared in
	// AI/DCAIBlackboardKeys.h (TargetActor, LastKnownLocation, HomeLocation,
	// bIsInvestigating). Leave null to opt out of BT-driven AI entirely.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Enemy|AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	// Seconds to keep the corpse around before Destroy(). <= 0 = never destroy.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DeltaCode|Enemy")
	float CorpseLifetime = 8.0f;

	// Pickup class used to spawn loot drops (B_DC_Pickup_Generic by default)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DeltaCode|Enemy|Loot")
	TSubclassOf<class ADCPickupBase> LootPickupClass;

protected:
	virtual void BeginPlay() override;

	/** BP hook — play hit react montage / tint material on damage. */
	UFUNCTION(BlueprintImplementableEvent, Category = "DeltaCode|Enemy")
	void OnHitReact(float DamageAmount, AActor* DamageCauser);

	/** BP hook — override to swap in ragdoll / animation-driven death. */
	UFUNCTION(BlueprintImplementableEvent, Category = "DeltaCode|Enemy")
	void OnDeathFX(AActor* KilledBy);

private:
	// [EVENT] Subscriber: UDCHealthComponent::OnHealthChanged
	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float OldHealth, float Delta,
	                         AActor* DamageInstigator);

	// [EVENT] Subscriber: UDCHealthComponent::OnDied
	UFUNCTION()
	void HandleDeath(AActor* KilledBy, AActor* DamageCauser);

	void SpawnLootDrops();

	FTimerHandle CorpseTimer;
};
