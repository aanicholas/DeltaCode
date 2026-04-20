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
#include "Actors/DCGameInstance.h"
#include "Actors/DCCharacterBase.h"
#include "Actors/DCPlayerControllerBase.h"
#include "Components/DCEquipmentComponent.h"
#include "Components/DCInventoryComponent.h"
#include "Components/DCNarrativeStateComponent.h"
#include "Components/DCProgressionComponent.h"
#include "Components/DCSkillTreeComponent.h"
#include "SaveSystem/DCSaveGame.h"
#include "Subsystems/DCQuestSubsystem.h"
#include "Subsystems/DCReputationSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UDCGameInstance::UDCGameInstance()
{
}

void UDCGameInstance::Init()
{
	Super::Init();

	SessionStartSeconds = FPlatformTime::Seconds();

	if (DefaultAutoLoadSlot >= 0 && DefaultAutoLoadSlot < MaxSaveSlots)
	{
		CurrentSlotIndex = DefaultAutoLoadSlot;
		LoadGameFromDisk();
	}
	else
	{
		// No auto-load — spawn an empty payload so the slot-select UI has something
		// to target before the user picks a slot.
		CurrentSaveGame = Cast<UDCSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UDCSaveGame::StaticClass()));
		if (CurrentSaveGame)
		{
			CurrentSaveGame->SetLoadedFromDisk(false);
		}
	}
}

void UDCGameInstance::Shutdown()
{
	// Fold any remaining wall-clock into the payload so a crash-free exit is accurate.
	AccumulatePlayTime();
	Super::Shutdown();
}

// ─── Slot name helpers ───────────────────────────────────────────────────────

FString UDCGameInstance::MakeSlotName(int32 SlotIndex) const
{
	return FString::Printf(TEXT("%s_%02d"), *SaveSlotPrefix, SlotIndex);
}

// ─── Save / load (current slot) ──────────────────────────────────────────────

void UDCGameInstance::SaveGameToDisk()
{
	if (!CurrentSaveGame || CurrentSlotIndex < 0)
	{
		return;
	}

	// Harvest current player state into the payload before writing to disk.
	if (ADCPlayerControllerBase* PC = Cast<ADCPlayerControllerBase>(
		GetFirstLocalPlayerController()))
	{
		CaptureFromPlayer(PC);
	}

	AccumulatePlayTime();

	const FString SlotName = MakeSlotName(CurrentSlotIndex);
	UGameplayStatics::SaveGameToSlot(CurrentSaveGame, SlotName, SaveUserIndex);
	CurrentSaveGame->ClearDirty();

	OnSaveCompleted.Broadcast(CurrentSlotIndex);
}

void UDCGameInstance::LoadGameFromDisk()
{
	if (CurrentSlotIndex < 0)
	{
		return;
	}

	const FString SlotName = MakeSlotName(CurrentSlotIndex);

	bool bLoaded = false;
	if (UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex))
	{
		CurrentSaveGame = Cast<UDCSaveGame>(
			UGameplayStatics::LoadGameFromSlot(SlotName, SaveUserIndex));
		bLoaded = (CurrentSaveGame != nullptr);
	}

	if (!CurrentSaveGame)
	{
		CurrentSaveGame = Cast<UDCSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UDCSaveGame::StaticClass()));
	}

	if (CurrentSaveGame)
	{
		CurrentSaveGame->SetLoadedFromDisk(bLoaded);
	}

	SessionStartSeconds = FPlatformTime::Seconds();

	if (bLoaded)
	{
		OnLoadCompleted.Broadcast(CurrentSlotIndex);
	}
}

// ─── Slot management ─────────────────────────────────────────────────────────

void UDCGameInstance::NewGameInSlot(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= MaxSaveSlots)
	{
		return;
	}

	// Fold any session time into the outgoing slot's payload so it isn't lost.
	AccumulatePlayTime();

	CurrentSlotIndex = SlotIndex;
	CurrentSaveGame = Cast<UDCSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UDCSaveGame::StaticClass()));

	if (CurrentSaveGame)
	{
		CurrentSaveGame->SetLoadedFromDisk(false);
	}

	SessionStartSeconds = FPlatformTime::Seconds();
}

