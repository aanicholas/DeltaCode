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
#include "Components/DCEquipmentComponent.h"
#include "Actors/DCWeaponBase.h"
#include "Actors/DCCharacterBase.h"
#include "Components/DCHealthComponent.h"
#include "Data/DCEquipmentDefinition.h"
#include "Data/DCWeaponDefinition.h"
#include "GAS/DCAttributeSet.h"
#include "SaveSystem/DCSaveGame.h"
#include "Subsystems/DCItemDefinitionRegistry.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UDCEquipmentComponent::UDCEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDCEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Tear down every slot so spawned actors / mesh components are cleaned up.
	TArray<EDCEquipmentSlot> Slots;
	EquippedSlots.GenerateKeyArray(Slots);
	for (EDCEquipmentSlot Slot : Slots)
	{
		UnequipSlot(Slot);
	}
	Super::EndPlay(EndPlayReason);
}

// ─── Resolution ──────────────────────────────────────────────────────────────

UDCItemDefinitionRegistry* UDCEquipmentComponent::GetRegistry() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UDCItemDefinitionRegistry>();
		}
	}
	return nullptr;
}

ADCCharacterBase* UDCEquipmentComponent::GetOwnerCharacter() const
{
	return Cast<ADCCharacterBase>(GetOwner());
}

FName UDCEquipmentComponent::ResolveAttachSocket(const UDCEquipmentDefinition& Def) const
{
	if (Def.AttachSocketOverride != NAME_None)
	{
		return Def.AttachSocketOverride;
	}
	switch (Def.EquipmentSlot)
	{
	case EDCEquipmentSlot::Weapon:     return AttachSocketName;
	case EDCEquipmentSlot::Helmet:     return TEXT("head");
	case EDCEquipmentSlot::Chest:      return TEXT("spine_03");
	case EDCEquipmentSlot::Legs:       return TEXT("pelvis");
	case EDCEquipmentSlot::Hands:      return TEXT("hand_l");
	case EDCEquipmentSlot::Feet:       return TEXT("foot_l");
	case EDCEquipmentSlot::Accessory1: return TEXT("spine_03");
	case EDCEquipmentSlot::Accessory2: return TEXT("spine_03");
	default: return NAME_None;
	}
}

// ─── Generic equip / unequip ─────────────────────────────────────────────────

bool UDCEquipmentComponent::EquipItem(const FDCItemHandle& Handle)
{
	if (!Handle.IsValid())
	{
		return false;
	}

	UDCItemDefinitionRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DCEquipmentComponent: no registry — cannot equip '%s'."),
			*Handle.ItemID.ToString());
		return false;
	}

	UDCEquipmentDefinition* EquipDef =
		Cast<UDCEquipmentDefinition>(Registry->FindItemByHandle(Handle));
	if (!EquipDef)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DCEquipmentComponent: '%s' is not equippable."),
			*Handle.ItemID.ToString());
		return false;
	}

	const EDCEquipmentSlot Slot = EquipDef->EquipmentSlot;
	if (Slot == EDCEquipmentSlot::None)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DCEquipmentComponent: '%s' has EquipmentSlot=None."),
			*Handle.ItemID.ToString());
		return false;
	}

	// Replace any existing item in this slot first
	if (EquippedSlots.Contains(Slot))
	{
		UnequipSlot(Slot);
	}

	FDCEquippedSlot State;
	State.ItemHandle = Handle;
	State.MaxDurability = EquipDef->MaxDurability;
	State.CurrentDurability = EquipDef->MaxDurability; // fresh-from-definition
	State.CachedDefinition = EquipDef;

	if (!SpawnSlotVisuals(State, *EquipDef))
	{
		return false;
	}

	ApplyStatModifiers(EquipDef->Modifiers);

	EquippedSlots.Add(Slot, State);

	if (Slot == EDCEquipmentSlot::Weapon && State.WeaponActor)
	{
		OnWeaponEquipped.Broadcast(State.WeaponActor);
	}

	OnEquipmentChanged.Broadcast(Slot, Handle);
	if (State.MaxDurability > 0)
	{
		OnDurabilityChanged.Broadcast(Slot, State.CurrentDurability, State.MaxDurability);
	}
	return true;
}

