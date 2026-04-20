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
#include "GameFramework/Character.h"
#include "Interfaces/DCInteractable.h"
#include "DCNPCBase.generated.h"

class UDCNPCDefinition;
class UDCDialogueComponent;

/**
 * Base NPC character. Reads its identity, dialogue tree, and role wiring from
 * a UDCNPCDefinition soft reference. Implements IDCInteractable so the
 * interaction system can initiate conversation.
 *
 * Vendor and quest-giver role wiring layers on top of this in Layer 3D / 3E.
 *
 * [INPUT]  From: UDCInteractionComponent::TryInteract → OnInteracted
 * [OUTPUT] To:   UDCDialogueComponent::StartConversation
 *
 * [DEPENDS ON] UDCNPCDefinition (Layer 2B) — identity + dialogue tree
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API ADCNPCBase : public ACharacter, public IDCInteractable
{
	GENERATED_BODY()

public:
	ADCNPCBase();

	// IDCInteractable
	virtual void OnInteracted_Implementation(APawn* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual bool CanInteract_Implementation(APawn* Interactor) const override;

	// Authored per-placement — loads and caches on BeginPlay
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|NPC")
	TSoftObjectPtr<UDCNPCDefinition> NPCDefinitionAsset;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|NPC")
	UDCNPCDefinition* GetNPCDefinition() const { return LoadedDefinition; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|NPC")
	UDCDialogueComponent* GetDialogueComponent() const { return DialogueComponent; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDCDialogueComponent> DialogueComponent;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDCNPCDefinition> LoadedDefinition;
};
