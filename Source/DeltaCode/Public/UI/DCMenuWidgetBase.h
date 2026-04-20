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
#include "UI/DCUserWidgetBase.h"
#include "DCMenuWidgetBase.generated.h"

/**
 * Base class for pushable menu widgets (inventory, pause, vendor, quest log…).
 * Lives on the HUD's widget stack — CloseMenu() pops it via ADCHUDBase.
 *
 * [INPUT]  From: ADCHUDBase::PushWidget() — created & added to viewport
 * [OUTPUT] To:   ADCHUDBase::PopWidget()  — via CloseMenu()
 *
 * Child widgets wire a Close button's OnClicked to CloseMenu() and override
 * OnMenuOpened / OnMenuClosed in BP for tween animations.
 */
UCLASS(Abstract)
class DELTACODE_API UDCMenuWidgetBase : public UDCUserWidgetBase
{
	GENERATED_BODY()

public:
	/** Pop this menu off the HUD stack. Fires OnMenuClosed before removal. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	void CloseMenu();

	/** True while the menu is on the HUD stack. */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|UI")
	bool IsMenuOpen() const { return bIsMenuOpen; }

	UFUNCTION(BlueprintImplementableEvent, Category = "DeltaCode|UI")
	void OnMenuOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "DeltaCode|UI")
	void OnMenuClosed();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	bool bIsMenuOpen = false;
};
