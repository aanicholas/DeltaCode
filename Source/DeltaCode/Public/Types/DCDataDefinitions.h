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
#include "GameplayTagContainer.h"
#include "Types/DCCoreTypes.h"
#include "DCDataDefinitions.generated.h"

// ─── Dialogue ────────────────────────────────────────────────────────────────

/**
 * A single selectable response within a dialogue node.
 * Consumed by UDCDialogueComponent (Layer 3B).
 */
USTRUCT(BlueprintType)
struct DELTACODE_API FDCDialogueResponse
{
	GENERATED_BODY()

	// Response text shown on the button
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Dialogue")
	FText ResponseText;

	// ID of the next dialogue node (NAME_None ends the conversation)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Dialogue")
	FName NextNodeID = NAME_None;

	// Tags required on the player for this response to be available
	// (e.g., quest.ActiveQuest.Investigation, reputation.High)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Dialogue")
	FGameplayTagContainer RequiredTags;

	// Tags to grant to the player / world when this response is chosen
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Dialogue")
	FGameplayTagContainer GrantedTags;

	// Optional item handle required in the player's inventory
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Dialogue")
	FDCItemHandle RequiredItem;
};

/**
 * One node in a dialogue tree. NPC definitions contain an array of these.
 * [INPUT]  From: UDCDialogueComponent::StartConversation — walks the tree by NodeID
 * [OUTPUT] To:   W_DC_DialoguePanel — renders SpeakerText and Responses
 */
USTRUCT(BlueprintType)
struct DELTACODE_API FDCDialogueNode
{
	GENERATED_BODY()

	// Unique identifier within the containing dialogue tree
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Dialogue")
	FName NodeID = NAME_None;

	// Line shown in the dialogue panel as the NPC's statement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Dialogue",
	          meta = (MultiLine = "true"))
	FText SpeakerText;

	// Player-selectable responses. Empty = node auto-advances or ends.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Dialogue")
	TArray<FDCDialogueResponse> Responses;

	// Tags required on the player / world for this node to be reachable
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Dialogue")
	FGameplayTagContainer ConditionTags;

	// Tags fired when the player enters this node
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Dialogue")
	FGameplayTagContainer ExecutionTags;
};

// ─── Economy ─────────────────────────────────────────────────────────────────

/**
 * A single stock entry in a vendor's inventory.
 * [INPUT]  From: UDCVendorDefinition → BuildStock()
 * [OUTPUT] To:   UDCVendorComponent at runtime
 */
USTRUCT(BlueprintType)
struct DELTACODE_API FDCEconomyEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Economy")
	FDCItemHandle ItemHandle;

	// Price per unit in the vendor's currency
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Economy",
	          meta = (ClampMin = "0"))
	int32 Price = 0;

	// Currency identifier (e.g., Gold, Caps, Glimmer). Matches an item handle for currency items.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Economy")
	FName CurrencyType = TEXT("Gold");

	// Available stock. -1 = infinite.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Economy",
	          meta = (ClampMin = "-1"))
	int32 StockQuantity = -1;

	// Seconds between restocks. 0 = never restocks.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Economy",
	          meta = (ClampMin = "0.0"))
	float RestockIntervalSeconds = 0.0f;
};

// ─── Quest ───────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EDCQuestObjectiveType : uint8
{
	ReachLocation       UMETA(DisplayName = "Reach Location"),
	KillTarget          UMETA(DisplayName = "Kill Target"),
	CollectItem         UMETA(DisplayName = "Collect Item"),
	TalkToNPC           UMETA(DisplayName = "Talk to NPC"),
	InteractWithObject  UMETA(DisplayName = "Interact With Object"),
	Custom              UMETA(DisplayName = "Custom (tag-driven)"),
};

/**
 * A single objective within a quest.
 * [INPUT]  From: UDCQuestSubsystem::UpdateObjective — progress events
 * [OUTPUT] To:   W_DC_QuestLog — display status
 */
USTRUCT(BlueprintType)
struct DELTACODE_API FDCQuestObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest")
	FName ObjectiveID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest")
	FText DisplayText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest")
	EDCQuestObjectiveType ObjectiveType = EDCQuestObjectiveType::Custom;

	// Required count (kills, items, interactions). 1 by default.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest",
	          meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	// Optional item handle for CollectItem objectives
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest",
	          meta = (EditCondition =
	                  "ObjectiveType == EDCQuestObjectiveType::CollectItem"))
	FDCItemHandle TargetItem;

	// Target class for KillTarget / InteractWithObject
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest",
	          meta = (EditCondition =
	                  "ObjectiveType == EDCQuestObjectiveType::KillTarget || ObjectiveType == EDCQuestObjectiveType::InteractWithObject"))
	TSoftClassPtr<AActor> TargetClass;

	// Tag-driven objective completion (for Custom objectives)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest")
	FGameplayTag CompletionTag;

	// If true, objective is not shown in quest log until a prior objective completes
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest")
	bool bHiddenUntilUnlocked = false;
};

/**
 * A reward granted on quest completion.
 * [OUTPUT] To: UDCInventoryComponent::AddItem, ability system, XP system
 */
USTRUCT(BlueprintType)
struct DELTACODE_API FDCQuestReward
{
	GENERATED_BODY()

	// Item to grant (optional)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest|Reward")
	FDCItemHandle RewardItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest|Reward",
	          meta = (ClampMin = "0"))
	int32 RewardItemCount = 1;

	// Experience / progression points
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest|Reward",
	          meta = (ClampMin = "0"))
	int32 ExperiencePoints = 0;

	// Currency awarded (paired with CurrencyType FName)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest|Reward",
	          meta = (ClampMin = "0"))
	int32 CurrencyAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest|Reward")
	FName CurrencyType = TEXT("Gold");

	// Tags granted to the player (reputation, story flags)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Quest|Reward")
	FGameplayTagContainer GrantedTags;
};
