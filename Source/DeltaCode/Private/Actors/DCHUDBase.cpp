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
#include "Actors/DCHUDBase.h"
#include "Blueprint/UserWidget.h"
#include "UI/DCHUDRootWidget.h"

ADCHUDBase::ADCHUDBase()
{
}

void ADCHUDBase::BeginPlay()
{
	Super::BeginPlay();

	if (HUDRootWidgetClass)
	{
		HUDRootWidget = CreateWidget<UUserWidget>(GetOwningPlayerController(),
		                                          HUDRootWidgetClass);
		if (HUDRootWidget)
		{
			HUDRootWidget->AddToViewport(0);
		}
	}
}

UUserWidget* ADCHUDBase::PushWidget(TSubclassOf<UUserWidget> WidgetClass)
{
	if (!WidgetClass)
	{
		return nullptr;
	}

	UUserWidget* NewWidget = CreateWidget<UUserWidget>(GetOwningPlayerController(),
	                                                    WidgetClass);
	if (NewWidget)
	{
		NewWidget->AddToViewport(100 + WidgetStack.Num());
		WidgetStack.Push(NewWidget);
		UpdateInputMode();
	}

	return NewWidget;
}

void ADCHUDBase::PopWidget()
{
	if (WidgetStack.Num() == 0)
	{
		return;
	}

	UUserWidget* TopWidget = WidgetStack.Pop();
	if (TopWidget)
	{
		TopWidget->RemoveFromParent();
	}

	UpdateInputMode();
}

void ADCHUDBase::ShowNotification(const FText& Message, float Duration)
{
	// [OUTPUT] To: UDCHUDRootWidget::ShowNotification — appends a notification entry widget
	if (UDCHUDRootWidget* DCRoot = Cast<UDCHUDRootWidget>(HUDRootWidget))
	{
		DCRoot->ShowNotification(Message, Duration);
		return;
	}

	// Fallback when the HUD root isn't a DeltaCode root (or isn't created yet)
	UE_LOG(LogTemp, Display, TEXT("DeltaCode Notification: %s"), *Message.ToString());
}

void ADCHUDBase::UpdateInputMode()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}

	if (WidgetStack.Num() > 0)
	{
		FInputModeUIOnly UIMode;
		UIMode.SetWidgetToFocus(WidgetStack.Last()->TakeWidget());
		UIMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(UIMode);
		PC->bShowMouseCursor = true;
	}
	else
	{
		FInputModeGameOnly GameMode;
		PC->SetInputMode(GameMode);
		PC->bShowMouseCursor = false;
	}
}
