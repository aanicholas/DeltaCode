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
#include "Engine/GameInstance.h"
#include "SaveSystem/DCSaveSlotInfo.h"
#include "DCGameInstance.generated.h"

class UDCSaveGame;
class ADCPlayerControllerBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnSaveCompleted, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnLoadCompleted, int32, SlotIndex);

/**
 * Base game instance for DeltaCode games.
 * Persists across level loads. Hosts subsystems and save game data.
 *
 * Slot model: slots are indexed 0..MaxSaveSlots-1. Slot name on disk is
 * SaveSlotPrefix + "_NN". The active slot (CurrentSlotIndex) is what
 * SaveGameToDisk writes to and what Init auto-loads if present.
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API UDCGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UDCGameInstance();

	virtual void Init() override;
	virtual void Shutdown() override;

	// ── Current save payload ────────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Save")
	UDCSaveGame* GetSaveGame() const { return CurrentSaveGame; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Save")
	int32 GetCurrentSlotIndex() const { return CurrentSlotIndex; }

	/** Writes live player state into CurrentSaveGame and flushes to disk. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Save")
	void SaveGameToDisk();

	/**
	 * Load the save from the active slot if it exists, otherwise create an empty
	 * payload. Runs at Init; re-runnable for slot switches via LoadSlot.
	 */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Save")
	void LoadGameFromDisk();

	// ── Slot management ─────────────────────────────────────────────────────

	/** Create a fresh save in the given slot and make it current. Wipes any prior save in that slot. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Save")
	void NewGameInSlot(int32 SlotIndex);

	/** Load the save in SlotIndex and make it current. Returns false if no save exists there. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Save")
	bool LoadSlot(int32 SlotIndex);

	/** Remove the save in SlotIndex. If it was current, the current save resets to an empty payload. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Save")
	bool DeleteSlot(int32 SlotIndex);

	/** Populate OutSlots with FDCSaveSlotInfo for every 0..MaxSaveSlots-1 (existing + empty). */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Save")
	void EnumerateSlots(TArray<FDCSaveSlotInfo>& OutSlots) const;

	/** Lightweight metadata for a single slot. Returns false on invalid index. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Save")
	bool GetSlotInfo(int32 SlotIndex, FDCSaveSlotInfo& OutInfo) const;

	// ── Capture / restore (called by checkpoint volumes + PC::OnPossess) ────

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Save")
	void CaptureFromPlayer(ADCPlayerControllerBase* PC);

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Save")
	void ApplySaveToPlayer(ADCPlayerControllerBase* PC);

	// ── Configuration ───────────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Save")
	FString SaveSlotPrefix = TEXT("DeltaCodeSave");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Save",
	          meta = (ClampMin = "1", ClampMax = "99"))
	int32 MaxSaveSlots = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Save")
	int32 SaveUserIndex = 0;

	/** Slot that Init auto-loads on first launch. -1 = no auto-load; start at main menu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Save")
	int32 DefaultAutoLoadSlot = 0;

	// ── Events ──────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Save")
	FDCOnSaveCompleted OnSaveCompleted;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Save")
	FDCOnLoadCompleted OnLoadCompleted;

protected:
	/** Build the on-disk slot name from an index. */
	FString MakeSlotName(int32 SlotIndex) const;

	/** Load a save without making it current (used by EnumerateSlots for projection). */
	UDCSaveGame* LoadSaveForSlot(int32 SlotIndex) const;

	/** Extract display metadata from a loaded save. */
	void FillSlotInfo(int32 SlotIndex, const UDCSaveGame* Save, FDCSaveSlotInfo& OutInfo) const;

	/** Accumulate this session's real-time into TotalPlayTimeSeconds and reset the timer. */
	void AccumulatePlayTime();

private:
	UPROPERTY()
	TObjectPtr<UDCSaveGame> CurrentSaveGame;

	UPROPERTY()
	int32 CurrentSlotIndex = -1;

	// Real-time seconds at the start of the current session segment, used to
	// accumulate play-time into CurrentSaveGame on save / slot-switch / shutdown.
	double SessionStartSeconds = 0.0;
};
