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
#include "DCNarrativeStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCOnNarrativeTagsChanged,
	const FGameplayTagContainer&, Added, const FGameplayTagContainer&, Removed);

/**
 * Persistent per-player tag store used by dialogue gating, quest objectives,
 * and world state (e.g. "MetCaptain", "KnowsAboutTheKey", "Reputation.High").
 * Attaches to ADCPlayerControllerBase.
 *
 * Kept separate from GAS owned-tags because narrative tags outlive a pawn
 * (respawn / level transition) and are serialized via UDCSaveGame.
 *
 * [INPUT]  From: UDCDialogueComponent — grants FDCDialogueResponse::GrantedTags
 * [INPUT]  From: UDCQuestSubsystem    — grants quest completion tags + FDCQuestReward tags
 * [OUTPUT] To:   UDCDialogueComponent — filters Responses by RequiredTags
 * [OUTPUT] To:   UDCQuestSubsystem    — evaluates PrerequisiteTags / BlockingTags
 * [OUTPUT] To:   UDCSaveGame          — persisted across sessions
 */
UCLASS(ClassGroup = (DeltaCode), meta = (BlueprintSpawnableComponent))
class DELTACODE_API UDCNarrativeStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCNarrativeStateComponent();

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Narrative")
	void AddTag(const FGameplayTag& Tag);

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Narrative")
	void AddTags(const FGameplayTagContainer& Tags);

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Narrative")
	void RemoveTag(const FGameplayTag& Tag);

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Narrative")
	void RemoveTags(const FGameplayTagContainer& Tags);

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Narrative")
	bool HasTag(const FGameplayTag& Tag) const { return Tags.HasTag(Tag); }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Narrative")
	bool HasAllTags(const FGameplayTagContainer& Required) const
	{
		return Tags.HasAll(Required);
	}

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Narrative")
	bool HasAnyTag(const FGameplayTagContainer& Any) const
	{
		return Tags.HasAny(Any);
	}

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Narrative")
	const FGameplayTagContainer& GetTags() const { return Tags; }

	/** Replace the entire tag state — used by UDCSaveGame on load. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Narrative")
	void SetTagsFromSave(const FGameplayTagContainer& InTags);

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Narrative")
	FDCOnNarrativeTagsChanged OnTagsChanged;

private:
	UPROPERTY(Transient)
	FGameplayTagContainer Tags;
};
