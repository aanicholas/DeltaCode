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
#include "DCSkillTreeComponent.generated.h"

class UDCSkillDefinition;
class UDCSkillTreeDefinition;
class UDCProgressionComponent;
class UDCAttributeSet;
class UDCHealthComponent;
class UDCNarrativeStateComponent;
class ADCPlayerControllerBase;
class ADCCharacterBase;

UENUM(BlueprintType)
enum class EDCSkillUnlockResult : uint8
{
	Success              UMETA(DisplayName = "Success"),
	Failed_Unknown       UMETA(DisplayName = "Unknown Skill"),
	Failed_AlreadyOwned  UMETA(DisplayName = "Already Unlocked"),
	Failed_Level         UMETA(DisplayName = "Level Too Low"),
	Failed_Prerequisite  UMETA(DisplayName = "Prerequisite Missing"),
	Failed_Points        UMETA(DisplayName = "Not Enough Skill Points"),
	Failed_NoTree        UMETA(DisplayName = "No Tree Asset"),
	Failed_NoProgression UMETA(DisplayName = "No Progression Component"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnSkillUnlocked, FName, SkillID);

/**
 * Per-player skill tree. Lives on ADCPlayerControllerBase alongside the
 * progression component. Validates prerequisites + minimum level + skill-point
 * cost (via UDCProgressionComponent::SpendSkillPoints), then applies the skill's
 * FDCStatModifier to the controlled pawn's attribute set + health component and
 * grants any GrantedTags to narrative state.
 *
 * [INPUT]  From: W_DC_SkillTreePanel::OnNodeClicked → UnlockSkill(SkillID)
 * [INPUT]  From: UDCSaveGame restore → RestoreFromSave(SavedSkills)
 * [OUTPUT] To:   UDCProgressionComponent::SpendSkillPoints — deducts cost
 * [OUTPUT] To:   UDCAttributeSet + UDCHealthComponent — additive modifiers
 * [OUTPUT] To:   UDCNarrativeStateComponent::AddTags — GrantedTags
 *
 * [DEPENDS ON] UDCSkillTreeDefinition — node graph + costs + prereqs
 * [DEPENDS ON] UDCProgressionComponent — skill-point pool + current level
 */
UCLASS(ClassGroup = (DeltaCode), meta = (BlueprintSpawnableComponent))
class DELTACODE_API UDCSkillTreeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCSkillTreeComponent();

	/** Which tree this player uses. Loaded synchronously on first use. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Skill")
	TSoftObjectPtr<UDCSkillTreeDefinition> SkillTree;

	// ── Query ───────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Skill")
	bool IsUnlocked(FName SkillID) const { return UnlockedSkillIDs.Contains(SkillID); }

	/** Non-mutating dry run. Writes a failure reason if the return is non-Success. */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Skill")
	EDCSkillUnlockResult CheckCanUnlock(FName SkillID) const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Skill")
	void GetUnlockedSkills(TArray<FName>& OutSkills) const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Skill")
	int32 GetUnlockedCount() const { return UnlockedSkillIDs.Num(); }

	// ── Mutation ────────────────────────────────────────────────────────────

	/** Validate + spend points + apply modifiers + grant tags. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Skill")
	EDCSkillUnlockResult UnlockSkill(FName SkillID);

	// ── Save / restore ──────────────────────────────────────────────────────

	/** Writes the set of unlocked SkillIDs for UDCSaveGame. */
	void CaptureForSave(TArray<FName>& OutSkills) const;

	/**
	 * Replay unlocks from save: re-applies every skill's modifiers + tags
	 * without spending points (progression restore handles the point pool).
	 */
	void RestoreFromSave(const TArray<FName>& SavedSkills);

	// ── Delegates ───────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Skill")
	FDCOnSkillUnlocked OnSkillUnlocked;

private:
	UDCSkillTreeDefinition* ResolveTree() const;
	ADCPlayerControllerBase* GetOwningPC() const;
	ADCCharacterBase* GetControlledCharacter() const;
	UDCProgressionComponent* GetProgression() const;
	UDCAttributeSet* GetAttributeSet() const;
	UDCHealthComponent* GetHealth() const;
	UDCNarrativeStateComponent* GetNarrative() const;

	// Pure validation — used by both CheckCanUnlock and UnlockSkill.
	EDCSkillUnlockResult ValidateUnlock(const UDCSkillDefinition& Skill) const;

	// Apply stat modifier + granted tags. Shared between UnlockSkill and RestoreFromSave.
	void ApplySkill(const UDCSkillDefinition& Skill);

	UPROPERTY(Transient)
	TSet<FName> UnlockedSkillIDs;

	UPROPERTY(Transient)
	mutable TObjectPtr<UDCSkillTreeDefinition> CachedTree;
};