void UDCEquipmentComponent::UnequipSlot(EDCEquipmentSlot Slot)
{
	FDCEquippedSlot* StatePtr = EquippedSlots.Find(Slot);
	if (!StatePtr)
	{
		return;
	}

	FDCEquippedSlot State = *StatePtr;
	EquippedSlots.Remove(Slot);

	if (State.CachedDefinition)
	{
		RemoveStatModifiers(State.CachedDefinition->Modifiers);
	}
	TeardownSlotVisuals(State);

	if (Slot == EDCEquipmentSlot::Weapon)
	{
		OnWeaponUnequipped.Broadcast();
	}
	OnEquipmentChanged.Broadcast(Slot, FDCItemHandle());
}

bool UDCEquipmentComponent::IsSlotOccupied(EDCEquipmentSlot Slot) const
{
	const FDCEquippedSlot* State = EquippedSlots.Find(Slot);
	return State && State->IsValid();
}

FDCItemHandle UDCEquipmentComponent::GetEquippedHandle(EDCEquipmentSlot Slot) const
{
	if (const FDCEquippedSlot* State = EquippedSlots.Find(Slot))
	{
		return State->ItemHandle;
	}
	return FDCItemHandle();
}

bool UDCEquipmentComponent::GetSlotState(EDCEquipmentSlot Slot, FDCEquippedSlot& OutState) const
{
	if (const FDCEquippedSlot* State = EquippedSlots.Find(Slot))
	{
		OutState = *State;
		return true;
	}
	return false;
}

// ─── Visuals ─────────────────────────────────────────────────────────────────

