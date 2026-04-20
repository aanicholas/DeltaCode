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
#include "UI/DCHUDRootWidget.h"
#include "UI/DCNotificationEntryWidget.h"
#include "Components/DCInteractionComponent.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

void UDCHUDRootWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (InteractionPromptText)
	{
		InteractionPromptText->SetVisibility(ESlateVisibility::Collapsed);
	}

	// [INPUT] From: UDCInteractionComponent — bind prompt-changed callback
	if (UDCInteractionComponent* Interaction = GetInteraction())
	{
		Interaction->OnFocusChanged.AddDynamic(
			this, &UDCHUDRootWidget::HandleInteractionFocusChanged);
	}
}

void UDCHUDRootWidget::NativeDestruct()
{
	if (UDCInteractionComponent* Interaction = GetInteraction())
	{
		Interaction->OnFocusChanged.RemoveDynamic(
			this, &UDCHUDRootWidget::HandleInteractionFocusChanged);
	}
	Super::NativeDestruct();
}

void UDCHUDRootWidget::ShowNotification(const FText& Message, float Duration)
{
	if (!NotificationContainer || !NotificationWidgetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DCHUDRootWidget: ShowNotification skipped — container or class unset (msg: %s)"),
			*Message.ToString());
		return;
	}

	UDCNotificationEntryWidget* Entry = CreateWidget<UDCNotificationEntryWidget>(
		GetOwningPlayer(), NotificationWidgetClass);
	if (!Entry)
	{
		return;
	}

	NotificationContainer->AddChild(Entry);
	Entry->SetMessage(Message, Duration);
}

void UDCHUDRootWidget::SetInteractionPrompt(const FText& PromptText)
{
	const bool bHasPrompt = !PromptText.IsEmpty();

	if (InteractionPromptText)
	{
		InteractionPromptText->SetText(PromptText);
		InteractionPromptText->SetVisibility(
			bHasPrompt ? ESlateVisibility::HitTestInvisible
			           : ESlateVisibility::Collapsed);
	}

	OnInteractionPromptChanged(PromptText, bHasPrompt);
}

void UDCHUDRootWidget::ClearInteractionPrompt()
{
	SetInteractionPrompt(FText::GetEmpty());
}

void UDCHUDRootWidget::HandleInteractionFocusChanged(AActor* NewTarget, const FText& PromptText)
{
	if (!NewTarget)
	{
		ClearInteractionPrompt();
		return;
	}
	SetInteractionPrompt(PromptText);
}
