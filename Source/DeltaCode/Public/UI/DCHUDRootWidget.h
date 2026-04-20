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
#include "DCHUDRootWidget.generated.h"

class UPanelWidget;
class UTextBlock;
class UDCNotificationEntryWidget;

/**
 * Persistent root widget created by ADCHUDBase on BeginPlay.
 * Hosts the notification stack and the interaction-prompt label.
 *
 * The W_DC_HUDRoot Blueprint child MUST bind these widgets:
 *   - NotificationContainer (UPanelWidget — a VerticalBox inside which
 *     UDCNotificationEntryWidget instances are appended)
 *   - InteractionPromptText (UTextBlock — set/cleared by the interaction component)
 *
 * [INPUT]  From: ADCHUDBase::ShowNotification() → ShowNotification()
 * [INPUT]  From: UDCInteractionComponent::OnFocusChanged → SetInteractionPrompt()
 * [OUTPUT] To:   W_DC_NotificationEntry instances in NotificationContainer
 *
 * [DEPENDS ON] UDCNotificationEntryWidget  (class picker set on the instance)
 * [DEPENDS ON] UDCInteractionComponent     (prompt source — bound in NativeConstruct)
 */
UCLASS(Abstract)
class DELTACODE_API UDCHUDRootWidget : public UDCUserWidgetBase
{
	GENERATED_BODY()

public:
	/** Append a new notification entry and start its auto-dismiss timer. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	void ShowNotification(const FText& Message, float Duration = 3.0f);

	/** Show or update the interaction prompt label (e.g. "Press E to pick up"). */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	void SetInteractionPrompt(const FText& PromptText);

	/** Hide the interaction prompt. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	void ClearInteractionPrompt();

	/** Notification widget class — set in the W_DC_HUDRoot BP default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|UI")
	TSubclassOf<UDCNotificationEntryWidget> NotificationWidgetClass;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// BindWidget: where notification entries are appended. The container itself
	// is authored in the W_DC_HUDRoot Blueprint (VerticalBox recommended).
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget),
	          Category = "DeltaCode|UI")
	TObjectPtr<UPanelWidget> NotificationContainer;

	// BindWidget: text label used for interaction prompts. Authored in BP.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget),
	          Category = "DeltaCode|UI")
	TObjectPtr<UTextBlock> InteractionPromptText;

	// BP hook fired when an interaction target changes — use this to tween
	// the prompt in/out from the Blueprint layer.
	UFUNCTION(BlueprintImplementableEvent, Category = "DeltaCode|UI")
	void OnInteractionPromptChanged(const FText& PromptText, bool bHasPrompt);

private:
	// [EVENT] Subscriber: UDCInteractionComponent::OnFocusChanged
	UFUNCTION()
	void HandleInteractionFocusChanged(AActor* NewTarget, const FText& PromptText);
};
