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
#include "DCInventoryComponent.generated.h"

class UDCItemDefinition;
class UDCItemDefinitionRegistry;

// ─── Result / Reason Enums ───────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EDCInventoryResult : uint8
{
	Success            UMETA(DisplayName = "Success"),
	PartialSuccess     UMETA(DisplayName = "Partial Success"),
	Failed_UnknownItem UMETA(DisplayName = "Unknown Item"),
	Failed_NoRegistry  UMETA(DisplayName = "Registry Unavailable"),
	Failed_Full        UMETA(DisplayName = "Inventory Full"),
	Failed_Overweight  UMETA(DisplayName = "Over Weight Limit"),
	Failed_Duplicate   UMETA(DisplayName = "Duplicate Unique Item"),
	Failed_NotFound    UMETA(DisplayName = "Item Not In Inventory"),
	Failed_Invalid     UMETA(DisplayName = "Invalid Arguments"),
};

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDCOnInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDCOnItemAdded,
	FDCItemHandle, ItemHandle, int32, AmountAdded, int32, NewTotalCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDCOnItemRemoved,
	FDCItemHandle, ItemHandle, int32, AmountRemoved, int32, NewTotalCount);

/**
 * Runtime inventory container. Attaches to ADCPlayerControllerBase (and later to
 * NPC vendors / containers). Resolves FDCItemHandle → UDCItemDefinition via the
 * game-instance registry — never stores hard refs to definitions.
 *
 * [INPUT]  From: ADCPickupBase::OnInteracted         — AddItem(Handle, Count)
 * [INPUT]  From: UDCVendorComponent::CompletePurchase — AddItem(Handle, Count)
 * [INPUT]  From: UDCQuestGiverComponent::GrantReward  — AddItem(Handle, Count)
 * [OUTPUT] To:   W_DC_InventoryPanel                  — binds OnInventoryChanged
 * [OUTPUT] To:   UDCQuestSubsystem                    — listens for quest-item pickups
 * [OUTPUT] To:   UDCVendorComponent::CompleteSale     — removes sold items
 *
 * [DEPENDS ON] UDCItemDefinitionRegistry (Layer 2B) for handle resolution.
 *              Stack rules (MaxStackSize, bIsUnique) read from UDCItemDefinition.
 *
 * [EVENT] Fires: OnInventoryChanged — coarse-grained, any mutation
 * [EVENT] Fires: OnItemAdded        — per-handle with delta + new total
 * [EVENT] Fires: OnItemRemoved      — per-handle with delta + new total
 */
UCLASS(ClassGroup = (DeltaCode), meta = (BlueprintSpawnableComponent))
class DELTACODE_API UDCInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCInventoryComponent();

	// ── Configuration ───────────────────────────────────────────────────────

	// Maximum number of distinct slots. 0 = unlimited.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Inventory",
	          meta = (ClampMin = "0"))
	int32 MaxSlots = 0;

	// Maximum carried weight in kg. 0 = no weight limit (arcade).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Inventory",
	          meta = (ClampMin = "0.0"))
	float MaxWeight = 0.0f;

	// ── Mutation ────────────────────────────────────────────────────────────

	/**
	 * Add `Count` of `ItemHandle` to the inventory. Respects MaxStackSize and
	 * bIsUnique. Writes the number actually added to OutAmountAdded — partial
	 * accepts return PartialSuccess.
	 */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Inventory")
	EDCInventoryResult AddItem(const FDCItemHandle& ItemHandle, int32 Count,
	                           int32& OutAmountAdded);

	/** Remove `Count` of `ItemHandle`. Fails with Failed_NotFound if insufficient. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Inventory")
	EDCInventoryResult RemoveItem(const FDCItemHandle& ItemHandle, int32 Count);

	/** Drop `Count` from the inventory. Pickup spawn is wired in Layer 3A. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Inventory")
	EDCInventoryResult DropItem(const FDCItemHandle& ItemHandle, int32 Count);

	/** Consume a single unit (e.g. potion). Fires OnItemRemoved. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Inventory")
	EDCInventoryResult UseItem(const FDCItemHandle& ItemHandle);

	/** Remove all entries. Broadcasts a single OnInventoryChanged. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Inventory")
	void ClearAll();

	/** Replace all entries with the given payload (used by UDCSaveGame restore). */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Inventory|Save")
	void RestoreFromSave(const TArray<FDCInventoryEntry>& SavedEntries);

	// ── Query ───────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Inventory")
	bool HasItem(const FDCItemHandle& ItemHandle, int32 Count = 1) const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Inventory")
	int32 GetItemCount(const FDCItemHandle& ItemHandle) const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Inventory")
	int32 GetNumSlotsUsed() const { return Entries.Num(); }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Inventory")
	float GetCurrentWeight() const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Inventory")
	const TArray<FDCInventoryEntry>& GetEntries() const { return Entries; }

	/** Resolve a handle → definition via the registry (may return nullptr). */
	UDCItemDefinition* ResolveDefinition(const FDCItemHandle& ItemHandle) const;

	// ── Events ──────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Inventory")
	FDCOnInventoryChanged OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Inventory")
	FDCOnItemAdded OnItemAdded;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Inventory")
	FDCOnItemRemoved OnItemRemoved;

protected:
	virtual void BeginPlay() override;

	// Runtime item slots. Multiple entries may share an ItemID if a stack is full.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeltaCode|Inventory",
	          meta = (AllowPrivateAccess = "true"))
	TArray<FDCInventoryEntry> Entries;

private:
	// Cached on first use — gameplay layer never re-imports.
	UDCItemDefinitionRegistry* GetRegistry() const;

	// Sums stack counts across all entries for a handle.
	int32 CountInternal(const FDCItemHandle& ItemHandle) const;
};
