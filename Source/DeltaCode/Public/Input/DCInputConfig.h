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
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DCInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * Pairs a Gameplay Tag with an Enhanced Input Action.
 * Allows systems to look up input actions by tag rather than hard reference.
 */
USTRUCT(BlueprintType)
struct FDCInputActionBinding
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Input")
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Input")
	TObjectPtr<UInputAction> InputAction;
};

/**
 * Data asset that defines the input configuration for a DeltaCode game.
 * Created as DA_DC_InputConfig in Content/DeltaCode/Input/.
 * Maps gameplay tags to Enhanced Input actions for tag-driven input binding.
 *
 * [OUTPUT] To: ADCCharacterBase::SetupPlayerInputComponent() — provides input bindings
 * [OUTPUT] To: ADCPlayerControllerBase::SetupInputComponent() — provides mapping context
 */
UCLASS(BlueprintType)
class DELTACODE_API UDCInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// Default mapping context applied to the local player on possess
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	// Tag-to-action bindings for gameplay systems
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Input",
	          meta = (TitleProperty = "InputTag"))
	TArray<FDCInputActionBinding> ActionBindings;

	// Find an input action by its gameplay tag
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Input")
	UInputAction* FindActionByTag(const FGameplayTag& InputTag) const;
};
