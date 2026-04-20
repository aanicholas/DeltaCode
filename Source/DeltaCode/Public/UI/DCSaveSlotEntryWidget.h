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
#include "SaveSystem/DCSaveSlotInfo.h"
#include "DCSaveSlotEntryWidget.generated.h"

class UButton;
class UTextBlock;
class UDCSaveSlotEntryWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnSlotEntryClicked,
	UDCSaveSlotEntryWidget*, Entry);

/**
 * One row of W_DC_SaveSlotPanel. Binds to an FDCSaveSlotInfo projection and
 * fires OnClicked when the player selects it. The panel owns selection state.
 *
 * [INPUT]  From: UDCSaveSlotPanelWidget::RefreshSlots — SetSlotInfo()
 * [OUTPUT] To:   UDCSaveSlotPanelWidget — OnClicked delegate (row picked)
 */
UCLASS(Abstract)
class DELTACODE_API UDCSaveSlotEntryWidget : public UDCUserWidgetBase
{
	GENERATED_BODY()

public:
	/** Panel calls this when building the row — caches the info and refreshes labels. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	void SetSlotInfo(const FDCSaveSlotInfo& InInfo);

	UFUNCTION(BlueprintPure, Category = "DeltaCode|UI")
	const FDCSaveSlotInfo& GetSlotInfo() const { return SlotInfo; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|UI")
	int32 GetSlotIndex() const { return SlotInfo.SlotIndex; }

	/** Toggle the selected visual (implemented in BP — tint, border, etc.). */
	UFUNCTION(BlueprintImplementableEvent, Category = "DeltaCode|UI")
	void OnSelectionChanged(bool bSelected);

	/** Fires from SelectButton->OnClicked. Panel subscribes to route selection. */
	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|UI")
	FDCOnSlotEntryClicked OnClicked;

protected:
	virtual void NativeConstruct() override;

	/** Hook for BP to re-render labels after SetSlotInfo — raw struct is available via GetSlotInfo. */
	UFUNCTION(BlueprintImplementableEvent, Category = "DeltaCode|UI")
	void OnSlotInfoApplied();

	// BindWidgetOptional button that covers the row surface.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UButton> SelectButton;

	// Optional text fields populated natively when present; BP may override via OnSlotInfoApplied.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UTextBlock> SlotIndexText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UTextBlock> PlayerNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UTextBlock> LevelNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UTextBlock> PlayerLevelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UTextBlock> PlayTimeText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UTextBlock> TimestampText;

private:
	UFUNCTION() void HandleSelectClicked();

	void RefreshNativeLabels();

	UPROPERTY()
	FDCSaveSlotInfo SlotInfo;
};
