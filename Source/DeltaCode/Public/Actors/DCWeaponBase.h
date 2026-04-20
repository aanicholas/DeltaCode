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
#include "GameFramework/Actor.h"
#include "Types/DCCoreTypes.h"
#include "DCWeaponBase.generated.h"

class USkeletalMeshComponent;
class UDCWeaponDefinition;
class ADCCharacterBase;
class UAnimMontage;

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnWeaponFired, AActor*, HitActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDCOnWeaponReloadStart);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDCOnWeaponReloadComplete);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCOnAmmoChanged,
	int32, CurrentMagazine, int32, ReserveAmmo);

/**
 * Runtime weapon actor. Spawned and attached by UDCEquipmentComponent when a
 * weapon is equipped; destroyed when unequipped. Owns current magazine, cooldown
 * state, and delegates; combat math comes from the owning UDCWeaponDefinition.
 *
 * [INPUT]  From: UDCEquipmentComponent::EquipWeapon() — Initialize()
 * [INPUT]  From: UDCEquipmentComponent::FireEquipped() / ReloadEquipped()
 * [OUTPUT] To:   IDCDamageable::ApplyDamage() on hit actor
 * [OUTPUT] To:   UDCInventoryComponent — decrements reserve ammo on reload
 *
 * [DEPENDS ON] UDCWeaponDefinition — combat values, fire/hit mode, ammo item
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API ADCWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	ADCWeaponBase();

	/** One-shot configuration from the equipment component. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Weapon")
	void Initialize(UDCWeaponDefinition* InDefinition, ADCCharacterBase* InOwnerCharacter);

	/** Attempt to fire. Returns true if a shot was actually discharged. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Weapon")
	bool TryFire();

	/** Begin a reload. Fails if already reloading, mag is full, or no reserve ammo. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Weapon")
	bool StartReload();

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Weapon")
	int32 GetCurrentMagazine() const { return CurrentMagazine; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Weapon")
	int32 GetReserveAmmo() const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Weapon")
	UDCWeaponDefinition* GetDefinition() const { return Definition; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Weapon")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Weapon")
	bool IsOnCooldown() const { return bIsOnCooldown; }

	// ── Muzzle / socket config ──────────────────────────────────────────────

	// Socket name on the weapon mesh where projectiles / tracers originate
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Weapon")
	FName MuzzleSocketName = TEXT("Muzzle");

	// ── Delegates ───────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Weapon")
	FDCOnWeaponFired OnFired;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Weapon")
	FDCOnWeaponReloadStart OnReloadStart;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Weapon")
	FDCOnWeaponReloadComplete OnReloadComplete;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Weapon")
	FDCOnAmmoChanged OnAmmoChanged;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	// Cached definition — set by Initialize()
	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Weapon")
	TObjectPtr<UDCWeaponDefinition> Definition;

	// Character that equipped us — source of trace origin and montage anim instance
	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Weapon")
	TWeakObjectPtr<ADCCharacterBase> OwnerCharacter;

	/** BP hook — spawn tracer / muzzle flash / sfx when a shot fires. */
	UFUNCTION(BlueprintImplementableEvent, Category = "DeltaCode|Weapon")
	void OnFireFX(const FVector& MuzzleLocation, const FVector& HitLocation, AActor* HitActor);

private:
	void PerformHitscan();
	void PerformProjectile();
	void PerformMelee();

	void EndFireCooldown();
	void FinishReload();

	// Play a montage on the owner character's mesh
	float PlayOwnerMontage(UAnimMontage* Montage) const;

	// Resolve muzzle world transform (fallback: actor location + forward)
	void GetMuzzleTransform(FVector& OutLocation, FVector& OutForward) const;

	int32 CurrentMagazine = 0;
	bool bIsOnCooldown = false;
	bool bIsReloading = false;

	FTimerHandle FireCooldownTimer;
	FTimerHandle ReloadTimer;
};
