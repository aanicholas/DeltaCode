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
#include "Components/DCSkillTreeComponent.h"
#include "Actors/DCCharacterBase.h"
#include "Actors/DCPlayerControllerBase.h"
#include "Components/DCHealthComponent.h"
#include "Components/DCNarrativeStateComponent.h"
#include "Components/DCProgressionComponent.h"
#include "Data/DCSkillDefinition.h"
#include "Data/DCSkillTreeDefinition.h"
#include "GAS/DCAttributeSet.h"
#include "AbilitySystemComponent.h"

UDCSkillTreeComponent::UDCSkillTreeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// ─── Resolution helpers ──────────────────────────────────────────────────────

UDCSkillTreeDefinition* UDCSkillTreeComponent::ResolveTree() const
{
	if (CachedTree)
	{
		return CachedTree;
	}
	if (SkillTree.IsNull())
	{
		return nullptr;
	}
	CachedTree = SkillTree.LoadSynchronous();
	return CachedTree;
}

ADCPlayerControllerBase* UDCSkillTreeComponent::GetOwningPC() const
{
	return Cast<ADCPlayerControllerBase>(GetOwner());
}

ADCCharacterBase* UDCSkillTreeComponent::GetControlledCharacter() const
{
	if (const ADCPlayerControllerBase* PC = GetOwningPC())
	{
		return Cast<ADCCharacterBase>(PC->GetPawn());
	}
	return nullptr;
}

UDCProgressionComponent* UDCSkillTreeComponent::GetProgression() const
{
	if (const ADCPlayerControllerBase* PC = GetOwningPC())
	{
		return PC->GetProgressionComponent();
	}
	return nullptr;
}

UDCAttributeSet* UDCSkillTreeComponent::GetAttributeSet() const
{
	if (ADCCharacterBase* Char = GetControlledCharacter())
	{
		if (UAbilitySystemComponent* ASC = Char->GetAbilitySystemComponent())
		{
			return const_cast<UDCAttributeSet*>(ASC->GetSet<UDCAttributeSet>());
		}
	}
	return nullptr;
}

UDCHealthComponent* UDCSkillTreeComponent::GetHealth() const
{
	if (ADCCharacterBase* Char = GetControlledCharacter())
	{
		return Char->GetHealthComponent();
	}
	return nullptr;
}

UDCNarrativeStateComponent* UDCSkillTreeComponent::GetNarrative() const
{
	if (const ADCPlayerControllerBase* PC = GetOwningPC())
	{
		return PC->GetNarrativeState();
	}
	return nullptr;
}

// ─── Query ───────────────────────────────────────────────────────────────────

void UDCSkillTreeComponent::GetUnlockedSkills(TArray<FName>& OutSkills) const
{
	OutSkills.Reset(UnlockedSkillIDs.Num());
	for (const FName& ID : UnlockedSkillIDs)
	{
		OutSkills.Add(ID);
	}
}

EDCSkillUnlockResult UDCSkillTreeComponent::CheckCanUnlock(FName SkillID) const
{
	UDCSkillTreeDefinition* Tree = ResolveTree();
	if (!Tree)
	{
		return EDCSkillUnlockResult::Failed_NoTree;
	}
	UDCSkillDefinition* Skill = Tree->FindSkill(SkillID);
	if (!Skill)
	{
		return EDCSkillUnlockResult::Failed_Unknown;
	}
	return ValidateUnlock(*Skill);
}

EDCSkillUnlockResult UDCSkillTreeComponent::ValidateUnlock(const UDCSkillDefinition& Skill) const
{
	if (UnlockedSkillIDs.Contains(Skill.SkillID))
	{
		return EDCSkillUnlockResult::Failed_AlreadyOwned;
	}

	UDCProgressionComponent* Progression = GetProgression();
	if (!Progression)
	{
		return EDCSkillUnlockResult::Failed_NoProgression;
	}

	if (Progression->GetCurrentLevel() < Skill.RequiredPlayerLevel)
	{
		return EDCSkillUnlockResult::Failed_Level;
	}

	for (const FName& PrereqID : Skill.PrerequisiteSkills)
	{
		if (!UnlockedSkillIDs.Contains(PrereqID))
		{
			return EDCSkillUnlockResult::Failed_Prerequisite;
		}
	}

	if (Progression->GetUnspentSkillPoints() < Skill.Cost)
	{
		return EDCSkillUnlockResult::Failed_Points;
	}

	return EDCSkillUnlockResult::Success;
}

