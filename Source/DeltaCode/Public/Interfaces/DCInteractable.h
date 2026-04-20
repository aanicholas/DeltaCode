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
#include "UObject/Interface.h"
#include "DCInteractable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UDCInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for any actor the player can interact with.
 * Implemented by pickups, NPCs, doors, terminals, quest objects.
 */
class DELTACODE_API IDCInteractable
{
	GENERATED_BODY()

public:
	// Called when a pawn initiates interaction with this actor
	// [OUTPUT] To: Implementing actor's specific interaction logic
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DeltaCode|Interaction")
	void OnInteracted(APawn* Interactor);

	// Returns the display text shown in the HUD interact prompt
	// [OUTPUT] To: W_DC_HUDRoot → interact prompt widget
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DeltaCode|Interaction")
	FText GetInteractionPrompt() const;

	// Whether interaction is currently available
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DeltaCode|Interaction")
	bool CanInteract(APawn* Interactor) const;
};
