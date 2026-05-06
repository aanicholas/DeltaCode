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
#include "Components/ActorComponent.h"
#include "DCHealthComponent.generated.h"

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FDCOnHealthChanged,
	float, NewHealth, float, OldHealth, float, Delta, AActor*, Instigator);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCOnDied,
	AActor*, KilledBy, AActor*, DamageCauser);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDCOnRevived);

/**
 * Damage / health bookkeeping for anything that implements IDCDamageable.
 * Lives on ADCCharacterBase (player + enemies) and destructibles.
 *
 * [INPUT]  From: IDCDamageable::ApplyDamageTier_Implementation → ApplyDamage()
 * [INPUT]  From: UDCItem consumables (heal potion) via Heal()
 * [OUTPUT] To:   UI health bar widgets via OnHealthChanged
 * [OUTPUT] To:   ADCEnemyBase::HandleDeath via OnDied
 * [OUTPUT] To:   Quest subsystem via OnDied (kill objectives)
 *
 * [DEPENDS ON] FDCStatBlock (Layer 1) — provides default MaxHealth / MaxShield
 */
UCLASS(ClassGroup = (DeltaCode), meta = (BlueprintSpawnableComponent))
class DELTACODE_API UDCHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCHealthComponent();

	// ── Configuration ───────────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Health",
	          meta = (ClampMin = "1.0"))
	float DefaultMaxHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Health",
	          meta = (ClampMin = "0.0"))
	float DefaultMaxShield = 0.0f;

	/**
	 * When true, shield depletes before health and regenerates after a delay.
	 * When false, shield is ignored entirely.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Health")
	bool bShieldEnabled = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Health",
	          meta = (ClampMin = "0.0",
	                  EditCondition = "bShieldEnabled"))
	float ShieldRegenRate = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Health",
	          meta = (ClampMin = "0.0",
	                  EditCondition = "bShieldEnabled"))
	float ShieldRegenDelay = 3.0f;

	// ── Runtime API ─────────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Health")
	void ApplyDamage(float Amount, AActor* DamageSource, AActor* DamageCauser);

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Health")
	void Heal(float Amount, AActor* Instigator = nullptr);

	/** Sets current health to 0 and fires OnDied. Used by scripted kill triggers. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Health")
	void Kill(AActor* KilledBy = nullptr, AActor* DamageCauser = nullptr);

	/** Restores full health + shield and clears the dead flag. Fires OnRevived. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Health")
	void Revive();

	/**
	 * Raise (or lower) the runtime MaxHealth by Delta. If bRefill is true, current
	 * health is bumped by the same amount (clamped to the new max). Fires
	 * OnHealthChanged so UI bars resize. Used by UDCProgressionComponent on level-up.
	 */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Health")
	void AdjustMaxHealth(float Delta, bool bRefill = true);

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Health")
	float GetHealthPercent() const
	{
		return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
	}

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Health")
	float GetCurrentShield() const { return CurrentShield; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Health")
	float GetMaxShield() const { return MaxShield; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Health")
	bool IsDead() const { return bIsDead; }

	// ── Delegates ───────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Health")
	FDCOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Health")
	FDCOnDied OnDied;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Health")
	FDCOnRevived OnRevived;

protected:
	virtual void BeginPlay() override;

private:
	// Current runtime values — initialized from Default* and the owner's FDCStatBlock
	UPROPERTY(Transient, BlueprintReadOnly, Category = "DeltaCode|Health",
	          meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "DeltaCode|Health",
	          meta = (AllowPrivateAccess = "true"))
	float MaxHealth = 100.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "DeltaCode|Health",
	          meta = (AllowPrivateAccess = "true"))
	float CurrentShield = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "DeltaCode|Health",
	          meta = (AllowPrivateAccess = "true"))
	float MaxShield = 0.0f;

	bool bIsDead = false;

	// Shield regen scheduling
	FTimerHandle ShieldRegenKickoffTimer;
	FTimerHandle ShieldRegenTickTimer;

	void StartShieldRegenCooldown();
	void TickShieldRegen();
	void StopShieldRegen();
};