bool UDCGameInstance::LoadSlot(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= MaxSaveSlots)
	{
		return false;
	}

	const FString SlotName = MakeSlotName(SlotIndex);
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex))
	{
		return false;
	}

	// Fold outgoing session time first; loading a different slot hard-resets state.
	AccumulatePlayTime();

	CurrentSlotIndex = SlotIndex;
	LoadGameFromDisk();
	return CurrentSaveGame != nullptr;
}

bool UDCGameInstance::DeleteSlot(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= MaxSaveSlots)
	{
		return false;
	}

	const FString SlotName = MakeSlotName(SlotIndex);
	const bool bDeleted = UGameplayStatics::DeleteGameInSlot(SlotName, SaveUserIndex);

	if (bDeleted && SlotIndex == CurrentSlotIndex)
	{
		// Reset current payload to empty so any active play session doesn't keep
		// writing into what looks-to-the-user like a deleted slot.
		CurrentSaveGame = Cast<UDCSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UDCSaveGame::StaticClass()));
		if (CurrentSaveGame)
		{
			CurrentSaveGame->SetLoadedFromDisk(false);
		}
		SessionStartSeconds = FPlatformTime::Seconds();
	}

	return bDeleted;
}

void UDCGameInstance::EnumerateSlots(TArray<FDCSaveSlotInfo>& OutSlots) const
{
	OutSlots.Reset(MaxSaveSlots);
	for (int32 Index = 0; Index < MaxSaveSlots; ++Index)
	{
		FDCSaveSlotInfo Info;
		GetSlotInfo(Index, Info);
		OutSlots.Add(MoveTemp(Info));
	}
}

bool UDCGameInstance::GetSlotInfo(int32 SlotIndex, FDCSaveSlotInfo& OutInfo) const
{
	if (SlotIndex < 0 || SlotIndex >= MaxSaveSlots)
	{
		return false;
	}

	OutInfo = FDCSaveSlotInfo();
	OutInfo.SlotIndex = SlotIndex;
	OutInfo.SlotName = MakeSlotName(SlotIndex);

	const UDCSaveGame* Save = (SlotIndex == CurrentSlotIndex && CurrentSaveGame && CurrentSaveGame->WasLoadedFromDisk())
		? CurrentSaveGame.Get()
		: LoadSaveForSlot(SlotIndex);

	FillSlotInfo(SlotIndex, Save, OutInfo);
	return true;
}

UDCSaveGame* UDCGameInstance::LoadSaveForSlot(int32 SlotIndex) const
{
	const FString SlotName = MakeSlotName(SlotIndex);
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex))
	{
		return nullptr;
	}
	return Cast<UDCSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, SaveUserIndex));
}

void UDCGameInstance::FillSlotInfo(int32 SlotIndex, const UDCSaveGame* Save,
                                   FDCSaveSlotInfo& OutInfo) const
{
	if (!Save)
	{
		OutInfo.bExists = false;
		return;
	}
	OutInfo.bExists = true;
	OutInfo.PlayerName = Save->PlayerName;
	OutInfo.PlayerLevel = Save->PlayerLevel;
	OutInfo.TotalPlayTimeSeconds = Save->TotalPlayTimeSeconds;
	OutInfo.LastSaveTimestamp = Save->LastSaveTimestamp;
	OutInfo.LevelName = Save->LevelName;
}

// ─── Play time ───────────────────────────────────────────────────────────────

void UDCGameInstance::AccumulatePlayTime()
{
	const double Now = FPlatformTime::Seconds();
	const double Delta = Now - SessionStartSeconds;
	if (CurrentSaveGame && Delta > 0.0)
	{
		CurrentSaveGame->TotalPlayTimeSeconds += static_cast<float>(Delta);
	}
	SessionStartSeconds = Now;
}

// ─── Capture / apply ─────────────────────────────────────────────────────────

