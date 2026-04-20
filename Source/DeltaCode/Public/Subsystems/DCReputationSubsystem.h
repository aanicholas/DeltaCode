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
#include "GameplayTagContainer.h"
#include "Data/DCFactionDefinition.h"
#include "SaveSystem/DCSaveGame.h"
#include "DCReputationSubsystem.generated.h"

class UDCQuestSubsystem;
class UDCQuestDefinition;
struct FDCActiveQuestState;

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDCOnReputationChanged,
	FGameplayTag, FactionTag, int32, NewValue, int32, Delta);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCOnFactionStanceChanged,
	FGameplayTag, FactionTag, EDCFactionStance, NewStance);

/**
 * Central authority for per-faction player reputation.
 *
 * Observes but does not modify the quest or dialogue systems. It binds to
 * UDCQuestSubsystem's OnQuestCompleted / OnQuestFailed delegates to emit
 * observation hooks (OnQuestCompletedForReputation / OnQuestFailedForReputation)
 * that Blueprint / project code subscribe to in order to map quest outcomes
 * onto reputation deltas. The mapping lives in user content — never hard-coded
 * here — so adding a new faction does not require editing quest code.
 *
 * [INPUT]  From: UDCQuestSubsystem::OnQuestCompleted   → observation broadcast
 * [INPUT]  From: UDCQuestSubsystem::OnQuestFailed      → observation broadcast
 * [INPUT]  From: UDCFactionComponent::BeginPlay        → RegisterFaction (seeding)
 * [INPUT]  From: Blueprint / gameplay code             → ModifyReputation
 * [OUTPUT] To:   Blueprint listeners                   → OnReputationChanged
 * [OUTPUT] To:   UDCFactionComponent / enemy AI        → stance queries
 *
 * [DEPENDS ON] UDCFactionDefinition — thresholds + initial values
 * [DEPENDS ON] UDCQuestSubsystem    — observation only, never mutated
 */
UCLASS()
class DELTACODE_API UDCReputationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Faction registration ────────────────────────────────────────────────

	/**
	 * Register a faction with the subsystem. Seeds the player's reputation from
	 * the definition's InitialPlayerReputation the first time a given tag is
	 * registered. Safe to call repeatedly — later calls are no-ops.
	 */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Reputation")
	void RegisterFaction(UDCFactionDefinition* Faction);

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Reputation")
	UDCFactionDefinition* FindFactionByTag(const FGameplayTag& FactionTag) const;

	// ── Reputation accessors ────────────────────────────────────────────────

	/** Get current player reputation with FactionTag. Returns 0 if unknown. */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Reputation")
	int32 GetReputation(const FGameplayTag& FactionTag) const;

	/** Add Delta to player reputation with FactionTag, clamped by the definition. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Reputation")
	void ModifyReputation(const FGameplayTag& FactionTag, int32 Delta);

	/** Replace player reputation with FactionTag, clamped by the definition. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Reputation")
	void SetReputation(const FGameplayTag& FactionTag, int32 Value);

	/** Resolve player reputation into the faction's configured stance bands. */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Reputation")
	EDCFactionStance GetPlayerStance(const FGameplayTag& FactionTag) const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Reputation")
	bool IsPlayerHostileWith(const FGameplayTag& FactionTag) const;

	// ── Save / restore ──────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Reputation|Save")
	void CaptureForSave(TArray<FDCSavedFactionReputation>& OutRecords) const;

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Reputation|Save")
	void RestoreFromSave(const TArray<FDCSavedFactionReputation>& InRecords);

	// ── Delegates ───────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Reputation")
	FDCOnReputationChanged OnReputationChanged;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Reputation")
	FDCOnFactionStanceChanged OnStanceChanged;

	// ── Observation hooks (quest / dialogue) ────────────────────────────────
	//
	// These fire after the corresponding quest-subsystem event, so project
	// Blueprints can map the outcome onto reputation deltas without the
	// reputation subsystem or quest subsystem knowing about each other.

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnQuestCompletedForReputation,
		UDCQuestDefinition*, QuestDefinition);

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Reputation|Observation")
	FDCOnQuestCompletedForReputation OnQuestCompletedForReputation;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Reputation|Observation")
	FDCOnQuestCompletedForReputation OnQuestFailedForReputation;

private:
	/** Apply clamping from the faction definition (or pass through when unknown). */
	int32 ClampReputation(const FGameplayTag& FactionTag, int32 Value) const;

	/** Update internal map, fire OnReputationChanged, and diff stance to fire OnStanceChanged. */
	void ApplyReputationInternal(const FGameplayTag& FactionTag,
	                             int32 NewValue, int32 OldValue);

	UFUNCTION()
	void HandleQuestCompleted(const FDCActiveQuestState& State);

	UFUNCTION()
	void HandleQuestFailed(const FDCActiveQuestState& State);

	void BindToQuestSubsystem();
	void UnbindFromQuestSubsystem();

	// Registered faction definitions keyed by FactionTag
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UDCFactionDefinition>> RegisteredFactions;

	// Player reputation keyed by FactionTag
	UPROPERTY(Transient)
	TMap<FGameplayTag, int32> FactionReputation;

	TWeakObjectPtr<UDCQuestSubsystem> BoundQuestSubsystem;
};
