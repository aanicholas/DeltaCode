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
#include "UI/DCMenuWidgetBase.h"
#include "Actors/DCHUDBase.h"

void UDCMenuWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	bIsMenuOpen = true;
	OnMenuOpened();
}

void UDCMenuWidgetBase::NativeDestruct()
{
	if (bIsMenuOpen)
	{
		bIsMenuOpen = false;
		OnMenuClosed();
	}
	Super::NativeDestruct();
}

void UDCMenuWidgetBase::CloseMenu()
{
	if (!bIsMenuOpen)
	{
		return;
	}

	// Prefer routing through the HUD so the widget stack / input mode stays consistent.
	if (ADCHUDBase* HUD = GetDCHUD())
	{
		HUD->PopWidget();
		return;
	}

	// Fallback: direct removal if the HUD is unavailable.
	RemoveFromParent();
}
