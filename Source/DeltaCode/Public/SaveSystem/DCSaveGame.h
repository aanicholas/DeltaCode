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
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "Types/DCCoreTypes.h"
#include "DCSaveGame.generated.h"

class UDCQuestDefinition;

// ─── Per-quest save record ───────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EDCSavedQuestState : uint8
{
	Active     UMETA(DisplayName = "Active"),
	Completed  UMETA(DisplayName = "Completed"),
	Failed     UMETA(DisplayName = "Failed"),
};

USTRUCT(BlueprintType)
struct DELTACODE_API FDCSavedQuestState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save")
	FName QuestID = NAME_None;

	// Soft path so we can reload the definition without baking a hard reference into the save file.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save")
	TSoftObjectPtr<UDCQuestDefinition> Definition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save")
	EDCSavedQuestState State = EDCSavedQuestState::Active;

	// Per-objective progress (only meaningful while State == Active)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save")
	TMap<FName, int32> ObjectiveProgress;
};

// ─── Per-faction reputation save record ─────────────────────────────────────

USTRUCT(BlueprintType)
struct DELTACODE_API FDCSavedFactionReputation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Faction|Save")
	FGameplayTag FactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Faction|Save")
	int32 Reputation = 0;
};

// ─── Per-slot equipment save record ──────────────────────────────────────────

USTRUCT(BlueprintType)
struct DELTACODE_API FDCSavedEquipmentSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save")
	EDCEquipmentSlot Slot = EDCEquipmentSlot::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save")
	FDCItemHandle ItemHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save")
	int32 CurrentDurability = 0;
};

/**
 * Save game payload for DeltaCode.
 *
 * The game instance orchestrates capture/restore. Components and subsystems
 * expose Capture/Restore methods that the game instance calls; nothing in
 * gameplay code should reach into this struct directly.
 *
 * [INPUT]  From: UDCGameInstance::SaveGameToDisk — captured from live state
 * [OUTPUT] To:   UDCGameInstance::ApplySaveToPlayer — restored on possess
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API UDCSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UDCSaveGame();

	// ── Slot metadata ───────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save")
	FString PlayerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save")
	float TotalPlayTimeSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save")
	FDateTime LastSaveTimestamp;

	/** Map / level name the save was taken on, for slot-panel display. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save")
	FString LevelName;

	// ── Inventory ───────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save|Inventory")
	TArray<FDCInventoryEntry> InventoryEntries;

	// ── Narrative / world state ─────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save|Narrative")
	FGameplayTagContainer NarrativeTags;

	// ── Progression ─────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save|Progression")
	int32 PlayerLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save|Progression")
	int32 PlayerXP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save|Progression")
	int32 UnspentSkillPoints = 0;

	// ── Skill tree ──────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save|Progression")
	TArray<FName> UnlockedSkills;

	// ── Equipment ───────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save|Equipment")
	TArray<FDCSavedEquipmentSlot> EquippedSlots;

	// ── Quests ──────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save|Quests")
	TArray<FDCSavedQuestState> Quests;

	// ── Faction reputation ──────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Save|Factions")
	TArray<FDCSavedFactionReputation> FactionReputation;

	// ── Dirty flag ──────────────────────────────────────────────────────────

	void MarkDirty() { bIsDirty = true; }
	bool IsDirty() const { return bIsDirty; }
	void ClearDirty() { bIsDirty = false; }

	/** True when this save was hydrated from disk (vs. freshly created). */
	bool WasLoadedFromDisk() const { return bLoadedFromDisk; }
	void SetLoadedFromDisk(bool bValue) { bLoadedFromDisk = bValue; }

private:
	bool bIsDirty = false;
	bool bLoadedFromDisk = false;
};
