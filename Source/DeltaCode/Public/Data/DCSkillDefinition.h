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
#include "Types/DCCoreTypes.h"
#include "DCSkillDefinition.generated.h"

class UTexture2D;

/**
 * A single unlockable skill node. Consumed by UDCSkillTreeDefinition + UDCSkillTreeComponent.
 * Created as DA_DC_Skill_[Name] in Content/DeltaCode/Data/Skills/.
 *
 * Costs skill points; may require prior skills and a minimum player level.
 * Unlocking applies FDCStatModifier additively (AttackPower / Defense / MaxStamina /
 * MaxHealth) and grants any tags in GrantedTags to the player's narrative state.
 *
 * [INPUT]  From: UDCSkillTreeComponent::UnlockSkill   — validation + application
 * [OUTPUT] To:   UDCAttributeSet + UDCHealthComponent — modifier payload
 * [OUTPUT] To:   UDCNarrativeStateComponent           — GrantedTags on unlock
 */
UCLASS(BlueprintType)
class DELTACODE_API UDCSkillDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable identifier referenced by save files and cross-skill prereqs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Skill",
	          AssetRegistrySearchable)
	FName SkillID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Skill")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Skill",
	          meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Skill")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Skill-point cost. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Skill",
	          meta = (ClampMin = "1"))
	int32 Cost = 1;

	/** Minimum player level required before this node becomes available. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Skill",
	          meta = (ClampMin = "1"))
	int32 RequiredPlayerLevel = 1;

	/** Other SkillIDs that must already be unlocked. Empty = root node. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Skill")
	TArray<FName> PrerequisiteSkills;

	/** Additive stat modifier applied on unlock; never removed (no "respec" yet). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Skill")
	FDCStatModifier Modifiers;

	/** Tags granted to the player's narrative state on unlock. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeltaCode|Skill")
	FGameplayTagContainer GrantedTags;
};
