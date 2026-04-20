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
#include "UI/DCNotificationEntryWidget.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UDCNotificationEntryWidget::SetMessage(const FText& InMessage, float InDuration)
{
	Message = InMessage;
	Duration = FMath::Max(0.1f, InDuration);

	OnNotificationShown(Message);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DismissTimer, this,
			&UDCNotificationEntryWidget::DismissNow, Duration, false);
	}
}

void UDCNotificationEntryWidget::DismissNow()
{
	OnNotificationDismissing();
	RemoveFromParent();
}

void UDCNotificationEntryWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DismissTimer);
	}
	Super::NativeDestruct();
}
