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
#include "Blueprint/UserWidget.h"
#include "DCUserWidgetBase.generated.h"

class ADCPlayerControllerBase;
class ADCHUDBase;
class UDCInventoryComponent;
class UDCInteractionComponent;

/**
 * Base class for every DeltaCode UMG widget.
 * Provides typed accessors for the DeltaCode player controller, HUD, and core
 * gameplay components so child widgets don't re-implement Cast<> boilerplate.
 *
 * All W_DC_* widget Blueprints must reparent to this (or a subclass) rather
 * than UUserWidget directly.
 *
 * [DEPENDS ON] ADCPlayerControllerBase (Layer 1)
 * [DEPENDS ON] ADCHUDBase              (Layer 1)
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class DELTACODE_API UDCUserWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "DeltaCode|UI")
	ADCPlayerControllerBase* GetDCPlayerController() const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|UI")
	ADCHUDBase* GetDCHUD() const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|UI")
	UDCInventoryComponent* GetInventory() const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|UI")
	UDCInteractionComponent* GetInteraction() const;
};
