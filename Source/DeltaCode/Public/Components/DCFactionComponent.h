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
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"
#include "DCFactionComponent.generated.h"

class UDCFactionDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCOnFactionAssigned,
	UDCFactionComponent*, Component, UDCFactionDefinition*, Faction);

/**
 * Per-actor faction identity. Attaches to NPCs, enemies, or any pawn that
 * should participate in reputation / perception affiliation. The source of
 * truth is the UDCFactionDefinition soft reference — everything else is
 * derived on demand so the same component can be reassigned at runtime
 * (e.g. brainwashed NPC, converted ally) without stale caches.
 *
 * [INPUT]  From: ADCNPCBase / ADCEnemyBase — authored on the character BP
 * [OUTPUT] To:   UDCReputationSubsystem — stance queries against the player
 * [OUTPUT] To:   ADCEnemyAIController::GetGenericTeamId — perception team
 *
 * [DEPENDS ON] UDCFactionDefinition — identity + default-stance tables
 */
UCLASS(ClassGroup = (DeltaCode), meta = (BlueprintSpawnableComponent))
class DELTACODE_API UDCFactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCFactionComponent();

	// Authored per-placement or on the owning character's BP
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Faction")
	TSoftObjectPtr<UDCFactionDefinition> FactionAsset;

	/** Swap the faction at runtime. Loads synchronously and rebroadcasts OnFactionAssigned. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Faction")
	void SetFaction(UDCFactionDefinition* NewFaction);

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Faction")
	UDCFactionDefinition* GetFaction() const { return LoadedFaction; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Faction")
	FGameplayTag GetFactionTag() const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Faction")
	FName GetFactionID() const;

	/** Returns FactionDefinition::TeamID, or 255 (neutral) when unassigned. */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Faction")
	uint8 GetTeamID() const;

	/** Resolve this component's stance toward another faction component's owner. */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Faction")
	bool GetStanceToward(const UDCFactionComponent* Other, uint8& OutStance) const;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Faction")
	FDCOnFactionAssigned OnFactionAssigned;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDCFactionDefinition> LoadedFaction;
};
