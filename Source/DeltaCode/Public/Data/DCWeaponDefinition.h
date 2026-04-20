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
#include "Data/DCEquipmentDefinition.h"
#include "DCWeaponDefinition.generated.h"

class ADCWeaponBase;
class USkeletalMesh;
class UAnimMontage;

UENUM(BlueprintType)
enum class EDCWeaponFireMode : uint8
{
	SemiAuto   UMETA(DisplayName = "Semi-Automatic"),
	FullAuto   UMETA(DisplayName = "Full-Automatic"),
	Burst      UMETA(DisplayName = "Burst"),
	Charged    UMETA(DisplayName = "Charged"),
};

UENUM(BlueprintType)
enum class EDCWeaponHitMode : uint8
{
	Hitscan    UMETA(DisplayName = "Hitscan (Instant)"),
	Projectile UMETA(DisplayName = "Projectile"),
	Melee      UMETA(DisplayName = "Melee"),
};

/**
 * Weapon-specific equipment definition.
 * Inherits identity/stack/tags from UDCItemDefinition, slot/modifiers/durability
 * from UDCEquipmentDefinition, and adds combat values consumed by ADCWeaponBase.
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API UDCWeaponDefinition : public UDCEquipmentDefinition
{
	GENERATED_BODY()

public:
	UDCWeaponDefinition();

	// ── Combat stats ────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Weapon|Combat",
	          meta = (ClampMin = "0.0"))
	float Damage = 10.0f;

	// Rounds per second
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Weapon|Combat",
	          meta = (ClampMin = "0.1"))
	float FireRate = 5.0f;

	// Effective range (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Weapon|Combat",
	          meta = (ClampMin = "1.0"))
	float Range = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Weapon|Combat")
	EDCWeaponFireMode FireMode = EDCWeaponFireMode::SemiAuto;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Weapon|Combat")
	EDCWeaponHitMode HitMode = EDCWeaponHitMode::Hitscan;

	// ── Ammo ────────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Weapon|Ammo",
	          meta = (ClampMin = "1"))
	int32 MagazineSize = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Weapon|Ammo",
	          meta = (ClampMin = "0.0"))
	float ReloadTime = 2.0f;

	// Ammo item this weapon consumes on fire. If None, weapon does not consume ammo.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Weapon|Ammo")
	FDCItemHandle AmmoType;

	// ── Spawn class & visuals ───────────────────────────────────────────────

	// Runtime weapon actor spawned when equipped (B_DC_Weapon_[Name])
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Weapon|Visual")
	TSoftClassPtr<ADCWeaponBase> WeaponActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Weapon|Visual")
	TSoftObjectPtr<USkeletalMesh> WeaponMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Weapon|Visual")
	TSoftObjectPtr<UAnimMontage> FireMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Weapon|Visual")
	TSoftObjectPtr<UAnimMontage> ReloadMontage;
};
