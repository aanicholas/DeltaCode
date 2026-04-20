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
#include "Subsystems/DCQuestSubsystem.h"
#include "Subsystems/DCItemDefinitionRegistry.h"
#include "Data/DCQuestDefinition.h"
#include "Data/DCItemDefinition.h"
#include "Actors/DCPlayerControllerBase.h"
#include "Components/DCInventoryComponent.h"
#include "Components/DCNarrativeStateComponent.h"
#include "Components/DCProgressionComponent.h"
#include "Engine/GameInstance.h"

void UDCQuestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UDCQuestSubsystem::Deinitialize()
{
	UnbindFromPlayer();
	ActiveQuests.Empty();
	CompletedQuestIDs.Empty();
	FailedQuestIDs.Empty();
	Super::Deinitialize();
}

UDCItemDefinitionRegistry* UDCQuestSubsystem::GetItemRegistry() const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UDCItemDefinitionRegistry>();
	}
	return nullptr;
}

// ─── Quest lifecycle ─────────────────────────────────────────────────────────

bool UDCQuestSubsystem::AcceptQuest(UDCQuestDefinition* Quest,
                                    ADCPlayerControllerBase* Player)
{
	if (!Quest || Quest->QuestID.IsNone() || !Player)
	{
		return false;
	}

	const FName QuestID = Quest->QuestID;
	if (ActiveQuests.Contains(QuestID) || CompletedQuestIDs.Contains(QuestID))
	{
		return false;
	}

	// Evaluate prereq / blocking tags against the player's narrative state
	if (UDCNarrativeStateComponent* Narrative = Player->GetNarrativeState())
	{
		if (!Quest->PrerequisiteTags.IsEmpty() &&
		    !Narrative->HasAllTags(Quest->PrerequisiteTags))
		{
			return false;
		}
		if (!Quest->BlockingTags.IsEmpty() &&
		    Narrative->HasAnyTag(Quest->BlockingTags))
		{
			return false;
		}
	}

	FDCActiveQuestState NewState;
	NewState.QuestID = QuestID;
	NewState.Definition = Quest;
	NewState.State = EDCQuestState::Active;
	for (const FDCQuestObjective& Obj : Quest->Objectives)
	{
		NewState.ObjectiveProgress.Add(Obj.ObjectiveID, 0);
	}
	ActiveQuests.Add(QuestID, NewState);

	BindToPlayer(Player);

	OnQuestAccepted.Broadcast(NewState);

	// Seed CollectItem objectives from what the player already holds
	if (UDCInventoryComponent* Inv = Player->GetInventoryComponent())
	{
		for (const FDCQuestObjective& Obj : Quest->Objectives)
		{
			if (Obj.ObjectiveType == EDCQuestObjectiveType::CollectItem &&
			    Obj.TargetItem.IsValid())
			{
				const int32 Held = Inv->GetItemCount(Obj.TargetItem);
				if (Held > 0)
				{
					AdvanceObjective(QuestID, Obj.ObjectiveID, Held);
				}
			}
		}
	}
	return true;
}

bool UDCQuestSubsystem::CompleteQuest(FName QuestID, bool bForce)
{
	FDCActiveQuestState* State = ActiveQuests.Find(QuestID);
	if (!State || State->State != EDCQuestState::Active)
	{
		return false;
	}
	if (!bForce && !AreAllObjectivesComplete(*State))
	{
		return false;
	}

	State->State = EDCQuestState::Completed;

	UDCQuestDefinition* Def = State->Definition;
	GrantRewards(Def);

	// Grant CompletionTags to the player
	if (Def && !Def->CompletionTags.IsEmpty())
	{
		if (ADCPlayerControllerBase* PC = BoundPlayer.Get())
		{
			if (UDCNarrativeStateComponent* Narrative = PC->GetNarrativeState())
			{
				Narrative->AddTags(Def->CompletionTags);
			}
		}
	}

	const FDCActiveQuestState Copy = *State;
	CompletedQuestIDs.Add(QuestID);
	OnQuestCompleted.Broadcast(Copy);
	ActiveQuests.Remove(QuestID);

	// Chain follow-up quest automatically if configured
	if (Def && !Def->FollowUpQuest.IsNull())
	{
		if (UDCQuestDefinition* NextQuest = Def->FollowUpQuest.LoadSynchronous())
		{
			if (ADCPlayerControllerBase* PC = BoundPlayer.Get())
			{
				AcceptQuest(NextQuest, PC);
			}
		}
	}
	return true;
}

