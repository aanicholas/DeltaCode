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
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/DCCoreTypes.h"
#include "Types/DCDataDefinitions.h"
#include "SaveSystem/DCSaveGame.h"
#include "DCQuestSubsystem.generated.h"

class UDCQuestDefinition;
class UDCItemDefinitionRegistry;
class ADCPlayerControllerBase;
class UDCInventoryComponent;
class UDCNarrativeStateComponent;

// ─── Runtime State ───────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EDCQuestState : uint8
{
	Inactive   UMETA(DisplayName = "Inactive"),
	Active     UMETA(DisplayName = "Active"),
	Completed  UMETA(DisplayName = "Completed"),
	Failed     UMETA(DisplayName = "Failed"),
};

USTRUCT(BlueprintType)
struct DELTACODE_API FDCActiveQuestState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Quest")
	FName QuestID = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Quest")
	TObjectPtr<UDCQuestDefinition> Definition;

	// ObjectiveID → current progress count
	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Quest")
	TMap<FName, int32> ObjectiveProgress;

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Quest")
	EDCQuestState State = EDCQuestState::Inactive;

	int32 GetProgress(FName ObjectiveID) const
	{
		const int32* Found = ObjectiveProgress.Find(ObjectiveID);
		return Found ? *Found : 0;
	}
};

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnQuestAccepted,
	const FDCActiveQuestState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDCOnObjectiveProgress,
	FName, QuestID, FName, ObjectiveID, int32, NewProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCOnObjectiveCompleted,
	FName, QuestID, FName, ObjectiveID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnQuestCompleted,
	const FDCActiveQuestState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnQuestFailed,
	const FDCActiveQuestState&, State);

/**
 * Central authority for quest state. Holds the set of Active / Completed /
 * Failed quests for the local player and resolves objective progress from
 * gameplay events (kills, pickups, dialogue endings, narrative tags).
 *
 * [INPUT]  From: Dialogue responses / quest-giver UI    → AcceptQuest / CompleteQuest
 * [INPUT]  From: ADCEnemyBase::HandleDeath              → NotifyEnemyKilled
 * [INPUT]  From: UDCInventoryComponent::OnItemAdded     → auto-subscribed
 * [INPUT]  From: UDCNarrativeStateComponent::OnTagsChanged → auto-subscribed (Custom obj)
 * [INPUT]  From: UDCDialogueComponent::OnConversationEnded → NotifyNPCTalked
 * [OUTPUT] To:   UDCInventoryComponent::AddItem  — on completion reward grant
 * [OUTPUT] To:   UDCNarrativeStateComponent::AddTags — CompletionTags / reward tags
 * [OUTPUT] To:   W_DC_QuestLog — binds OnQuestAccepted / OnObjectiveProgress etc.
 *
 * [DEPENDS ON] UDCItemDefinitionRegistry — rewards referenced by handle
 * [DEPENDS ON] UDCInventoryComponent     — pickup → objective progress
 * [DEPENDS ON] UDCNarrativeStateComponent — prerequisite / blocking tag eval
 */
UCLASS()
class DELTACODE_API UDCQuestSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Quest lifecycle ─────────────────────────────────────────────────────

	/** Accept a quest for the given player. Fails if already active/completed or blocked by prereq/block tags. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Quest")
	bool AcceptQuest(UDCQuestDefinition* Quest, ADCPlayerControllerBase* Player);

	/** Mark a quest complete and grant its rewards. Fails if not all objectives are done and bForce is false. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Quest")
	bool CompleteQuest(FName QuestID, bool bForce = false);

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Quest")
	bool FailQuest(FName QuestID);

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Quest")
	bool AbandonQuest(FName QuestID);

	// ── Gameplay event hooks ────────────────────────────────────────────────

	/** Call from enemy death; resolves KillTarget objectives whose TargetClass matches. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Quest|Events")
	void NotifyEnemyKilled(AActor* Target, AActor* KilledBy);

	/** Call from dialogue end; resolves TalkToNPC objectives referencing the NPC ID. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Quest|Events")
	void NotifyNPCTalked(FName NPCID);

	/** Call when the player interacts with a non-NPC object; resolves InteractWithObject objectives. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Quest|Events")
	void NotifyInteracted(AActor* Target);

	/** Call when the player reaches a ReachLocation trigger (trigger volume passes its tag here). */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Quest|Events")
	void NotifyLocationReached(FGameplayTag LocationTag);

	// ── Query ───────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Quest")
	bool IsQuestActive(FName QuestID) const { return ActiveQuests.Contains(QuestID); }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Quest")
	bool IsQuestCompleted(FName QuestID) const { return CompletedQuestIDs.Contains(QuestID); }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Quest")
	bool IsQuestFailed(FName QuestID) const { return FailedQuestIDs.Contains(QuestID); }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Quest")
	bool GetQuestState(FName QuestID, FDCActiveQuestState& OutState) const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Quest")
	void GetActiveQuests(TArray<FDCActiveQuestState>& OutQuests) const;

	// ── Save / restore ──────────────────────────────────────────────────────

	/** Copy active / completed / failed quest state for UDCSaveGame. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Quest|Save")
	void CaptureForSave(TArray<FDCSavedQuestState>& OutQuests) const;

	/** Restore quest state from UDCSaveGame. Rebuilds active map + completed/failed sets. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Quest|Save")
	void RestoreFromSave(const TArray<FDCSavedQuestState>& SavedQuests);

	// ── Delegates ───────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Quest")
	FDCOnQuestAccepted OnQuestAccepted;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Quest")
	FDCOnObjectiveProgress OnObjectiveProgress;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Quest")
	FDCOnObjectiveCompleted OnObjectiveCompleted;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Quest")
	FDCOnQuestCompleted OnQuestCompleted;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Quest")
	FDCOnQuestFailed OnQuestFailed;

private:
	// Active state — one entry per in-progress quest, keyed by QuestID
	UPROPERTY(Transient)
	TMap<FName, FDCActiveQuestState> ActiveQuests;

	UPROPERTY(Transient)
	TSet<FName> CompletedQuestIDs;

	UPROPERTY(Transient)
	TSet<FName> FailedQuestIDs;

	TWeakObjectPtr<ADCPlayerControllerBase> BoundPlayer;

	// ── Internal progress ───────────────────────────────────────────────────

	// Bump a specific objective; fires Progress + Completed deltas; tries to auto-complete
	void AdvanceObjective(FName QuestID, FName ObjectiveID, int32 Delta);

	// Check if every non-hidden objective has reached its required count
	bool AreAllObjectivesComplete(const FDCActiveQuestState& State) const;

	// Grant items / XP / tags / currency from the definition's reward struct
	void GrantRewards(UDCQuestDefinition* Quest);

	// Resolve a handle from the registry — helper for reward grants
	UDCItemDefinitionRegistry* GetItemRegistry() const;

	// ── Listener wiring ─────────────────────────────────────────────────────

	// Attach to the player's inventory + narrative state. Safe to call repeatedly.
	void BindToPlayer(ADCPlayerControllerBase* Player);
	void UnbindFromPlayer();

	UFUNCTION()
	void HandleInventoryItemAdded(FDCItemHandle ItemHandle, int32 AmountAdded, int32 NewTotal);

	UFUNCTION()
	void HandleNarrativeTagsChanged(const FGameplayTagContainer& Added,
	                                const FGameplayTagContainer& Removed);
};
