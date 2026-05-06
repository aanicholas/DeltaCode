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
#include "Actors/DCWeaponBase.h"
#include "Actors/DCCharacterBase.h"
#include "Actors/DCPlayerControllerBase.h"
#include "Components/DCInventoryComponent.h"
#include "Components/DCEquipmentComponent.h"
#include "Data/DCWeaponDefinition.h"
#include "GAS/DCDamageStatics.h"
#include "Types/DCDamageable.h"
#include "Types/DCDamageTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"

ADCWeaponBase::ADCWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADCWeaponBase::Initialize(UDCWeaponDefinition* InDefinition,
                               ADCCharacterBase* InOwnerCharacter)
{
	Definition = InDefinition;
	OwnerCharacter = InOwnerCharacter;

	if (Definition && !Definition->WeaponMesh.IsNull())
	{
		if (USkeletalMesh* Mesh = Definition->WeaponMesh.LoadSynchronous())
		{
			WeaponMesh->SetSkeletalMesh(Mesh);
		}
	}

	CurrentMagazine = Definition ? Definition->MagazineSize : 0;
	bIsOnCooldown = false;
	bIsReloading = false;

	OnAmmoChanged.Broadcast(CurrentMagazine, GetReserveAmmo());
}

int32 ADCWeaponBase::GetReserveAmmo() const
{
	if (!Definition || !Definition->AmmoType.IsValid() || !OwnerCharacter.IsValid())
	{
		return 0;
	}
	const ADCPlayerControllerBase* PC =
		Cast<ADCPlayerControllerBase>(OwnerCharacter->GetController());
	if (!PC)
	{
		return 0;
	}
	if (const UDCInventoryComponent* Inventory = PC->GetInventoryComponent())
	{
		return Inventory->GetItemCount(Definition->AmmoType);
	}
	return 0;
}

// ─── Fire ────────────────────────────────────────────────────────────────────

bool ADCWeaponBase::TryFire()
{
	if (!Definition || bIsReloading || bIsOnCooldown)
	{
		return false;
	}

	// Refuse to fire if the equipment slot reports the weapon is broken
	if (OwnerCharacter.IsValid())
	{
		if (UDCEquipmentComponent* Equip =
			OwnerCharacter->GetEquipmentComponent())
		{
			if (Equip->IsSlotBroken(EDCEquipmentSlot::Weapon))
			{
				return false;
			}
		}
	}

	if (Definition->MagazineSize > 0 && CurrentMagazine <= 0)
	{
		// Auto-reload convenience — designers can gate this behind a flag later
		StartReload();
		return false;
	}

	switch (Definition->HitMode)
	{
	case EDCWeaponHitMode::Hitscan:   PerformHitscan();   break;
	case EDCWeaponHitMode::Projectile: PerformProjectile(); break;
	case EDCWeaponHitMode::Melee:     PerformMelee();     break;
	}

	if (Definition->MagazineSize > 0)
	{
		--CurrentMagazine;
	}

	// Montage
	if (UAnimMontage* Fire = Definition->FireMontage.LoadSynchronous())
	{
		PlayOwnerMontage(Fire);
	}

	// Fire-rate cooldown
	bIsOnCooldown = true;
	const float Interval = 1.0f / FMath::Max(0.01f, Definition->FireRate);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FireCooldownTimer, this,
			&ADCWeaponBase::EndFireCooldown, Interval, false);
	}

	OnAmmoChanged.Broadcast(CurrentMagazine, GetReserveAmmo());

	// [OUTPUT] To: UDCEquipmentComponent — durability tick per shot
	if (Definition->MaxDurability > 0 && Definition->DurabilityPerUse > 0
	    && OwnerCharacter.IsValid())
	{
		if (UDCEquipmentComponent* Equip = OwnerCharacter->GetEquipmentComponent())
		{
			Equip->DamageDurability(EDCEquipmentSlot::Weapon, Definition->DurabilityPerUse);
		}
	}
	return true;
}

void ADCWeaponBase::EndFireCooldown()
{
	bIsOnCooldown = false;
}

void ADCWeaponBase::PerformHitscan()
{
	if (!OwnerCharacter.IsValid() || !Definition)
	{
		return;
	}

	FVector MuzzleLoc, Forward;
	GetMuzzleTransform(MuzzleLoc, Forward);

	// Prefer camera-forward for aim direction on player characters (FPS / TPS aim)
	FVector AimStart = MuzzleLoc;
	FVector AimDir = Forward;
	if (UCameraComponent* Cam = OwnerCharacter->FindComponentByClass<UCameraComponent>())
	{
		AimStart = Cam->GetComponentLocation();
		AimDir = Cam->GetForwardVector();
	}

	const FVector End = AimStart + AimDir * Definition->Range;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DCWeaponTrace), /*bTraceComplex*/ true);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(OwnerCharacter.Get());

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, AimStart, End,
	                                                       ECC_Visibility, Params);

	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
	const FVector ImpactLoc = bHit ? Hit.ImpactPoint : End;

	if (HitActor)
	{
		// TODO: drive Tier from UDCWeaponDefinition once a DamageTier
		// field is added there. Default Medium matches the legacy float
		// damage range used by current weapon definitions.
		UDCDamageStatics::ApplyDefaultTieredDamage(
			HitActor, EDCDamageTier::Medium, FGameplayTagContainer{},
			OwnerCharacter.Get(), this);
	}

	OnFireFX(MuzzleLoc, ImpactLoc, HitActor);
	OnFired.Broadcast(HitActor);
}