bool UDCQuestSubsystem::FailQuest(FName QuestID)
{
	FDCActiveQuestState* State = ActiveQuests.Find(QuestID);
	if (!State || State->State != EDCQuestState::Active)
	{
		return false;
	}
	State->State = EDCQuestState::Failed;

	const FDCActiveQuestState Copy = *State;
	FailedQuestIDs.Add(QuestID);
	OnQuestFailed.Broadcast(Copy);
	ActiveQuests.Remove(QuestID);
	return true;
}

bool UDCQuestSubsystem::AbandonQuest(FName QuestID)
{
	if (!ActiveQuests.Contains(QuestID))
	{
		return false;
	}
	ActiveQuests.Remove(QuestID);
	return true;
}

// ─── Rewards ─────────────────────────────────────────────────────────────────

void UDCQuestSubsystem::GrantRewards(UDCQuestDefinition* Quest)
{
	if (!Quest)
	{
		return;
	}
	ADCPlayerControllerBase* PC = BoundPlayer.Get();
	if (!PC)
	{
		return;
	}

	UDCInventoryComponent* Inv = PC->GetInventoryComponent();
	UDCNarrativeStateComponent* Narrative = PC->GetNarrativeState();
	UDCProgressionComponent* Progression = PC->GetProgressionComponent();

	for (const FDCQuestReward& Reward : Quest->Rewards)
	{
		if (Inv && Reward.RewardItem.IsValid() && Reward.RewardItemCount > 0)
		{
			int32 AmountAdded = 0;
			Inv->AddItem(Reward.RewardItem, Reward.RewardItemCount, AmountAdded);
		}

		if (Inv && Reward.CurrencyAmount > 0 && !Reward.CurrencyType.IsNone())
		{
			int32 AmountAdded = 0;
			Inv->AddItem(FDCItemHandle(Reward.CurrencyType),
			             Reward.CurrencyAmount, AmountAdded);
		}

		if (Narrative && !Reward.GrantedTags.IsEmpty())
		{
			Narrative->AddTags(Reward.GrantedTags);
		}

		// [OUTPUT] To: UDCProgressionComponent — XP + level-up evaluation
		if (Progression && Reward.ExperiencePoints > 0)
		{
			Progression->GrantXP(Reward.ExperiencePoints);
		}
	}
}

// ─── Event hooks ─────────────────────────────────────────────────────────────

void UDCQuestSubsystem::NotifyEnemyKilled(AActor* Target, AActor* /*KilledBy*/)
{
	if (!Target)
	{
		return;
	}

	for (auto& Pair : ActiveQuests)
	{
		UDCQuestDefinition* Def = Pair.Value.Definition;
		if (!Def)
		{
			continue;
		}
		for (const FDCQuestObjective& Obj : Def->Objectives)
		{
			if (Obj.ObjectiveType != EDCQuestObjectiveType::KillTarget)
			{
				continue;
			}
			UClass* RequiredClass = Obj.TargetClass.Get();
			if (!RequiredClass)
			{
				RequiredClass = Obj.TargetClass.LoadSynchronous();
			}
			if (RequiredClass && Target->IsA(RequiredClass))
			{
				AdvanceObjective(Pair.Key, Obj.ObjectiveID, 1);
			}
		}
	}
}

void UDCQuestSubsystem::NotifyNPCTalked(FName NPCID)
{
	if (NPCID.IsNone())
	{
		return;
	}
	for (auto& Pair : ActiveQuests)
	{
		UDCQuestDefinition* Def = Pair.Value.Definition;
		if (!Def)
		{
			continue;
		}
		for (const FDCQuestObjective& Obj : Def->Objectives)
		{
			if (Obj.ObjectiveType == EDCQuestObjectiveType::TalkToNPC &&
			    Obj.ObjectiveID == NPCID)
			{
				AdvanceObjective(Pair.Key, Obj.ObjectiveID, 1);
			}
		}
	}
}

void UDCQuestSubsystem::NotifyInteracted(AActor* Target)
{
	if (!Target)
	{
		return;
	}
	for (auto& Pair : ActiveQuests)
	{
		UDCQuestDefinition* Def = Pair.Value.Definition;
		if (!Def)
		{
			continue;
		}
		for (const FDCQuestObjective& Obj : Def->Objectives)
		{
			if (Obj.ObjectiveType != EDCQuestObjectiveType::InteractWithObject)
			{
				continue;
			}
			UClass* RequiredClass = Obj.TargetClass.Get();
			if (!RequiredClass)
			{
				RequiredClass = Obj.TargetClass.LoadSynchronous();
			}
			if (RequiredClass && Target->IsA(RequiredClass))
			{
				AdvanceObjective(Pair.Key, Obj.ObjectiveID, 1);
			}
		}
	}
}