// ─── Mutation ────────────────────────────────────────────────────────────────

EDCSkillUnlockResult UDCSkillTreeComponent::UnlockSkill(FName SkillID)
{
	UDCSkillTreeDefinition* Tree = ResolveTree();
	if (!Tree)
	{
		return EDCSkillUnlockResult::Failed_NoTree;
	}
	UDCSkillDefinition* Skill = Tree->FindSkill(SkillID);
	if (!Skill)
	{
		return EDCSkillUnlockResult::Failed_Unknown;
	}

	const EDCSkillUnlockResult Check = ValidateUnlock(*Skill);
	if (Check != EDCSkillUnlockResult::Success)
	{
		return Check;
	}

	UDCProgressionComponent* Progression = GetProgression();
	// ValidateUnlock already guaranteed sufficient points — but SpendSkillPoints
	// remains the authority, so any race here surfaces as Failed_Points.
	if (!Progression->SpendSkillPoints(Skill->Cost))
	{
		return EDCSkillUnlockResult::Failed_Points;
	}

	UnlockedSkillIDs.Add(Skill->SkillID);
	ApplySkill(*Skill);

	OnSkillUnlocked.Broadcast(Skill->SkillID);
	return EDCSkillUnlockResult::Success;
}

// ─── Modifier application ────────────────────────────────────────────────────

void UDCSkillTreeComponent::ApplySkill(const UDCSkillDefinition& Skill)
{
	if (UDCAttributeSet* Attrs = GetAttributeSet())
	{
		if (Skill.Modifiers.AttackPowerBonus != 0.0f)
		{
			Attrs->SetAttackPower(Attrs->GetAttackPower() + Skill.Modifiers.AttackPowerBonus);
		}
		if (Skill.Modifiers.DefenseBonus != 0.0f)
		{
			Attrs->SetDefense(Attrs->GetDefense() + Skill.Modifiers.DefenseBonus);
		}
		if (Skill.Modifiers.MaxStaminaBonus != 0.0f)
		{
			const float NewMax = Attrs->GetMaxStamina() + Skill.Modifiers.MaxStaminaBonus;
			Attrs->SetMaxStamina(NewMax);
			// Refill stamina on skill unlock (matches level-up behavior)
			Attrs->SetStamina(NewMax);
		}
	}

	if (Skill.Modifiers.MaxHealthBonus != 0.0f)
	{
		if (UDCHealthComponent* Health = GetHealth())
		{
			Health->AdjustMaxHealth(Skill.Modifiers.MaxHealthBonus, /*bRefill*/ true);
		}
	}

	if (!Skill.GrantedTags.IsEmpty())
	{
		if (UDCNarrativeStateComponent* Narrative = GetNarrative())
		{
			Narrative->AddTags(Skill.GrantedTags);
		}
	}
}

// ─── Save / restore ──────────────────────────────────────────────────────────

void UDCSkillTreeComponent::CaptureForSave(TArray<FName>& OutSkills) const
{
	OutSkills.Reset(UnlockedSkillIDs.Num());
	for (const FName& ID : UnlockedSkillIDs)
	{
		OutSkills.Add(ID);
	}
}

void UDCSkillTreeComponent::RestoreFromSave(const TArray<FName>& SavedSkills)
{
	UDCSkillTreeDefinition* Tree = ResolveTree();
	if (!Tree)
	{
		// No tree means we can't resolve modifiers; still store the IDs so a later
		// SkillTree asset change doesn't silently drop the player's unlocks.
		UnlockedSkillIDs.Empty();
		UnlockedSkillIDs.Reserve(SavedSkills.Num());
		for (const FName& ID : SavedSkills)
		{
			UnlockedSkillIDs.Add(ID);
		}
		return;
	}

	UnlockedSkillIDs.Empty();
	UnlockedSkillIDs.Reserve(SavedSkills.Num());

	for (const FName& ID : SavedSkills)
	{
		UnlockedSkillIDs.Add(ID);
		if (UDCSkillDefinition* Skill = Tree->FindSkill(ID))
		{
			ApplySkill(*Skill);
		}
	}
}
