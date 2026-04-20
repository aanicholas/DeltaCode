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
#include "UI/DCMenuWidgetBase.h"
#include "SaveSystem/DCSaveSlotInfo.h"
#include "DCSaveSlotPanelWidget.generated.h"

class UButton;
class UPanelWidget;
class UDCSaveSlotEntryWidget;

UENUM(BlueprintType)
enum class EDCSaveSlotPanelMode : uint8
{
	Save  UMETA(DisplayName = "Save"),
	Load  UMETA(DisplayName = "Load"),
};

/**
 * Slot-picker for Save and Load flows. Populates SlotsContainer with one
 * UDCSaveSlotEntryWidget per authored slot, tracks selection, then routes the
 * confirm button to UDCGameInstance — SaveGameToDisk / NewGameInSlot in Save
 * mode, LoadSlot in Load mode. Delete always calls DeleteSlot on the current
 * selection.
 *
 * [INPUT]  From: UDCPauseMenuWidget::OpenSlotPanel — PushWidget + ConfigureMode
 * [INPUT]  From: Main menu flow — PushWidget + ConfigureMode(Load)
 * [OUTPUT] To:   UDCGameInstance::{SaveGameToDisk, NewGameInSlot, LoadSlot, DeleteSlot}
 * [OUTPUT] To:   ADCHUDBase::PopWidget — CloseMenu() after a successful action
 *
 * [DEPENDS ON] UDCGameInstance   — slot enumeration + mutators
 * [DEPENDS ON] UDCSaveSlotEntryWidget — per-row view
 */
UCLASS(Abstract)
class DELTACODE_API UDCSaveSlotPanelWidget : public UDCMenuWidgetBase
{
	GENERATED_BODY()

public:
	/** Configure the panel for Save vs. Load before it refreshes the list. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	void ConfigureMode(EDCSaveSlotPanelMode InMode);

	UFUNCTION(BlueprintPure, Category = "DeltaCode|UI")
	EDCSaveSlotPanelMode GetMode() const { return Mode; }

	/** Rebuild the slot list from UDCGameInstance::EnumerateSlots. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	void RefreshSlots();

	UFUNCTION(BlueprintPure, Category = "DeltaCode|UI")
	int32 GetSelectedSlotIndex() const { return SelectedSlotIndex; }

	/** Row class spawned into SlotsContainer. Assign to W_DC_SaveSlotEntry in BP. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|UI")
	TSubclassOf<UDCSaveSlotEntryWidget> SlotEntryClass;

	/** Optional — when Save mode needs to spin up a brand-new run, travel here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|UI")
	FName NewGameLevelName = NAME_None;

protected:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "DeltaCode|UI")
	void OnModeChanged(EDCSaveSlotPanelMode NewMode);

	UFUNCTION(BlueprintImplementableEvent, Category = "DeltaCode|UI")
	void OnSlotsRefreshed();

	UFUNCTION(BlueprintImplementableEvent, Category = "DeltaCode|UI")
	void OnSelectionChanged(int32 NewSelectedIndex);

	// ── BindWidget buttons — BP child wires these ──────────────────────────

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UPanelWidget> SlotsContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UButton> DeleteButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UButton> CancelButton;

private:
	UFUNCTION() void HandleConfirmClicked();
	UFUNCTION() void HandleDeleteClicked();
	UFUNCTION() void HandleCancelClicked();
	UFUNCTION() void HandleEntryClicked(UDCSaveSlotEntryWidget* Entry);

	void BuildEntries(const TArray<FDCSaveSlotInfo>& Slots);
	void SetSelectedSlot(int32 NewIndex);
	void PerformSaveOrLoad();

	UPROPERTY()
	TArray<TObjectPtr<UDCSaveSlotEntryWidget>> EntryWidgets;

	EDCSaveSlotPanelMode Mode = EDCSaveSlotPanelMode::Load;
	int32 SelectedSlotIndex = INDEX_NONE;
};