void ADCWeaponBase::PerformProjectile()
{
	// Projectile class lives on an expanded UDCWeaponDefinition in Layer 3A polish.
	// Stub: log so design knows the hit mode is recognized but not yet implemented.
	UE_LOG(LogTemp, Warning,
		TEXT("DCWeaponBase '%s': Projectile fire mode not yet implemented."),
		*GetName());

	FVector MuzzleLoc, Forward;
	GetMuzzleTransform(MuzzleLoc, Forward);
	OnFireFX(MuzzleLoc, MuzzleLoc + Forward * 500.0f, nullptr);
	OnFired.Broadcast(nullptr);
}

void ADCWeaponBase::PerformMelee()
{
	if (!OwnerCharacter.IsValid() || !Definition)
	{
		return;
	}

	FVector MuzzleLoc, Forward;
	GetMuzzleTransform(MuzzleLoc, Forward);

	const FVector Origin = OwnerCharacter->GetActorLocation()
	                     + OwnerCharacter->GetActorForwardVector() * 50.0f;
	const FVector End = Origin + OwnerCharacter->GetActorForwardVector() * Definition->Range;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DCMeleeSweep), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(OwnerCharacter.Get());

	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Origin, End,
		FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(60.0f), Params);

	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
	if (HitActor)
	{
		// TODO: drive Tier from UDCWeaponDefinition once a DamageTier
		// field is added there.
		UDCDamageStatics::ApplyDefaultTieredDamage(
			HitActor, EDCDamageTier::Medium, FGameplayTagContainer{},
			OwnerCharacter.Get(), this);
	}

	OnFireFX(MuzzleLoc, bHit ? Hit.ImpactPoint : End, HitActor);
	OnFired.Broadcast(HitActor);
}

// ─── Reload ──────────────────────────────────────────────────────────────────

bool ADCWeaponBase::StartReload()
{
	if (!Definition || bIsReloading)
	{
		return false;
	}
	if (Definition->MagazineSize <= 0 || CurrentMagazine >= Definition->MagazineSize)
	{
		return false;
	}
	if (Definition->AmmoType.IsValid() && GetReserveAmmo() <= 0)
	{
		return false;
	}

	bIsReloading = true;
	OnReloadStart.Broadcast();

	if (UAnimMontage* Montage = Definition->ReloadMontage.LoadSynchronous())
	{
		PlayOwnerMontage(Montage);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ReloadTimer, this,
			&ADCWeaponBase::FinishReload, FMath::Max(0.01f, Definition->ReloadTime), false);
	}
	return true;
}

void ADCWeaponBase::FinishReload()
{
	bIsReloading = false;
	if (!Definition)
	{
		return;
	}

	const int32 RoomInMag = Definition->MagazineSize - CurrentMagazine;
	int32 ToLoad = RoomInMag;

	// Consume from inventory reserve if this weapon uses ammo items
	if (Definition->AmmoType.IsValid() && OwnerCharacter.IsValid())
	{
		if (ADCPlayerControllerBase* PC =
			Cast<ADCPlayerControllerBase>(OwnerCharacter->GetController()))
		{
			if (UDCInventoryComponent* Inv = PC->GetInventoryComponent())
			{
				const int32 Reserve = Inv->GetItemCount(Definition->AmmoType);
				ToLoad = FMath::Min(ToLoad, Reserve);
				if (ToLoad > 0)
				{
					Inv->RemoveItem(Definition->AmmoType, ToLoad);
				}
			}
		}
	}

	CurrentMagazine += ToLoad;

	OnAmmoChanged.Broadcast(CurrentMagazine, GetReserveAmmo());
	OnReloadComplete.Broadcast();
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

void ADCWeaponBase::GetMuzzleTransform(FVector& OutLocation, FVector& OutForward) const
{
	if (WeaponMesh && WeaponMesh->DoesSocketExist(MuzzleSocketName))
	{
		const FTransform T = WeaponMesh->GetSocketTransform(MuzzleSocketName);
		OutLocation = T.GetLocation();
		OutForward = T.GetRotation().GetForwardVector();
		return;
	}
	OutLocation = GetActorLocation();
	OutForward = GetActorForwardVector();
}

float ADCWeaponBase::PlayOwnerMontage(UAnimMontage* Montage) const
{
	if (!Montage || !OwnerCharacter.IsValid())
	{
		return 0.0f;
	}
	if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
	{
		if (UAnimInstance* AnimInst = Mesh->GetAnimInstance())
		{
			return AnimInst->Montage_Play(Montage);
		}
	}
	return 0.0f;
}