void UDCQuestSubsystem::NotifyLocationReached(FGameplayTag LocationTag)
{
	if (!LocationTag.IsValid())
	{
		return;
	}
	for (auto& Pair : ActiveQuests)
	{
		UDCQuestDefinition* Def = Pair.Value.Definition;
		if (!Def)
		{
			continue;
		}
		for (const FDCQuestObjective& Obj : Def->Objectives)
		{
			if (Obj.ObjectiveType == EDCQuestObjectiveType::ReachLocation &&
			    Obj.CompletionTag == LocationTag)
			{
				AdvanceObjective(Pair.Key, Obj.ObjectiveID, 1);
			}
		}
	}
}

// ─── Auto-listeners ──────────────────────────────────────────────────────────

void UDCQuestSubsystem::HandleInventoryItemAdded(FDCItemHandle ItemHandle,
                                                 int32 AmountAdded, int32 NewTotal)
{
	if (!ItemHandle.IsValid() || AmountAdded <= 0)
	{
		return;
	}
	for (auto& Pair : ActiveQuests)
	{
		UDCQuestDefinition* Def = Pair.Value.Definition;
		if (!Def)
		{
			continue;
		}
		for (const FDCQuestObjective& Obj : Def->Objectives)
		{
			if (Obj.ObjectiveType == EDCQuestObjectiveType::CollectItem &&
			    Obj.TargetItem == ItemHandle)
			{
				AdvanceObjective(Pair.Key, Obj.ObjectiveID, AmountAdded);
			}
		}
	}
}

void UDCQuestSubsystem::HandleNarrativeTagsChanged(const FGameplayTagContainer& Added,
                                                   const FGameplayTagContainer& /*Removed*/)
{
	if (Added.IsEmpty())
	{
		return;
	}
	for (auto& Pair : ActiveQuests)
	{
		UDCQuestDefinition* Def = Pair.Value.Definition;
		if (!Def)
		{
			continue;
		}
		for (const FDCQuestObjective& Obj : Def->Objectives)
		{
			if (Obj.ObjectiveType == EDCQuestObjectiveType::Custom &&
			    Obj.CompletionTag.IsValid() &&
			    Added.HasTag(Obj.CompletionTag))
			{
				// Custom objectives complete in one shot
				AdvanceObjective(Pair.Key, Obj.ObjectiveID, Obj.RequiredCount);
			}
		}
	}
}

// ─── Progress / completion ───────────────────────────────────────────────────

void UDCQuestSubsystem::AdvanceObjective(FName QuestID, FName ObjectiveID, int32 Delta)
{
	FDCActiveQuestState* State = ActiveQuests.Find(QuestID);
	if (!State || State->State != EDCQuestState::Active)
	{
		return;
	}

	const FDCQuestObjective* ObjDef = State->Definition
		? State->Definition->Objectives.FindByPredicate(
			[ObjectiveID](const FDCQuestObjective& O) { return O.ObjectiveID == ObjectiveID; })
		: nullptr;
	if (!ObjDef)
	{
		return;
	}

	int32& Progress = State->ObjectiveProgress.FindOrAdd(ObjectiveID);
	const int32 Cap = FMath::Max(1, ObjDef->RequiredCount);
	if (Progress >= Cap)
	{
		return; // Already complete
	}

	const int32 Old = Progress;
	Progress = FMath::Clamp(Progress + Delta, 0, Cap);
	if (Progress == Old)
	{
		return;
	}

	OnObjectiveProgress.Broadcast(QuestID, ObjectiveID, Progress);

	if (Progress >= Cap)
	{
		OnObjectiveCompleted.Broadcast(QuestID, ObjectiveID);

		// Auto-complete the whole quest when all objectives are done
		if (AreAllObjectivesComplete(*State))
		{
			CompleteQuest(QuestID, /*bForce*/ false);
		}
	}
}

bool UDCQuestSubsystem::AreAllObjectivesComplete(const FDCActiveQuestState& State) const
{
	if (!State.Definition)
	{
		return false;
	}
	for (const FDCQuestObjective& Obj : State.Definition->Objectives)
	{
		const int32 Progress = State.GetProgress(Obj.ObjectiveID);
		const int32 Required = FMath::Max(1, Obj.RequiredCount);
		if (Progress < Required)
		{
			return false;
		}
	}
	return true;
}

