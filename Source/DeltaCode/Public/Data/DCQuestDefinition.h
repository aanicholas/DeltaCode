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
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Types/DCDataDefinitions.h"
#include "DCQuestDefinition.generated.h"

class UTexture2D;

/**
 * Quest definition data asset.
 * Created as DA_DC_Quest_[Name] in Content/DeltaCode/Data/Quests/.
 *
 * [INPUT]  From: UDCQuestGiverComponent — offered to player via NPC interaction
 * [OUTPUT] To:   UDCQuestSubsystem — runtime state tracking, objective updates
 * [OUTPUT] To:   W_DC_QuestLog — display quest title / description / progress
 *
 * Quest logic lives on the subsystem. This asset is data only — never put
 * behavior here beyond static references and configuration.
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API UDCQuestDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// ── Identity ────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AssetRegistrySearchable,
	          Category = "DeltaCode|Quest")
	FName QuestID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest",
	          meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest")
	TSoftObjectPtr<UTexture2D> Icon;

	// ── Classification ──────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AssetRegistrySearchable,
	          Category = "DeltaCode|Quest")
	bool bIsMainQuest = false;

	// Recommended player level — purely cosmetic hint
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest",
	          meta = (ClampMin = "0"))
	int32 RecommendedLevel = 0;

	// Tags required on the player to accept this quest (faction, prior story)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest")
	FGameplayTagContainer PrerequisiteTags;

	// Tags that block this quest from being offered (opposing faction, already failed)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest")
	FGameplayTagContainer BlockingTags;

	// ── Objectives & rewards ────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest",
	          meta = (TitleProperty = "ObjectiveID"))
	TArray<FDCQuestObjective> Objectives;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest")
	TArray<FDCQuestReward> Rewards;

	// Tags granted on quest completion (reputation, story progress)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest")
	FGameplayTagContainer CompletionTags;

	// If true, the player can fail this quest permanently
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest")
	bool bCanFail = false;

	// Next quest automatically offered after completion
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest")
	TSoftObjectPtr<UDCQuestDefinition> FollowUpQuest;
};
