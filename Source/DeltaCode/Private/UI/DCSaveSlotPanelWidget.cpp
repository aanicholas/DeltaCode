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
#include "UI/DCSaveSlotPanelWidget.h"
#include "UI/DCSaveSlotEntryWidget.h"
#include "Actors/DCGameInstance.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Kismet/GameplayStatics.h"

void UDCSaveSlotPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ConfirmButton) { ConfirmButton->OnClicked.AddUniqueDynamic(this, &UDCSaveSlotPanelWidget::HandleConfirmClicked); }
	if (DeleteButton)  { DeleteButton->OnClicked.AddUniqueDynamic(this,  &UDCSaveSlotPanelWidget::HandleDeleteClicked); }
	if (CancelButton)  { CancelButton->OnClicked.AddUniqueDynamic(this,  &UDCSaveSlotPanelWidget::HandleCancelClicked); }

	RefreshSlots();
}

void UDCSaveSlotPanelWidget::ConfigureMode(EDCSaveSlotPanelMode InMode)
{
	Mode = InMode;
	OnModeChanged(Mode);

	// Slot list doesn't change between Save / Load, but the confirm-button
	// label does — let BP re-render on the mode change.
}

void UDCSaveSlotPanelWidget::RefreshSlots()
{
	UDCGameInstance* GI = Cast<UDCGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (!GI)
	{
		return;
	}

	TArray<FDCSaveSlotInfo> Slots;
	GI->EnumerateSlots(Slots);
	BuildEntries(Slots);

	// Keep selection valid across refreshes (after Delete).
	if (SelectedSlotIndex != INDEX_NONE)
	{
		const bool bStillValid = Slots.ContainsByPredicate(
			[This = SelectedSlotIndex](const FDCSaveSlotInfo& S) { return S.SlotIndex == This; });
		if (!bStillValid)
		{
			SetSelectedSlot(INDEX_NONE);
		}
	}

	OnSlotsRefreshed();
}

void UDCSaveSlotPanelWidget::BuildEntries(const TArray<FDCSaveSlotInfo>& Slots)
{
	if (!SlotsContainer || !SlotEntryClass)
	{
		return;
	}

	SlotsContainer->ClearChildren();
	EntryWidgets.Reset(Slots.Num());

	for (const FDCSaveSlotInfo& Info : Slots)
	{
		UDCSaveSlotEntryWidget* Entry = CreateWidget<UDCSaveSlotEntryWidget>(this, SlotEntryClass);
		if (!Entry)
		{
			continue;
		}
		Entry->SetSlotInfo(Info);
		Entry->OnClicked.AddUniqueDynamic(this, &UDCSaveSlotPanelWidget::HandleEntryClicked);
		SlotsContainer->AddChild(Entry);
		EntryWidgets.Add(Entry);
	}
}

void UDCSaveSlotPanelWidget::HandleEntryClicked(UDCSaveSlotEntryWidget* Entry)
{
	if (!Entry)
	{
		return;
	}
	SetSelectedSlot(Entry->GetSlotIndex());
}

void UDCSaveSlotPanelWidget::SetSelectedSlot(int32 NewIndex)
{
	if (NewIndex == SelectedSlotIndex)
	{
		return;
	}
	SelectedSlotIndex = NewIndex;

	for (UDCSaveSlotEntryWidget* Entry : EntryWidgets)
	{
		if (Entry)
		{
			Entry->OnSelectionChanged(Entry->GetSlotIndex() == SelectedSlotIndex);
		}
	}

	OnSelectionChanged(SelectedSlotIndex);
}

void UDCSaveSlotPanelWidget::HandleConfirmClicked()
{
	if (SelectedSlotIndex == INDEX_NONE)
	{
		return;
	}
	PerformSaveOrLoad();
}

void UDCSaveSlotPanelWidget::HandleDeleteClicked()
{
	if (SelectedSlotIndex == INDEX_NONE)
	{
		return;
	}
	UDCGameInstance* GI = Cast<UDCGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (!GI)
	{
		return;
	}
	GI->DeleteSlot(SelectedSlotIndex);
	RefreshSlots();
}

void UDCSaveSlotPanelWidget::HandleCancelClicked()
{
	CloseMenu();
}

void UDCSaveSlotPanelWidget::PerformSaveOrLoad()
{
	UDCGameInstance* GI = Cast<UDCGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (!GI)
	{
		return;
	}

	FDCSaveSlotInfo Info;
	const bool bInfoValid = GI->GetSlotInfo(SelectedSlotIndex, Info);

	switch (Mode)
	{
	case EDCSaveSlotPanelMode::Save:
		// Empty slot → start a new run; occupied slot → overwrite in place.
		if (bInfoValid && Info.bExists)
		{
			// Make this slot current, then flush. NewGameInSlot would wipe the save.
			if (GI->GetCurrentSlotIndex() != SelectedSlotIndex)
			{
				GI->LoadSlot(SelectedSlotIndex);
			}
			GI->SaveGameToDisk();
		}
		else
		{
			GI->NewGameInSlot(SelectedSlotIndex);
			if (!NewGameLevelName.IsNone())
			{
				UGameplayStatics::OpenLevel(this, NewGameLevelName);
				return; // level travel tears down this widget
			}
			GI->SaveGameToDisk();
		}
		CloseMenu();
		break;

	case EDCSaveSlotPanelMode::Load:
		if (bInfoValid && Info.bExists && GI->LoadSlot(SelectedSlotIndex))
		{
			CloseMenu();
		}
		break;
	}
}