// ─── Query ───────────────────────────────────────────────────────────────────

bool UDCQuestSubsystem::GetQuestState(FName QuestID, FDCActiveQuestState& OutState) const
{
	if (const FDCActiveQuestState* Found = ActiveQuests.Find(QuestID))
	{
		OutState = *Found;
		return true;
	}
	return false;
}

void UDCQuestSubsystem::GetActiveQuests(TArray<FDCActiveQuestState>& OutQuests) const
{
	OutQuests.Reset(ActiveQuests.Num());
	for (const TPair<FName, FDCActiveQuestState>& Pair : ActiveQuests)
	{
		OutQuests.Add(Pair.Value);
	}
}

// ─── Save / restore ──────────────────────────────────────────────────────────

void UDCQuestSubsystem::CaptureForSave(TArray<FDCSavedQuestState>& OutQuests) const
{
	OutQuests.Reset();
	OutQuests.Reserve(ActiveQuests.Num() + CompletedQuestIDs.Num() + FailedQuestIDs.Num());

	for (const TPair<FName, FDCActiveQuestState>& Pair : ActiveQuests)
	{
		FDCSavedQuestState Saved;
		Saved.QuestID = Pair.Key;
		Saved.Definition = Pair.Value.Definition;	// soft path avoids baking a hard ref
		Saved.State = EDCSavedQuestState::Active;
		Saved.ObjectiveProgress = Pair.Value.ObjectiveProgress;
		OutQuests.Add(MoveTemp(Saved));
	}
	for (const FName& ID : CompletedQuestIDs)
	{
		FDCSavedQuestState Saved;
		Saved.QuestID = ID;
		Saved.State = EDCSavedQuestState::Completed;
		OutQuests.Add(MoveTemp(Saved));
	}
	for (const FName& ID : FailedQuestIDs)
	{
		FDCSavedQuestState Saved;
		Saved.QuestID = ID;
		Saved.State = EDCSavedQuestState::Failed;
		OutQuests.Add(MoveTemp(Saved));
	}
}

void UDCQuestSubsystem::RestoreFromSave(const TArray<FDCSavedQuestState>& SavedQuests)
{
	ActiveQuests.Empty();
	CompletedQuestIDs.Empty();
	FailedQuestIDs.Empty();

	for (const FDCSavedQuestState& Saved : SavedQuests)
	{
		switch (Saved.State)
		{
		case EDCSavedQuestState::Active:
		{
			FDCActiveQuestState Live;
			Live.QuestID = Saved.QuestID;
			Live.Definition = Saved.Definition.LoadSynchronous();
			Live.State = EDCQuestState::Active;
			Live.ObjectiveProgress = Saved.ObjectiveProgress;
			ActiveQuests.Add(Saved.QuestID, MoveTemp(Live));
			break;
		}
		case EDCSavedQuestState::Completed:
			CompletedQuestIDs.Add(Saved.QuestID);
			break;
		case EDCSavedQuestState::Failed:
			FailedQuestIDs.Add(Saved.QuestID);
			break;
		}
	}
}

// ─── Listener wiring ─────────────────────────────────────────────────────────

void UDCQuestSubsystem::BindToPlayer(ADCPlayerControllerBase* Player)
{
	if (!Player || BoundPlayer.Get() == Player)
	{
		return;
	}

	UnbindFromPlayer();
	BoundPlayer = Player;

	if (UDCInventoryComponent* Inv = Player->GetInventoryComponent())
	{
		Inv->OnItemAdded.AddDynamic(this, &UDCQuestSubsystem::HandleInventoryItemAdded);
	}
	if (UDCNarrativeStateComponent* Narrative = Player->GetNarrativeState())
	{
		Narrative->OnTagsChanged.AddDynamic(
			this, &UDCQuestSubsystem::HandleNarrativeTagsChanged);
	}
}

void UDCQuestSubsystem::UnbindFromPlayer()
{
	if (ADCPlayerControllerBase* PC = BoundPlayer.Get())
	{
		if (UDCInventoryComponent* Inv = PC->GetInventoryComponent())
		{
			Inv->OnItemAdded.RemoveDynamic(this, &UDCQuestSubsystem::HandleInventoryItemAdded);
		}
		if (UDCNarrativeStateComponent* Narrative = PC->GetNarrativeState())
		{
			Narrative->OnTagsChanged.RemoveDynamic(
				this, &UDCQuestSubsystem::HandleNarrativeTagsChanged);
		}
	}
	BoundPlayer.Reset();
}
