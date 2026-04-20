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
#include "Components/DCFactionComponent.h"
#include "Data/DCFactionDefinition.h"
#include "Subsystems/DCReputationSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

UDCFactionComponent::UDCFactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDCFactionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Resolve the soft reference once the component starts — perception/reputation
	// queries before BeginPlay will just see an unassigned neutral team.
	if (!LoadedFaction && !FactionAsset.IsNull())
	{
		LoadedFaction = FactionAsset.LoadSynchronous();
	}

	if (LoadedFaction)
	{
		// [OUTPUT] To: UDCReputationSubsystem — seeds initial reputation on first see.
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UDCReputationSubsystem* Rep = GI->GetSubsystem<UDCReputationSubsystem>())
				{
					Rep->RegisterFaction(LoadedFaction);
				}
			}
		}
		OnFactionAssigned.Broadcast(this, LoadedFaction);
	}
}

void UDCFactionComponent::SetFaction(UDCFactionDefinition* NewFaction)
{
	LoadedFaction = NewFaction;
	FactionAsset = NewFaction;

	if (LoadedFaction)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UDCReputationSubsystem* Rep = GI->GetSubsystem<UDCReputationSubsystem>())
				{
					Rep->RegisterFaction(LoadedFaction);
				}
			}
		}
	}

	OnFactionAssigned.Broadcast(this, LoadedFaction);
}

FGameplayTag UDCFactionComponent::GetFactionTag() const
{
	return LoadedFaction ? LoadedFaction->FactionTag : FGameplayTag::EmptyTag;
}

FName UDCFactionComponent::GetFactionID() const
{
	return LoadedFaction ? LoadedFaction->FactionID : NAME_None;
}

uint8 UDCFactionComponent::GetTeamID() const
{
	return LoadedFaction ? LoadedFaction->TeamID : FGenericTeamId::NoTeam.GetId();
}

bool UDCFactionComponent::GetStanceToward(const UDCFactionComponent* Other,
                                          uint8& OutStance) const
{
	if (!LoadedFaction || !Other || !Other->GetFaction())
	{
		return false;
	}
	OutStance = static_cast<uint8>(
		LoadedFaction->GetDefaultStanceToward(Other->GetFactionTag()));
	return true;
}
