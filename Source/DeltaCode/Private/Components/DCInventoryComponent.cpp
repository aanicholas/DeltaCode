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
#include "Components/DCInventoryComponent.h"
#include "Data/DCItemDefinition.h"
#include "Subsystems/DCItemDefinitionRegistry.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"

UDCInventoryComponent::UDCInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDCInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

// ─── Registry resolution ─────────────────────────────────────────────────────

UDCItemDefinitionRegistry* UDCInventoryComponent::GetRegistry() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}
	const UWorld* World = Owner->GetWorld();
	if (!World)
	{
		return nullptr;
	}
	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}
	return GI->GetSubsystem<UDCItemDefinitionRegistry>();
}

UDCItemDefinition* UDCInventoryComponent::ResolveDefinition(
	const FDCItemHandle& ItemHandle) const
{
	if (!ItemHandle.IsValid())
	{
		return nullptr;
	}
	if (UDCItemDefinitionRegistry* Registry = GetRegistry())
	{
		return Registry->FindItemByHandle(ItemHandle);
	}
	return nullptr;
}

// ─── Query helpers ───────────────────────────────────────────────────────────

int32 UDCInventoryComponent::CountInternal(const FDCItemHandle& ItemHandle) const
{
	int32 Total = 0;
	for (const FDCInventoryEntry& Entry : Entries)
	{
		if (Entry.ItemHandle == ItemHandle)
		{
			Total += Entry.StackCount;
		}
	}
	return Total;
}

bool UDCInventoryComponent::HasItem(const FDCItemHandle& ItemHandle, int32 Count) const
{
	if (!ItemHandle.IsValid() || Count <= 0)
	{
		return false;
	}
	return CountInternal(ItemHandle) >= Count;
}

int32 UDCInventoryComponent::GetItemCount(const FDCItemHandle& ItemHandle) const
{
	if (!ItemHandle.IsValid())
	{
		return 0;
	}
	return CountInternal(ItemHandle);
}

float UDCInventoryComponent::GetCurrentWeight() const
{
	float Total = 0.0f;
	for (const FDCInventoryEntry& Entry : Entries)
	{
		if (const UDCItemDefinition* Def = ResolveDefinition(Entry.ItemHandle))
		{
			Total += Def->Weight * static_cast<float>(Entry.StackCount);
		}
	}
	return Total;
}

// ─── Mutation ────────────────────────────────────────────────────────────────

EDCInventoryResult UDCInventoryComponent::AddItem(
	const FDCItemHandle& ItemHandle, int32 Count, int32& OutAmountAdded)
{
	OutAmountAdded = 0;

	if (!ItemHandle.IsValid() || Count <= 0)
	{
		return EDCInventoryResult::Failed_Invalid;
	}

	UDCItemDefinition* Definition = ResolveDefinition(ItemHandle);
	if (!Definition)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DCInventoryComponent: AddItem failed — handle '%s' not found in registry."),
			*ItemHandle.ItemID.ToString());
		return EDCInventoryResult::Failed_UnknownItem;
	}

	// Unique items: reject duplicates outright
	if (Definition->bIsUnique && CountInternal(ItemHandle) > 0)
	{
		return EDCInventoryResult::Failed_Duplicate;
	}

	const int32 MaxStack = Definition->bIsUnique
		? 1
		: FMath::Max(1, Definition->MaxStackSize);
	const float ItemWeight = Definition->Weight;

	int32 Remaining = Definition->bIsUnique ? 1 : Count;

	// 1) Fill existing stacks that have room
	for (int32 Index = 0; Index < Entries.Num() && Remaining > 0; ++Index)
	{
		FDCInventoryEntry& Entry = Entries[Index];
		if (Entry.ItemHandle != ItemHandle || Entry.StackCount >= MaxStack)
		{
			continue;
		}

		const int32 Room = MaxStack - Entry.StackCount;
		int32 ToAdd = FMath::Min(Room, Remaining);

		// Respect weight limit
		if (MaxWeight > 0.0f && ItemWeight > 0.0f)
		{
			const float WeightRemaining = MaxWeight - GetCurrentWeight();
			const int32 WeightFit = FMath::FloorToInt(WeightRemaining / ItemWeight);
			ToAdd = FMath::Min(ToAdd, FMath::Max(0, WeightFit));
		}

		if (ToAdd <= 0)
		{
			break;
		}

		Entry.StackCount += ToAdd;
		Remaining -= ToAdd;
		OutAmountAdded += ToAdd;
	}

	// 2) Create new slots for the leftover, up to MaxSlots
	while (Remaining > 0)
	{
		if (MaxSlots > 0 && Entries.Num() >= MaxSlots)
		{
			break;
		}

		int32 ToAdd = FMath::Min(MaxStack, Remaining);

		if (MaxWeight > 0.0f && ItemWeight > 0.0f)
		{
			const float WeightRemaining = MaxWeight - GetCurrentWeight();
			const int32 WeightFit = FMath::FloorToInt(WeightRemaining / ItemWeight);
			ToAdd = FMath::Min(ToAdd, FMath::Max(0, WeightFit));
		}

		if (ToAdd <= 0)
		{
			break;
		}

		FDCInventoryEntry NewEntry;
		NewEntry.ItemHandle = ItemHandle;
		NewEntry.StackCount = ToAdd;
		Entries.Add(NewEntry);

		Remaining -= ToAdd;
		OutAmountAdded += ToAdd;
	}

	if (OutAmountAdded <= 0)
	{
		// Nothing fit — classify the first blocking reason
		if (MaxWeight > 0.0f && GetCurrentWeight() >= MaxWeight)
		{
			return EDCInventoryResult::Failed_Overweight;
		}
		return EDCInventoryResult::Failed_Full;
	}

	// [EVENT] Fires: OnItemAdded + OnInventoryChanged
	const int32 NewTotal = CountInternal(ItemHandle);
	OnItemAdded.Broadcast(ItemHandle, OutAmountAdded, NewTotal);
	OnInventoryChanged.Broadcast();

	const int32 Requested = Definition->bIsUnique ? 1 : Count;
	return (OutAmountAdded < Requested)
		? EDCInventoryResult::PartialSuccess
		: EDCInventoryResult::Success;
}

