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
#include "Interfaces/DCInteractable.h"
#include "Types/DCCoreTypes.h"
#include "DCPickupBase.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UDCItemDefinition;

/**
 * World-placed item pickup. Resolves its mesh + display name from the
 * UDCItemDefinition referenced by ItemHandle on spawn.
 *
 * Implements IDCInteractable — UDCInteractionComponent detects it, the HUD
 * shows the pickup prompt, and on interact the item is pushed to the player's
 * inventory. Destroys itself once the full stack has been accepted.
 *
 * [INPUT]  From: UDCInteractionComponent::TryInteract() — via IDCInteractable
 * [OUTPUT] To:   UDCInventoryComponent::AddItem()        — item transfer
 * [OUTPUT] To:   ADCHUDBase::ShowNotification()          — pickup toast
 *
 * [DEPENDS ON] UDCItemDefinitionRegistry — handle resolution on BeginPlay
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API ADCPickupBase : public AActor, public IDCInteractable
{
	GENERATED_BODY()

public:
	ADCPickupBase();

	// IDCInteractable
	virtual void OnInteracted_Implementation(APawn* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual bool CanInteract_Implementation(APawn* Interactor) const override;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Pickup")
	FDCItemHandle GetItemHandle() const { return ItemHandle; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Pickup")
	int32 GetQuantity() const { return Quantity; }

	/** Set at spawn time (e.g. loot drops) to configure the pickup procedurally. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Pickup")
	void InitializeFromHandle(const FDCItemHandle& InHandle, int32 InQuantity = 1);

protected:
	virtual void BeginPlay() override;

	// The item this pickup represents. Authored per-placement in the level.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Pickup")
	FDCItemHandle ItemHandle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Pickup",
	          meta = (ClampMin = "1"))
	int32 Quantity = 1;

	// Interaction radius (for overlap-mode detection components)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Pickup")
	float InteractionRadius = 150.0f;

	// Root mesh — its StaticMesh is resolved from UDCItemDefinition.WorldMesh on BeginPlay
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> InteractionSphere;

	/** BP hook — override to play pickup FX / SFX before the actor destroys itself. */
	UFUNCTION(BlueprintImplementableEvent, Category = "DeltaCode|Pickup")
	void OnPickedUp(APawn* Interactor, int32 AmountTaken);

private:
	// Loads MeshComponent's StaticMesh from the definition's soft WorldMesh ref.
	void ApplyVisualsFromDefinition(UDCItemDefinition* Definition);

	// Cached display text — built from the definition on BeginPlay
	UPROPERTY(Transient)
	FText CachedDisplayName;
};