bool UDCEquipmentComponent::SpawnSlotVisuals(FDCEquippedSlot& State,
                                              UDCEquipmentDefinition& Def)
{
	ADCCharacterBase* Character = GetOwnerCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return false;
	}

	const FName Socket = ResolveAttachSocket(Def);

	// Weapon slot — spawn the runtime weapon actor
	if (Def.EquipmentSlot == EDCEquipmentSlot::Weapon)
	{
		UDCWeaponDefinition* WeaponDef = Cast<UDCWeaponDefinition>(&Def);
		if (!WeaponDef || WeaponDef->WeaponActorClass.IsNull())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("DCEquipmentComponent: weapon '%s' has no WeaponActorClass."),
				*Def.ItemID.ToString());
			return false;
		}

		TSubclassOf<ADCWeaponBase> SpawnClass = WeaponDef->WeaponActorClass.LoadSynchronous();
		if (!SpawnClass)
		{
			return false;
		}

		FActorSpawnParameters Params;
		Params.Owner = Character;
		Params.Instigator = Character;
		Params.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ADCWeaponBase* NewWeapon = World->SpawnActor<ADCWeaponBase>(
			SpawnClass, Character->GetActorTransform(), Params);
		if (!NewWeapon)
		{
			return false;
		}

		if (USkeletalMeshComponent* CharMesh = Character->GetMesh())
		{
			NewWeapon->AttachToComponent(CharMesh,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		}

		NewWeapon->Initialize(WeaponDef, Character);
		State.WeaponActor = NewWeapon;
		return true;
	}

	// Armor / accessory slot — attach a cosmetic mesh component
	USkeletalMeshComponent* CharMesh = Character->GetMesh();
	if (!CharMesh)
	{
		// No skeletal owner — still treat as success (stat modifiers only)
		return true;
	}

	if (!Def.EquippedSkeletalMesh.IsNull())
	{
		USkeletalMeshComponent* Attached = NewObject<USkeletalMeshComponent>(Character);
		Attached->RegisterComponent();
		Attached->AttachToComponent(CharMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		Attached->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (USkeletalMesh* Mesh = Def.EquippedSkeletalMesh.LoadSynchronous())
		{
			Attached->SetSkeletalMesh(Mesh);
		}
		State.AttachedMesh = Attached;
	}
	else if (!Def.EquippedStaticMesh.IsNull())
	{
		UStaticMeshComponent* Attached = NewObject<UStaticMeshComponent>(Character);
		Attached->RegisterComponent();
		Attached->AttachToComponent(CharMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		Attached->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (UStaticMesh* Mesh = Def.EquippedStaticMesh.LoadSynchronous())
		{
			Attached->SetStaticMesh(Mesh);
		}
		State.AttachedMesh = Attached;
	}
	return true;
}

void UDCEquipmentComponent::TeardownSlotVisuals(FDCEquippedSlot& State)
{
	if (State.WeaponActor)
	{
		State.WeaponActor->Destroy();
		State.WeaponActor = nullptr;
	}
	if (State.AttachedMesh)
	{
		State.AttachedMesh->DestroyComponent();
		State.AttachedMesh = nullptr;
	}
}

// ─── Stat modifiers ──────────────────────────────────────────────────────────

void UDCEquipmentComponent::ApplyStatModifiers(const FDCStatModifier& Mods)
{
	ADCCharacterBase* Character = GetOwnerCharacter();
	if (!Character)
	{
		return;
	}

	if (UDCAttributeSet* Attrs = Character->GetAttributeSet())
	{
		if (Mods.AttackPowerBonus != 0.0f)
		{
			Attrs->SetAttackPower(Attrs->GetAttackPower() + Mods.AttackPowerBonus);
		}
		if (Mods.DefenseBonus != 0.0f)
		{
			Attrs->SetDefense(Attrs->GetDefense() + Mods.DefenseBonus);
		}
		if (Mods.MaxStaminaBonus != 0.0f)
		{
			const float NewMax = Attrs->GetMaxStamina() + Mods.MaxStaminaBonus;
			Attrs->SetMaxStamina(NewMax);
			// Don't refill stamina on equip — only raise the cap
		}
	}

	if (Mods.MaxHealthBonus != 0.0f)
	{
		if (UDCHealthComponent* Health = Character->GetHealthComponent())
		{
			// Cap raise without forced refill (designers can heal explicitly if desired)
			Health->AdjustMaxHealth(Mods.MaxHealthBonus, /*bRefill*/ false);
		}
	}
}

void UDCEquipmentComponent::RemoveStatModifiers(const FDCStatModifier& Mods)
{
	ADCCharacterBase* Character = GetOwnerCharacter();
	if (!Character)
	{
		return;
	}

	if (UDCAttributeSet* Attrs = Character->GetAttributeSet())
	{
		if (Mods.AttackPowerBonus != 0.0f)
		{
			Attrs->SetAttackPower(Attrs->GetAttackPower() - Mods.AttackPowerBonus);
		}
		if (Mods.DefenseBonus != 0.0f)
		{
			Attrs->SetDefense(Attrs->GetDefense() - Mods.DefenseBonus);
		}
		if (Mods.MaxStaminaBonus != 0.0f)
		{
			const float NewMax = FMath::Max(1.0f,
				Attrs->GetMaxStamina() - Mods.MaxStaminaBonus);
			Attrs->SetMaxStamina(NewMax);
		}
	}

	if (Mods.MaxHealthBonus != 0.0f)
	{
		if (UDCHealthComponent* Health = Character->GetHealthComponent())
		{
			Health->AdjustMaxHealth(-Mods.MaxHealthBonus, /*bRefill*/ false);
		}
	}
}

// ─── Durability ──────────────────────────────────────────────────────────────

void UDCEquipmentComponent::DamageDurability(EDCEquipmentSlot Slot, int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	FDCEquippedSlot* State = EquippedSlots.Find(Slot);
	if (!State || State->MaxDurability <= 0)
	{
		return;
	}

	const int32 Old = State->CurrentDurability;
	State->CurrentDurability = FMath::Max(0, State->CurrentDurability - Amount);
	if (State->CurrentDurability != Old)
	{
		OnDurabilityChanged.Broadcast(Slot, State->CurrentDurability, State->MaxDurability);
	}
}

void UDCEquipmentComponent::RepairSlot(EDCEquipmentSlot Slot, int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	FDCEquippedSlot* State = EquippedSlots.Find(Slot);
	if (!State || State->MaxDurability <= 0)
	{
		return;
	}

	const int32 Old = State->CurrentDurability;
	State->CurrentDurability = FMath::Min(State->MaxDurability, State->CurrentDurability + Amount);
	if (State->CurrentDurability != Old)
	{
		OnDurabilityChanged.Broadcast(Slot, State->CurrentDurability, State->MaxDurability);
	}
}

bool UDCEquipmentComponent::IsSlotBroken(EDCEquipmentSlot Slot) const
{
	if (const FDCEquippedSlot* State = EquippedSlots.Find(Slot))
	{
		return State->IsBroken();
	}
	return false;
}

// ─── Weapon convenience wrappers ─────────────────────────────────────────────

ADCWeaponBase* UDCEquipmentComponent::EquipWeapon(const FDCItemHandle& WeaponHandle)
{
	if (!EquipItem(WeaponHandle))
	{
		return nullptr;
	}
	return GetEquippedWeapon();
}

void UDCEquipmentComponent::UnequipWeapon()
{
	UnequipSlot(EDCEquipmentSlot::Weapon);
}

bool UDCEquipmentComponent::FireEquipped()
{
	ADCWeaponBase* Weapon = GetEquippedWeapon();
	return Weapon ? Weapon->TryFire() : false;
}

bool UDCEquipmentComponent::ReloadEquipped()
{
	ADCWeaponBase* Weapon = GetEquippedWeapon();
	return Weapon ? Weapon->StartReload() : false;
}

ADCWeaponBase* UDCEquipmentComponent::GetEquippedWeapon() const
{
	if (const FDCEquippedSlot* State = EquippedSlots.Find(EDCEquipmentSlot::Weapon))
	{
		return State->WeaponActor;
	}
	return nullptr;
}

// ─── Save / restore ──────────────────────────────────────────────────────────

void UDCEquipmentComponent::CaptureForSave(TArray<FDCSavedEquipmentSlot>& OutSlots) const
{
	OutSlots.Reset();
	OutSlots.Reserve(EquippedSlots.Num());
	for (const TPair<EDCEquipmentSlot, FDCEquippedSlot>& Pair : EquippedSlots)
	{
		FDCSavedEquipmentSlot Saved;
		Saved.Slot = Pair.Key;
		Saved.ItemHandle = Pair.Value.ItemHandle;
		Saved.CurrentDurability = Pair.Value.CurrentDurability;
		OutSlots.Add(MoveTemp(Saved));
	}
}

void UDCEquipmentComponent::RestoreFromSave(const TArray<FDCSavedEquipmentSlot>& SavedSlots)
{
	// Tear down whatever is currently equipped so modifiers / visuals start clean
	TArray<EDCEquipmentSlot> Slots;
	EquippedSlots.GenerateKeyArray(Slots);
	for (EDCEquipmentSlot Slot : Slots)
	{
		UnequipSlot(Slot);
	}

	for (const FDCSavedEquipmentSlot& Saved : SavedSlots)
	{
		if (!Saved.ItemHandle.IsValid())
		{
			continue;
		}
		if (!EquipItem(Saved.ItemHandle))
		{
			continue;
		}
		// EquipItem sets CurrentDurability to max; overwrite with the saved value.
		if (FDCEquippedSlot* State = EquippedSlots.Find(Saved.Slot))
		{
			if (State->MaxDurability > 0)
			{
				const int32 Clamped = FMath::Clamp(Saved.CurrentDurability,
					0, State->MaxDurability);
				if (Clamped != State->CurrentDurability)
				{
					State->CurrentDurability = Clamped;
					OnDurabilityChanged.Broadcast(Saved.Slot,
						State->CurrentDurability, State->MaxDurability);
				}
			}
		}
	}
}