EDCInventoryResult UDCInventoryComponent::RemoveItem(
	const FDCItemHandle& ItemHandle, int32 Count)
{
	if (!ItemHandle.IsValid() || Count <= 0)
	{
		return EDCInventoryResult::Failed_Invalid;
	}

	if (CountInternal(ItemHandle) < Count)
	{
		return EDCInventoryResult::Failed_NotFound;
	}

	int32 Remaining = Count;

	// Drain from the tail so indices stay stable while we shrink stacks.
	for (int32 Index = Entries.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		FDCInventoryEntry& Entry = Entries[Index];
		if (Entry.ItemHandle != ItemHandle)
		{
			continue;
		}

		const int32 Take = FMath::Min(Entry.StackCount, Remaining);
		Entry.StackCount -= Take;
		Remaining -= Take;

		if (Entry.StackCount <= 0)
		{
			Entries.RemoveAt(Index);
		}
	}

	const int32 NewTotal = CountInternal(ItemHandle);
	OnItemRemoved.Broadcast(ItemHandle, Count, NewTotal);
	OnInventoryChanged.Broadcast();

	return EDCInventoryResult::Success;
}

EDCInventoryResult UDCInventoryComponent::DropItem(
	const FDCItemHandle& ItemHandle, int32 Count)
{
	// Pickup-spawn wiring lands in Layer 3A (ADCPickupBase). For now this is a
	// semantic alias for RemoveItem so UI can call Drop without a branch.
	return RemoveItem(ItemHandle, Count);
}

EDCInventoryResult UDCInventoryComponent::UseItem(const FDCItemHandle& ItemHandle)
{
	if (!ItemHandle.IsValid())
	{
		return EDCInventoryResult::Failed_Invalid;
	}

	if (!HasItem(ItemHandle, 1))
	{
		return EDCInventoryResult::Failed_NotFound;
	}

	// Consumable effects (heal, buff, etc.) resolve through GAS in Layer 3C.
	// This stub only removes one unit and broadcasts so UI can update.
	return RemoveItem(ItemHandle, 1);
}

void UDCInventoryComponent::ClearAll()
{
	if (Entries.IsEmpty())
	{
		return;
	}
	Entries.Reset();
	OnInventoryChanged.Broadcast();
}

// ─── Save / restore ──────────────────────────────────────────────────────────

void UDCInventoryComponent::RestoreFromSave(const TArray<FDCInventoryEntry>& SavedEntries)
{
	Entries = SavedEntries;
	OnInventoryChanged.Broadcast();
}
