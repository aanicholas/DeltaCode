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
#include "Data/DCItemDefinition.h"
#include "Types/DCCoreTypes.h"
#include "DCEquipmentDefinition.generated.h"

class USkeletalMesh;
class UStaticMesh;

/**
 * Base data asset for any *equippable* item (armor, accessory, weapon).
 * Carries slot, stat modifiers, durability, and optional attached-mesh visuals.
 *
 * UDCWeaponDefinition extends this to add combat values + spawn class.
 *
 * [INPUT]  From: UDCEquipmentComponent::EquipItem(Handle) — slot routing + modifiers
 * [OUTPUT] To:   UDCAttributeSet (AttackPower / Defense / MaxStamina) — applied / removed
 * [OUTPUT] To:   UDCHealthComponent::AdjustMaxHealth — MaxHealthBonus applied / removed
 *
 * Created as DA_DC_Equip_[Name] in Content/DeltaCode/Data/Items/Equipment/.
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API UDCEquipmentDefinition : public UDCItemDefinition
{
	GENERATED_BODY()

public:
	UDCEquipmentDefinition();

	// ── Slotting ────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AssetRegistrySearchable,
	          Category = "DeltaCode|Equipment")
	EDCEquipmentSlot EquipmentSlot = EDCEquipmentSlot::None;

	/**
	 * Socket the equipped mesh attaches to. Defaults differ per slot — designers
	 * can override per-asset (e.g. shield to "hand_l", quiver to "spine_03").
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Equipment")
	FName AttachSocketOverride = NAME_None;

	// ── Stat modifiers ──────────────────────────────────────────────────────

	/** Additive bonuses applied to the wearer while this is equipped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Equipment")
	FDCStatModifier Modifiers;

	// ── Durability ──────────────────────────────────────────────────────────

	/** Max durability points. 0 = indestructible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Equipment|Durability",
	          meta = (ClampMin = "0"))
	int32 MaxDurability = 0;

	/**
	 * Durability lost per use (per shot for weapons, per damage instance for armor).
	 * Ignored if MaxDurability is 0.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Equipment|Durability",
	          meta = (ClampMin = "0"))
	int32 DurabilityPerUse = 1;

	// ── Visual ──────────────────────────────────────────────────────────────

	/** Optional attached mesh — armor pieces, accessories. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Equipment|Visual")
	TSoftObjectPtr<USkeletalMesh> EquippedSkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Equipment|Visual")
	TSoftObjectPtr<UStaticMesh> EquippedStaticMesh;
};
