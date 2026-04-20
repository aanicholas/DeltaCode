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
#include "DCNotificationEntryWidget.generated.h"

/**
 * Single notification entry — shown by UDCHUDRootWidget in its notification panel.
 * Auto-removes itself after `Duration` seconds. The visual layout (background,
 * icon, fade animation) lives in the W_DC_NotificationEntry Blueprint child.
 *
 * [INPUT] From: UDCHUDRootWidget::ShowNotification()
 */
UCLASS(Abstract)
class DELTACODE_API UDCNotificationEntryWidget : public UDCUserWidgetBase
{
	GENERATED_BODY()

public:
	/** Populate the message and start the auto-dismiss timer. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	void SetMessage(const FText& InMessage, float InDuration);

	UFUNCTION(BlueprintPure, Category = "DeltaCode|UI")
	FText GetMessage() const { return Message; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|UI")
	float GetDuration() const { return Duration; }

	/** Overridable hook — BP child animates fade-in / updates bound text. */
	UFUNCTION(BlueprintImplementableEvent, Category = "DeltaCode|UI")
	void OnNotificationShown(const FText& InMessage);

	/** Overridable hook — BP child plays fade-out animation before dismiss. */
	UFUNCTION(BlueprintImplementableEvent, Category = "DeltaCode|UI")
	void OnNotificationDismissing();

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|UI")
	FText Message;

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|UI")
	float Duration = 3.0f;

private:
	FTimerHandle DismissTimer;

	void DismissNow();
};
