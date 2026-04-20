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
#include "UI/DCSaveSlotEntryWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Internationalization/Text.h"

void UDCSaveSlotEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SelectButton)
	{
		SelectButton->OnClicked.AddUniqueDynamic(this,
			&UDCSaveSlotEntryWidget::HandleSelectClicked);
	}
}

void UDCSaveSlotEntryWidget::SetSlotInfo(const FDCSaveSlotInfo& InInfo)
{
	SlotInfo = InInfo;
	RefreshNativeLabels();
	OnSlotInfoApplied();
}

void UDCSaveSlotEntryWidget::HandleSelectClicked()
{
	OnClicked.Broadcast(this);
}

void UDCSaveSlotEntryWidget::RefreshNativeLabels()
{
	if (SlotIndexText)
	{
		SlotIndexText->SetText(FText::AsNumber(SlotInfo.SlotIndex));
	}

	// Empty slots get a localized placeholder — populated slots get live data.
	static const FText EmptyLabel = NSLOCTEXT("DeltaCode", "EmptySaveSlot", "— Empty —");

	if (PlayerNameText)
	{
		PlayerNameText->SetText(SlotInfo.bExists
			? FText::FromString(SlotInfo.PlayerName)
			: EmptyLabel);
	}
	if (LevelNameText)
	{
		LevelNameText->SetText(SlotInfo.bExists
			? FText::FromString(SlotInfo.LevelName)
			: FText::GetEmpty());
	}
	if (PlayerLevelText)
	{
		PlayerLevelText->SetText(SlotInfo.bExists
			? FText::AsNumber(SlotInfo.PlayerLevel)
			: FText::GetEmpty());
	}
	if (PlayTimeText)
	{
		if (SlotInfo.bExists)
		{
			const int32 TotalSeconds = FMath::FloorToInt(SlotInfo.TotalPlayTimeSeconds);
			const int32 Hours   = TotalSeconds / 3600;
			const int32 Minutes = (TotalSeconds % 3600) / 60;
			PlayTimeText->SetText(FText::FromString(
				FString::Printf(TEXT("%02d:%02d"), Hours, Minutes)));
		}
		else
		{
			PlayTimeText->SetText(FText::GetEmpty());
		}
	}
	if (TimestampText)
	{
		TimestampText->SetText(SlotInfo.bExists
			? FText::AsDateTime(SlotInfo.LastSaveTimestamp)
			: FText::GetEmpty());
	}
}
