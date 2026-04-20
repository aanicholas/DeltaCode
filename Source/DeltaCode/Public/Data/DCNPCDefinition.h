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
#include "DCNPCDefinition.generated.h"

class ACharacter;
class USkeletalMesh;
class UDCVendorDefinition;
class UDCQuestDefinition;

UENUM(BlueprintType)
enum class EDCNPCRole : uint8
{
	Ambient         UMETA(DisplayName = "Ambient"),
	QuestGiver      UMETA(DisplayName = "Quest Giver"),
	Vendor          UMETA(DisplayName = "Vendor"),
	VendorAndQuests UMETA(DisplayName = "Vendor + Quest Giver"),
	HostileOnTrigger UMETA(DisplayName = "Hostile On Trigger"),
};

/**
 * NPC definition data asset.
 * Created as DA_DC_NPC_[Name] in Content/DeltaCode/Data/NPCs/.
 *
 * [INPUT]  From: Mission templates — placed in levels with a definition reference
 * [OUTPUT] To:   ADCNPCBase — spawned class + runtime component wiring
 * [OUTPUT] To:   UDCDialogueComponent — dialogue tree source
 * [OUTPUT] To:   UDCVendorComponent — vendor role wiring
 * [OUTPUT] To:   UDCQuestGiverComponent — quest role wiring
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API UDCNPCDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// ── Identity ────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AssetRegistrySearchable,
	          Category = "DeltaCode|NPC")
	FName NPCID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|NPC")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AssetRegistrySearchable,
	          Category = "DeltaCode|NPC")
	EDCNPCRole Role = EDCNPCRole::Ambient;

	// Runtime Character class spawned for this NPC
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|NPC")
	TSoftClassPtr<ACharacter> NPCClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|NPC")
	TSoftObjectPtr<USkeletalMesh> NPCMesh;

	// ── Dialogue tree ───────────────────────────────────────────────────────

	// Entry node ID within the DialogueNodes array
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|NPC|Dialogue")
	FName RootDialogueNodeID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|NPC|Dialogue",
	          meta = (TitleProperty = "NodeID"))
	TArray<FDCDialogueNode> DialogueNodes;

	// Resolve a node by ID for convenience
	UFUNCTION(BlueprintPure, Category = "DeltaCode|NPC|Dialogue")
	bool FindDialogueNode(FName NodeID, FDCDialogueNode& OutNode) const;

	// ── Vendor role ─────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|NPC|Vendor",
	          meta = (EditCondition =
	                  "Role == EDCNPCRole::Vendor || Role == EDCNPCRole::VendorAndQuests"))
	TSoftObjectPtr<UDCVendorDefinition> VendorDefinition;

	// ── Quest role ──────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|NPC|Quest",
	          meta = (EditCondition =
	                  "Role == EDCNPCRole::QuestGiver || Role == EDCNPCRole::VendorAndQuests"))
	TArray<TSoftObjectPtr<UDCQuestDefinition>> AvailableQuests;

	// ── Faction / reputation ────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|NPC")
	FGameplayTagContainer FactionTags;
};
