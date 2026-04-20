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
#include "DCPauseMenuWidget.generated.h"

class UButton;
class UDCSaveSlotPanelWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDCOnPauseMenuOpened);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDCOnPauseMenuClosed);

/**
 * Native base for W_DC_PauseMenu. Pauses the game while open, routes Resume /
 * Save / Load / Quit through UDCGameInstance, and opens the save-slot panel
 * via the HUD widget stack so the existing input-mode handling still applies.
 *
 * The W_DC_PauseMenu Blueprint child binds the BindWidget buttons below and
 * assigns SaveSlotPanelClass to its W_DC_SaveSlotPanel subclass so Save / Load
 * open the slot picker in the appropriate mode.
 *
 * [INPUT]  From: ADCPlayerControllerBase → HUD::PushWidget(PauseMenuClass)
 * [OUTPUT] To:   UDCGameInstance::SaveGameToDisk / LoadSlot
 * [OUTPUT] To:   ADCHUDBase::PushWidget(SaveSlotPanelClass)
 *
 * [DEPENDS ON] UDCGameInstance   — slot management
 * [DEPENDS ON] ADCHUDBase        — PushWidget / PopWidget lifecycle
 */
UCLASS(Abstract)
class DELTACODE_API UDCPauseMenuWidget : public UDCMenuWidgetBase
{
	GENERATED_BODY()

public:
	/** Save-slot panel class used by the Save / Load entry points. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|UI")
	TSubclassOf<UDCSaveSlotPanelWidget> SaveSlotPanelClass;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|UI")
	FDCOnPauseMenuOpened OnPauseMenuOpened;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|UI")
	FDCOnPauseMenuClosed OnPauseMenuClosed;

	// ── BlueprintCallable entry points ─────────────────────────────────────
	// Safe to bind directly from BP button OnClicked events — each of these
	// also fires from the BindWidget default handlers below.

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	void HandleResume();

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	void HandleOpenSavePanel();

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	void HandleOpenLoadPanel();

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	void HandleQuickSave();

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	void HandleReturnToMainMenu();

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|UI")
	void HandleQuitGame();

	/** Optional main-menu level to travel to from HandleReturnToMainMenu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|UI")
	FName MainMenuLevelName = NAME_None;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ── Optional BindWidget buttons — BP child wires these by name ─────────

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UButton> SaveButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UButton> LoadButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UButton> QuickSaveButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UButton> MainMenuButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional),
	          Category = "DeltaCode|UI")
	TObjectPtr<UButton> QuitButton;

private:
	void SetGamePaused(bool bPaused);

	// Spawn the slot panel in the requested mode on the HUD widget stack.
	void OpenSlotPanel(bool bSaveMode);

	UFUNCTION() void HandleResumeClicked();
	UFUNCTION() void HandleSaveClicked();
	UFUNCTION() void HandleLoadClicked();
	UFUNCTION() void HandleQuickSaveClicked();
	UFUNCTION() void HandleMainMenuClicked();
	UFUNCTION() void HandleQuitClicked();

	bool bPausedByThisWidget = false;
};
