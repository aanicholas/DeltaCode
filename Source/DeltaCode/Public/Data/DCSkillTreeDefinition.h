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
#include "DCSkillTreeDefinition.generated.h"

class UDCSkillDefinition;

/**
 * Top-level skill-tree asset. Lists the skill nodes available to a player.
 * Created as DA_DC_SkillTree_[Name] in Content/DeltaCode/Data/Skills/.
 *
 * A class / archetype / weapon-specialization all map to their own tree asset;
 * the player's UDCSkillTreeComponent points at the one in use.
 *
 * [INPUT]  From: UDCSkillTreeComponent::ResolveTree → node lookup via FindSkill
 * [OUTPUT] To:   W_DC_SkillTreePanel — paints the graph
 */
UCLASS(BlueprintType)
class DELTACODE_API UDCSkillTreeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable identifier for debug / save compatibility. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Skill",
	          AssetRegistrySearchable)
	FName TreeID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Skill")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Skill",
	          meta = (TitleProperty = "SkillID"))
	TArray<TObjectPtr<UDCSkillDefinition>> Skills;

	/** Linear scan — tree sizes are small (dozens of nodes), no lookup map needed. */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Skill")
	UDCSkillDefinition* FindSkill(FName SkillID) const;
};
