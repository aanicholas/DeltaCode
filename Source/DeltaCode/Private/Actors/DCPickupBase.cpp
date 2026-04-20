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
#include "Actors/DCPickupBase.h"
#include "Actors/DCPlayerControllerBase.h"
#include "Actors/DCHUDBase.h"
#include "Components/DCInventoryComponent.h"
#include "Data/DCItemDefinition.h"
#include "Subsystems/DCItemDefinitionRegistry.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

ADCPickupBase::ADCPickupBase()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(MeshComponent);
	InteractionSphere->SetSphereRadius(InteractionRadius);
	InteractionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
}

void ADCPickupBase::BeginPlay()
{
	Super::BeginPlay();

	InteractionSphere->SetSphereRadius(InteractionRadius);

	if (!ItemHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DCPickupBase '%s' has no ItemHandle — pickup will be inert."),
			*GetName());
		return;
	}

	if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UDCItemDefinitionRegistry* Registry =
			GI->GetSubsystem<UDCItemDefinitionRegistry>())
		{
			UDCItemDefinition* Definition = Registry->FindItemByHandle(ItemHandle);
			if (Definition)
			{
				CachedDisplayName = Definition->DisplayName;
				ApplyVisualsFromDefinition(Definition);
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("DCPickupBase '%s' could not resolve ItemID '%s' in registry."),
					*GetName(), *ItemHandle.ItemID.ToString());
			}
		}
	}
}

void ADCPickupBase::ApplyVisualsFromDefinition(UDCItemDefinition* Definition)
{
	if (!Definition || Definition->WorldMesh.IsNull())
	{
		return;
	}

	// Sync-load for now — streaming/async load wiring lands in Layer 4 polish.
	if (UStaticMesh* Mesh = Definition->WorldMesh.LoadSynchronous())
	{
		MeshComponent->SetStaticMesh(Mesh);
	}
}

void ADCPickupBase::InitializeFromHandle(const FDCItemHandle& InHandle, int32 InQuantity)
{
	ItemHandle = InHandle;
	Quantity = FMath::Max(1, InQuantity);

	if (HasActorBegunPlay() && ItemHandle.IsValid())
	{
		if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UDCItemDefinitionRegistry* Registry =
				GI->GetSubsystem<UDCItemDefinitionRegistry>())
			{
				if (UDCItemDefinition* Definition = Registry->FindItemByHandle(ItemHandle))
				{
					CachedDisplayName = Definition->DisplayName;
					ApplyVisualsFromDefinition(Definition);
				}
			}
		}
	}
}

// ─── IDCInteractable ─────────────────────────────────────────────────────────

void ADCPickupBase::OnInteracted_Implementation(APawn* Interactor)
{
	if (!Interactor || !ItemHandle.IsValid())
	{
		return;
	}

	ADCPlayerControllerBase* PC = Cast<ADCPlayerControllerBase>(Interactor->GetController());
	if (!PC)
	{
		return;
	}

	UDCInventoryComponent* Inventory = PC->GetInventoryComponent();
	if (!Inventory)
	{
		return;
	}

	int32 AmountAdded = 0;
	const EDCInventoryResult Result = Inventory->AddItem(ItemHandle, Quantity, AmountAdded);

	if (AmountAdded <= 0)
	{
		// Tell the player why — inventory full, too heavy, duplicate unique, etc.
		if (ADCHUDBase* HUD = Cast<ADCHUDBase>(PC->GetHUD()))
		{
			HUD->ShowNotification(
				NSLOCTEXT("DeltaCode", "PickupRefused", "Can't pick that up right now."), 2.0f);
		}
		return;
	}

	// Notification: "Picked up 3× Medkit"
	if (ADCHUDBase* HUD = Cast<ADCHUDBase>(PC->GetHUD()))
	{
		const FText Msg = FText::Format(
			NSLOCTEXT("DeltaCode", "PickupTakenFmt", "Picked up {0}× {1}"),
			FText::AsNumber(AmountAdded),
			CachedDisplayName.IsEmpty() ? FText::FromName(ItemHandle.ItemID) : CachedDisplayName);
		HUD->ShowNotification(Msg, 2.5f);
	}

	OnPickedUp(Interactor, AmountAdded);

	Quantity -= AmountAdded;
	if (Quantity <= 0 || Result == EDCInventoryResult::Success)
	{
		Destroy();
	}
}

FText ADCPickupBase::GetInteractionPrompt_Implementation() const
{
	const FText Name = CachedDisplayName.IsEmpty()
		? FText::FromName(ItemHandle.ItemID)
		: CachedDisplayName;

	if (Quantity > 1)
	{
		return FText::Format(
			NSLOCTEXT("DeltaCode", "PickupPromptFmtStack", "Pick up {0} ({1})"),
			Name, FText::AsNumber(Quantity));
	}
	return FText::Format(NSLOCTEXT("DeltaCode", "PickupPromptFmt", "Pick up {0}"), Name);
}

bool ADCPickupBase::CanInteract_Implementation(APawn* /*Interactor*/) const
{
	return ItemHandle.IsValid() && Quantity > 0;
}