void UDCGameInstance::CaptureFromPlayer(ADCPlayerControllerBase* PC)
{
	if (!CurrentSaveGame || !PC)
	{
		return;
	}

	if (UDCInventoryComponent* Inv = PC->GetInventoryComponent())
	{
		CurrentSaveGame->InventoryEntries = Inv->GetEntries();
	}
	if (UDCNarrativeStateComponent* Narrative = PC->GetNarrativeState())
	{
		CurrentSaveGame->NarrativeTags = Narrative->GetTags();
	}
	if (UDCProgressionComponent* Prog = PC->GetProgressionComponent())
	{
		Prog->CaptureForSave(CurrentSaveGame->PlayerLevel,
			CurrentSaveGame->PlayerXP,
			CurrentSaveGame->UnspentSkillPoints);
	}
	if (UDCSkillTreeComponent* Skills = PC->GetSkillTreeComponent())
	{
		Skills->CaptureForSave(CurrentSaveGame->UnlockedSkills);
	}
	if (ADCCharacterBase* Char = Cast<ADCCharacterBase>(PC->GetPawn()))
	{
		if (UDCEquipmentComponent* Equip = Char->GetEquipmentComponent())
		{
			Equip->CaptureForSave(CurrentSaveGame->EquippedSlots);
		}
	}
	if (UDCQuestSubsystem* Quests = GetSubsystem<UDCQuestSubsystem>())
	{
		Quests->CaptureForSave(CurrentSaveGame->Quests);
	}
	if (UDCReputationSubsystem* Reputation = GetSubsystem<UDCReputationSubsystem>())
	{
		Reputation->CaptureForSave(CurrentSaveGame->FactionReputation);
	}

	if (const UWorld* World = PC->GetWorld())
	{
		CurrentSaveGame->LevelName = World->GetMapName();
	}
	CurrentSaveGame->LastSaveTimestamp = FDateTime::Now();
	CurrentSaveGame->MarkDirty();
}

void UDCGameInstance::ApplySaveToPlayer(ADCPlayerControllerBase* PC)
{
	// Guard against stomping a fresh-play session with the default-constructed payload.
	if (!CurrentSaveGame || !PC || !CurrentSaveGame->WasLoadedFromDisk())
	{
		return;
	}

	if (UDCInventoryComponent* Inv = PC->GetInventoryComponent())
	{
		Inv->RestoreFromSave(CurrentSaveGame->InventoryEntries);
	}
	if (UDCNarrativeStateComponent* Narrative = PC->GetNarrativeState())
	{
		Narrative->SetTagsFromSave(CurrentSaveGame->NarrativeTags);
	}
	if (UDCProgressionComponent* Prog = PC->GetProgressionComponent())
	{
		Prog->RestoreFromSave(CurrentSaveGame->PlayerLevel,
			CurrentSaveGame->PlayerXP,
			CurrentSaveGame->UnspentSkillPoints);
	}
	// Restore order: equipment first (baseline), then skills (stack on top).
	if (ADCCharacterBase* Char = Cast<ADCCharacterBase>(PC->GetPawn()))
	{
		if (UDCEquipmentComponent* Equip = Char->GetEquipmentComponent())
		{
			Equip->RestoreFromSave(CurrentSaveGame->EquippedSlots);
		}
	}
	if (UDCSkillTreeComponent* Skills = PC->GetSkillTreeComponent())
	{
		Skills->RestoreFromSave(CurrentSaveGame->UnlockedSkills);
	}
	if (UDCQuestSubsystem* Quests = GetSubsystem<UDCQuestSubsystem>())
	{
		Quests->RestoreFromSave(CurrentSaveGame->Quests);
	}
	// Reputation restores after quests so any reputation deltas driven by quest
	// completion events during RestoreFromSave have already settled.
	if (UDCReputationSubsystem* Reputation = GetSubsystem<UDCReputationSubsystem>())
	{
		Reputation->RestoreFromSave(CurrentSaveGame->FactionReputation);
	}
}
