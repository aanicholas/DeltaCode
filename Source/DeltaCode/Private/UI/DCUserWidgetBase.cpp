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
#include "UI/DCUserWidgetBase.h"
#include "Actors/DCPlayerControllerBase.h"
#include "Actors/DCHUDBase.h"
#include "Components/DCInventoryComponent.h"
#include "Components/DCInteractionComponent.h"

ADCPlayerControllerBase* UDCUserWidgetBase::GetDCPlayerController() const
{
	return Cast<ADCPlayerControllerBase>(GetOwningPlayer());
}

ADCHUDBase* UDCUserWidgetBase::GetDCHUD() const
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		return Cast<ADCHUDBase>(PC->GetHUD());
	}
	return nullptr;
}

UDCInventoryComponent* UDCUserWidgetBase::GetInventory() const
{
	if (ADCPlayerControllerBase* PC = GetDCPlayerController())
	{
		return PC->GetInventoryComponent();
	}
	return nullptr;
}

UDCInteractionComponent* UDCUserWidgetBase::GetInteraction() const
{
	if (ADCPlayerControllerBase* PC = GetDCPlayerController())
	{
		return PC->GetInteractionComponent();
	}
	return nullptr;
}
