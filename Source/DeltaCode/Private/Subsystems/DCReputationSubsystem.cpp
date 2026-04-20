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
#include "Subsystems/DCReputationSubsystem.h"
#include "Subsystems/DCQuestSubsystem.h"
#include "Engine/GameInstance.h"

void UDCReputationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Ensure the quest subsystem is initialised first so BindToQuestSubsystem()
	// below can bind to its delegates without racing collection construction.
	Collection.InitializeDependency(UDCQuestSubsystem::StaticClass());
	Super::Initialize(Collection);

	BindToQuestSubsystem();
}

void UDCReputationSubsystem::Deinitialize()
{
	UnbindFromQuestSubsystem();
	RegisteredFactions.Empty();
	FactionReputation.Empty();
	Super::Deinitialize();
}

// ─── Registration ────────────────────────────────────────────────────────────

void UDCReputationSubsystem::RegisterFaction(UDCFactionDefinition* Faction)
{
	if (!Faction || !Faction->FactionTag.IsValid())
	{
		return;
	}

	const FGameplayTag Tag = Faction->FactionTag;
	RegisteredFactions.FindOrAdd(Tag) = Faction;

	// Seed initial reputation the first time we see this faction
	if (!FactionReputation.Contains(Tag))
	{
		const int32 Seed = ClampReputation(Tag, Faction->InitialPlayerReputation);
		FactionReputation.Add(Tag, Seed);
	}
}

UDCFactionDefinition* UDCReputationSubsystem::FindFactionByTag(
	const FGameplayTag& FactionTag) const
{
	const TObjectPtr<UDCFactionDefinition>* Found = RegisteredFactions.Find(FactionTag);
	return Found ? Found->Get() : nullptr;
}

// ─── Reputation accessors ────────────────────────────────────────────────────

int32 UDCReputationSubsystem::GetReputation(const FGameplayTag& FactionTag) const
{
	if (const int32* Found = FactionReputation.Find(FactionTag))
	{
		return *Found;
	}
	// Fall back to the definition's initial value if registered but never touched
	if (UDCFactionDefinition* Def = FindFactionByTag(FactionTag))
	{
		return Def->InitialPlayerReputation;
	}
	return 0;
}

void UDCReputationSubsystem::ModifyReputation(const FGameplayTag& FactionTag, int32 Delta)
{
	if (!FactionTag.IsValid() || Delta == 0)
	{
		return;
	}
	const int32 Current = GetReputation(FactionTag);
	const int32 NewValue = ClampReputation(FactionTag, Current + Delta);
	if (NewValue == Current)
	{
		return;
	}
	ApplyReputationInternal(FactionTag, NewValue, Current);
}

void UDCReputationSubsystem::SetReputation(const FGameplayTag& FactionTag, int32 Value)
{
	if (!FactionTag.IsValid())
	{
		return;
	}
	const int32 Current = GetReputation(FactionTag);
	const int32 NewValue = ClampReputation(FactionTag, Value);
	if (NewValue == Current)
	{
		return;
	}
	ApplyReputationInternal(FactionTag, NewValue, Current);
}

EDCFactionStance UDCReputationSubsystem::GetPlayerStance(const FGameplayTag& FactionTag) const
{
	if (UDCFactionDefinition* Def = FindFactionByTag(FactionTag))
	{
		return Def->ResolvePlayerStance(GetReputation(FactionTag));
	}
	return EDCFactionStance::Neutral;
}

bool UDCReputationSubsystem::IsPlayerHostileWith(const FGameplayTag& FactionTag) const
{
	return GetPlayerStance(FactionTag) == EDCFactionStance::Hostile;
}

// ─── Save / restore ──────────────────────────────────────────────────────────

void UDCReputationSubsystem::CaptureForSave(
	TArray<FDCSavedFactionReputation>& OutRecords) const
{
	OutRecords.Reset();
	OutRecords.Reserve(FactionReputation.Num());
	for (const TPair<FGameplayTag, int32>& Pair : FactionReputation)
	{
		FDCSavedFactionReputation Row;
		Row.FactionTag = Pair.Key;
		Row.Reputation = Pair.Value;
		OutRecords.Add(Row);
	}
}

void UDCReputationSubsystem::RestoreFromSave(
	const TArray<FDCSavedFactionReputation>& InRecords)
{
	// Restore preserves registration — the save only carries the player's
	// standing, not the set of faction definitions themselves.
	FactionReputation.Reset();
	for (const FDCSavedFactionReputation& Row : InRecords)
	{
		if (!Row.FactionTag.IsValid())
		{
			continue;
		}
		FactionReputation.Add(Row.FactionTag,
			ClampReputation(Row.FactionTag, Row.Reputation));
	}
}

// ─── Internal ────────────────────────────────────────────────────────────────

int32 UDCReputationSubsystem::ClampReputation(const FGameplayTag& FactionTag,
                                               int32 Value) const
{
	if (UDCFactionDefinition* Def = FindFactionByTag(FactionTag))
	{
		return FMath::Clamp(Value, Def->MinReputation, Def->MaxReputation);
	}
	return Value;
}

void UDCReputationSubsystem::ApplyReputationInternal(const FGameplayTag& FactionTag,
                                                      int32 NewValue, int32 OldValue)
{
	EDCFactionStance OldStance = EDCFactionStance::Neutral;
	EDCFactionStance NewStance = EDCFactionStance::Neutral;
	if (UDCFactionDefinition* Def = FindFactionByTag(FactionTag))
	{
		OldStance = Def->ResolvePlayerStance(OldValue);
		NewStance = Def->ResolvePlayerStance(NewValue);
	}

	FactionReputation.Add(FactionTag, NewValue);
	OnReputationChanged.Broadcast(FactionTag, NewValue, NewValue - OldValue);

	if (NewStance != OldStance)
	{
		OnStanceChanged.Broadcast(FactionTag, NewStance);
	}
}

// ─── Quest observation ──────────────────────────────────────────────────────

void UDCReputationSubsystem::BindToQuestSubsystem()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}
	UDCQuestSubsystem* Quests = GI->GetSubsystem<UDCQuestSubsystem>();
	if (!Quests)
	{
		return;
	}
	Quests->OnQuestCompleted.AddDynamic(this, &UDCReputationSubsystem::HandleQuestCompleted);
	Quests->OnQuestFailed.AddDynamic(this, &UDCReputationSubsystem::HandleQuestFailed);
	BoundQuestSubsystem = Quests;
}

void UDCReputationSubsystem::UnbindFromQuestSubsystem()
{
	if (UDCQuestSubsystem* Quests = BoundQuestSubsystem.Get())
	{
		Quests->OnQuestCompleted.RemoveDynamic(this, &UDCReputationSubsystem::HandleQuestCompleted);
		Quests->OnQuestFailed.RemoveDynamic(this, &UDCReputationSubsystem::HandleQuestFailed);
	}
	BoundQuestSubsystem.Reset();
}

void UDCReputationSubsystem::HandleQuestCompleted(const FDCActiveQuestState& State)
{
	// [OUTPUT] To: project Blueprint — maps quest → faction delta
	OnQuestCompletedForReputation.Broadcast(State.Definition);
}

void UDCReputationSubsystem::HandleQuestFailed(const FDCActiveQuestState& State)
{
	// [OUTPUT] To: project Blueprint — maps failed quest → faction penalty
	OnQuestFailedForReputation.Broadcast(State.Definition);
}
