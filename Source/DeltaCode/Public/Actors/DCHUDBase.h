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
#include "GameFramework/HUD.h"
#include "DCHUDBase.generated.h"

/**
 * Base HUD class for DeltaCode games.
 * Manages a widget stack for menus, dialogues, inventory screens, etc.
 * Handles input mode switching when widgets are pushed/popped.
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API ADCHUDBase : public AHUD
{
	GENERATED_BODY()

public:
	ADCHUDBase();

	// Push a widget onto the stack. Sets UI input mode if stack was empty.
	// [INPUT]  From: Any gameplay system → opens a UI panel
	// [OUTPUT] To: PlayerController → SetInputMode() when stack changes
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	UUserWidget* PushWidget(TSubclassOf<UUserWidget> WidgetClass);

	// Pop the top widget. Restores game input mode if stack becomes empty.
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	void PopWidget();

	// Show a timed notification message on screen
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	void ShowNotification(const FText& Message, float Duration = 3.0f);

	// Access the root HUD widget (created in BeginPlay)
	UFUNCTION(BlueprintPure, Category = "DeltaCode|UI")
	UUserWidget* GetHUDRootWidget() const { return HUDRootWidget; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|UI")
	bool HasActiveWidgets() const { return WidgetStack.Num() > 0; }

protected:
	virtual void BeginPlay() override;

	// Root widget class — set to W_DC_HUDRoot in Blueprint child
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|UI")
	TSubclassOf<UUserWidget> HUDRootWidgetClass;

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> HUDRootWidget;

	UPROPERTY()
	TArray<TObjectPtr<UUserWidget>> WidgetStack;

	void UpdateInputMode();
};
