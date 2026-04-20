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
#include "Types/DCCoreTypes.h"
#include "SaveSystem/DCSaveGame.h"
#include "DCEquipmentComponent.generated.h"

class ADCWeaponBase;
class UDCWeaponDefinition;
class UDCEquipmentDefinition;
class ADCCharacterBase;
class UDCItemDefinitionRegistry;
class UMeshComponent;

// ─── Equipped slot record ────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct DELTACODE_API FDCEquippedSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Equipment")
	FDCItemHandle ItemHandle;

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Equipment")
	int32 CurrentDurability = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Equipment")
	int32 MaxDurability = 0;

	// Spawned weapon actor (Weapon slot only)
	UPROPERTY()
	TObjectPtr<ADCWeaponBase> WeaponActor;

	// Attached cosmetic mesh (armor / accessory slots)
	UPROPERTY()
	TObjectPtr<UMeshComponent> AttachedMesh;

	// Cached definition to avoid re-resolving every query
	UPROPERTY()
	TObjectPtr<UDCEquipmentDefinition> CachedDefinition;

	bool IsBroken() const { return MaxDurability > 0 && CurrentDurability <= 0; }
	bool IsValid() const { return ItemHandle.IsValid(); }
};

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnWeaponEquipped, ADCWeaponBase*, Weapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDCOnWeaponUnequipped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCOnEquipmentChanged,
	EDCEquipmentSlot, Slot, FDCItemHandle, NewItemHandle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDCOnDurabilityChanged,
	EDCEquipmentSlot, Slot, int32, NewDurability, int32, MaxDurability);

/**
 * Equipment manager. Lives on ADCCharacterBase. Tracks every equipment slot
 * (weapon + armor + accessories), spawning weapon actors / attaching cosmetic
 * meshes, and applying or removing stat modifiers via the attribute set + health
 * component as gear is worn or removed.
 *
 * [INPUT]  From: Inventory UI / dialogue / pickup → EquipItem(Handle)
 * [INPUT]  From: ADCWeaponBase::TryFire → DamageDurability(Weapon, def->DurabilityPerUse)
 * [OUTPUT] To:   ADCWeaponBase::Initialize / TryFire / StartReload (weapon slot)
 * [OUTPUT] To:   UDCAttributeSet (AttackPower / Defense / MaxStamina) on equip / unequip
 * [OUTPUT] To:   UDCHealthComponent::AdjustMaxHealth on equip / unequip
 * [OUTPUT] To:   UI — OnEquipmentChanged / OnDurabilityChanged for paper-doll + bars
 *
 * [DEPENDS ON] UDCItemDefinitionRegistry — handle → UDCEquipmentDefinition
 * [DEPENDS ON] UDCEquipmentDefinition — slot, modifiers, durability, attached mesh
 */
UCLASS(ClassGroup = (DeltaCode), meta = (BlueprintSpawnableComponent))
class DELTACODE_API UDCEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCEquipmentComponent();

	// Default socket for weapons (overridden per equipment via AttachSocketOverride)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Equipment")
	FName AttachSocketName = TEXT("hand_r");

	// ── Generic slot API ────────────────────────────────────────────────────

	/**
	 * Equip the item identified by Handle. Resolves to UDCEquipmentDefinition,
	 * routes to the slot it declares, replaces any existing item in that slot,
	 * spawns / attaches the visual representation, and applies stat modifiers.
	 *
	 * Returns true on success. Failure causes: registry missing, non-equipment
	 * handle, slot None, weapon class missing, mesh load failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Equipment")
	bool EquipItem(const FDCItemHandle& Handle);

	/** Unequip whatever's in the given slot. Removes modifiers + visual. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Equipment")
	void UnequipSlot(EDCEquipmentSlot Slot);

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Equipment")
	bool IsSlotOccupied(EDCEquipmentSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Equipment")
	FDCItemHandle GetEquippedHandle(EDCEquipmentSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Equipment")
	bool GetSlotState(EDCEquipmentSlot Slot, FDCEquippedSlot& OutState) const;

	// ── Durability ──────────────────────────────────────────────────────────

	/** Subtract `Amount` from the slot's durability. Broadcasts OnDurabilityChanged. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Equipment")
	void DamageDurability(EDCEquipmentSlot Slot, int32 Amount);

	/** Repair the slot's durability up to its max. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Equipment")
	void RepairSlot(EDCEquipmentSlot Slot, int32 Amount);

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Equipment")
	bool IsSlotBroken(EDCEquipmentSlot Slot) const;

	// ── Weapon convenience (slot-typed wrappers around the weapon slot) ─────

	/** Equivalent to EquipItem(WeaponHandle); returns the spawned weapon actor. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Equipment")
	ADCWeaponBase* EquipWeapon(const FDCItemHandle& WeaponHandle);

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Equipment")
	void UnequipWeapon();

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Equipment")
	bool FireEquipped();

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Equipment")
	bool ReloadEquipped();

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Equipment")
	ADCWeaponBase* GetEquippedWeapon() const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Equipment")
	bool HasEquippedWeapon() const { return GetEquippedWeapon() != nullptr; }

	// ── Save / restore ──────────────────────────────────────────────────────

	/** Serialize current slot contents + durability values into the save payload. */
	void CaptureForSave(TArray<FDCSavedEquipmentSlot>& OutSlots) const;

	/** Rehydrate slots from save — re-equips items and restores durability. */
	void RestoreFromSave(const TArray<FDCSavedEquipmentSlot>& SavedSlots);

	// ── Delegates ───────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Equipment")
	FDCOnWeaponEquipped OnWeaponEquipped;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Equipment")
	FDCOnWeaponUnequipped OnWeaponUnequipped;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Equipment")
	FDCOnEquipmentChanged OnEquipmentChanged;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Equipment")
	FDCOnDurabilityChanged OnDurabilityChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UDCItemDefinitionRegistry* GetRegistry() const;
	ADCCharacterBase* GetOwnerCharacter() const;

	// Slot resolution
	FName ResolveAttachSocket(const UDCEquipmentDefinition& Def) const;

	// Spawn / attach the visual representation. Sets State.WeaponActor or AttachedMesh.
	bool SpawnSlotVisuals(FDCEquippedSlot& State, UDCEquipmentDefinition& Def);
	void TeardownSlotVisuals(FDCEquippedSlot& State);

	// Stat modifier application — additive + reversible
	void ApplyStatModifiers(const FDCStatModifier& Mods);
	void RemoveStatModifiers(const FDCStatModifier& Mods);

	UPROPERTY(Transient)
	TMap<EDCEquipmentSlot, FDCEquippedSlot> EquippedSlots;
};
