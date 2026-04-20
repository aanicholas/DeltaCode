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
#include "GameplayTagContainer.h"
#include "DCCoreTypes.generated.h"

// ─── Camera Mode ─────────────────────────────────────────────────────────────
// Project-level default camera perspective. Consumed by UDeltaCodeSettings so
// the generator knows which ADCCharacterBase subclass to set as the project's
// default pawn, and by ADCDualPerspectiveCharacter to pick an initial view.

UENUM(BlueprintType)
enum class EDCCameraMode : uint8
{
	ThirdPerson     UMETA(DisplayName = "Third Person"),
	FirstPerson     UMETA(DisplayName = "First Person"),
	DualPerspective UMETA(DisplayName = "Dual Perspective (Runtime Toggle)"),
};

// ─── Item Types ──────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EDCItemType : uint8
{
	None        UMETA(DisplayName = "None"),
	Weapon      UMETA(DisplayName = "Weapon"),
	Armor       UMETA(DisplayName = "Armor"),
	Consumable  UMETA(DisplayName = "Consumable"),
	QuestItem   UMETA(DisplayName = "Quest Item"),
	Ammo        UMETA(DisplayName = "Ammo"),
	Currency    UMETA(DisplayName = "Currency"),
	Miscellaneous UMETA(DisplayName = "Miscellaneous"),
};

// ─── Equipment Slot ──────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EDCEquipmentSlot : uint8
{
	None        UMETA(DisplayName = "None"),
	Weapon      UMETA(DisplayName = "Weapon"),
	Helmet      UMETA(DisplayName = "Helmet"),
	Chest       UMETA(DisplayName = "Chest"),
	Legs        UMETA(DisplayName = "Legs"),
	Hands       UMETA(DisplayName = "Hands"),
	Feet        UMETA(DisplayName = "Feet"),
	Accessory1  UMETA(DisplayName = "Accessory 1"),
	Accessory2  UMETA(DisplayName = "Accessory 2"),
};

// ─── Stat Modifier ───────────────────────────────────────────────────────────
// Additive bonuses applied while a piece of equipment is worn.
// Removed cleanly on unequip via the same delta.

USTRUCT(BlueprintType)
struct DELTACODE_API FDCStatModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Equipment")
	float MaxHealthBonus = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Equipment")
	float MaxStaminaBonus = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Equipment")
	float AttackPowerBonus = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Equipment")
	float DefenseBonus = 0.0f;
};

// ─── Item Rarity ─────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EDCItemRarity : uint8
{
	Common      UMETA(DisplayName = "Common"),
	Uncommon    UMETA(DisplayName = "Uncommon"),
	Rare        UMETA(DisplayName = "Rare"),
	Epic        UMETA(DisplayName = "Epic"),
	Legendary   UMETA(DisplayName = "Legendary"),
};

// ─── Item Handle ─────────────────────────────────────────────────────────────
// Lightweight identifier for an item definition. Used across inventory,
// loot tables, vendor stock, and save data. Points to a UDCItemDefinition
// in the registry (Layer 2B).

USTRUCT(BlueprintType)
struct DELTACODE_API FDCItemHandle
{
	GENERATED_BODY()

	FDCItemHandle() = default;
	explicit FDCItemHandle(FName InItemID) : ItemID(InItemID) {}

	// Unique identifier matching UDCItemDefinition::ItemID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Item")
	FName ItemID = NAME_None;

	bool IsValid() const { return ItemID != NAME_None; }

	bool operator==(const FDCItemHandle& Other) const { return ItemID == Other.ItemID; }
	bool operator!=(const FDCItemHandle& Other) const { return !(*this == Other); }

	friend uint32 GetTypeHash(const FDCItemHandle& Handle)
	{
		return GetTypeHash(Handle.ItemID);
	}
};

// ─── Stat Block ──────────────────────────────────────────────────────────────
// Generic stat container used by characters, weapons, and equipment.
// Values are base amounts — modifiers from gear/abilities are applied on top.

USTRUCT(BlueprintType)
struct DELTACODE_API FDCStatBlock
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Stats")
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Stats")
	float MaxShield = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Stats")
	float MoveSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Stats")
	float AttackPower = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Stats")
	float Defense = 0.0f;

	// Gameplay tag set for ability/effect filtering
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Stats")
	FGameplayTagContainer StatTags;
};

// ─── Inventory Entry ─────────────────────────────────────────────────────────
// Runtime inventory slot: pairs an item handle with instance-specific data.

USTRUCT(BlueprintType)
struct DELTACODE_API FDCInventoryEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Inventory")
	FDCItemHandle ItemHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Inventory")
	int32 StackCount = 1;

	// Instance-level data (durability, enchantments, etc.) — expanded in Layer 4B
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Inventory")
	float Durability = -1.0f;

	bool IsValid() const { return ItemHandle.IsValid() && StackCount > 0; }
};
