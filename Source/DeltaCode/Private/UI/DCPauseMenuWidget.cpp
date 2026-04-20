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
#include "UI/DCPauseMenuWidget.h"
#include "UI/DCSaveSlotPanelWidget.h"
#include "Actors/DCGameInstance.h"
#include "Actors/DCHUDBase.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UDCPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetGamePaused(true);

	if (ResumeButton)    { ResumeButton->OnClicked.AddUniqueDynamic(this, &UDCPauseMenuWidget::HandleResumeClicked); }
	if (SaveButton)      { SaveButton->OnClicked.AddUniqueDynamic(this, &UDCPauseMenuWidget::HandleSaveClicked); }
	if (LoadButton)      { LoadButton->OnClicked.AddUniqueDynamic(this, &UDCPauseMenuWidget::HandleLoadClicked); }
	if (QuickSaveButton) { QuickSaveButton->OnClicked.AddUniqueDynamic(this, &UDCPauseMenuWidget::HandleQuickSaveClicked); }
	if (MainMenuButton)  { MainMenuButton->OnClicked.AddUniqueDynamic(this, &UDCPauseMenuWidget::HandleMainMenuClicked); }
	if (QuitButton)      { QuitButton->OnClicked.AddUniqueDynamic(this, &UDCPauseMenuWidget::HandleQuitClicked); }

	OnPauseMenuOpened.Broadcast();
}

void UDCPauseMenuWidget::NativeDestruct()
{
	if (bPausedByThisWidget)
	{
		SetGamePaused(false);
	}

	OnPauseMenuClosed.Broadcast();

	Super::NativeDestruct();
}

void UDCPauseMenuWidget::SetGamePaused(bool bPaused)
{
	// Only toggle pause for the state transition we caused, so nested menus
	// (save panel opening on top of us) don't flap the pause flag.
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (bPaused && !UGameplayStatics::IsGamePaused(this))
		{
			UGameplayStatics::SetGamePaused(this, true);
			bPausedByThisWidget = true;
		}
		else if (!bPaused && bPausedByThisWidget)
		{
			UGameplayStatics::SetGamePaused(this, false);
			bPausedByThisWidget = false;
		}
		SetIsFocusable(true);
	}
}

// ─── Public entry points ─────────────────────────────────────────────────────

void UDCPauseMenuWidget::HandleResume()
{
	CloseMenu();
}

void UDCPauseMenuWidget::HandleOpenSavePanel()
{
	OpenSlotPanel(/*bSaveMode=*/true);
}

void UDCPauseMenuWidget::HandleOpenLoadPanel()
{
	OpenSlotPanel(/*bSaveMode=*/false);
}

void UDCPauseMenuWidget::HandleQuickSave()
{
	if (UDCGameInstance* GI = Cast<UDCGameInstance>(UGameplayStatics::GetGameInstance(this)))
	{
		GI->SaveGameToDisk();
	}
}

void UDCPauseMenuWidget::HandleReturnToMainMenu()
{
	if (MainMenuLevelName.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[DC] PauseMenu::HandleReturnToMainMenu — MainMenuLevelName is unset."));
		return;
	}

	// Un-pause before level travel; OpenLevel ignores pause but the travel path
	// leaves stale timers otherwise.
	if (bPausedByThisWidget)
	{
		SetGamePaused(false);
	}
	UGameplayStatics::OpenLevel(this, MainMenuLevelName);
}

void UDCPauseMenuWidget::HandleQuitGame()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit,
			/*bIgnorePlatformRestrictions=*/false);
	}
}

// ─── BindWidget button handlers ──────────────────────────────────────────────

void UDCPauseMenuWidget::HandleResumeClicked()    { HandleResume(); }
void UDCPauseMenuWidget::HandleSaveClicked()      { HandleOpenSavePanel(); }
void UDCPauseMenuWidget::HandleLoadClicked()      { HandleOpenLoadPanel(); }
void UDCPauseMenuWidget::HandleQuickSaveClicked() { HandleQuickSave(); }
void UDCPauseMenuWidget::HandleMainMenuClicked()  { HandleReturnToMainMenu(); }
void UDCPauseMenuWidget::HandleQuitClicked()      { HandleQuitGame(); }

// ─── Internal ────────────────────────────────────────────────────────────────

void UDCPauseMenuWidget::OpenSlotPanel(bool bSaveMode)
{
	if (!SaveSlotPanelClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[DC] PauseMenu::OpenSlotPanel — SaveSlotPanelClass is unset."));
		return;
	}

	ADCHUDBase* HUD = GetDCHUD();
	if (!HUD)
	{
		return;
	}

	if (UUserWidget* Created = HUD->PushWidget(SaveSlotPanelClass))
	{
		if (UDCSaveSlotPanelWidget* SlotPanel = Cast<UDCSaveSlotPanelWidget>(Created))
		{
			SlotPanel->ConfigureMode(bSaveMode
				? EDCSaveSlotPanelMode::Save
				: EDCSaveSlotPanelMode::Load);
		}
	}
}
