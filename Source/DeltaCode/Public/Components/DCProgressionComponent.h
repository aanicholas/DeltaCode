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
#include "DCProgressionComponent.generated.h"

class UDCLevelCurve;
class UDCAttributeSet;
class UAbilitySystemComponent;
class UDCHealthComponent;
class ADCCharacterBase;

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDCOnExperienceGained,
	int32, AmountGained, int32, NewTotalXP, int32, XPToNextLevel);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCOnLeveledUp,
	int32, NewLevel, int32, SkillPointsAwarded);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnSkillPointsChanged,
	int32, NewUnspent);

/**
 * Player-facing progression: experience, level, skill points.
 * Lives on ADCPlayerControllerBase (so progression survives pawn respawn).
 *
 * Bridges to GAS via the controlled pawn's UAbilitySystemComponent + UDCAttributeSet.
 * On level-up, applies bonuses to both the attribute set (Stamina/Attack/Defense)
 * and the engine-authoritative UDCHealthComponent (max health).
 *
 * [INPUT]  From: UDCQuestSubsystem::GrantRewards     → GrantXP(Reward.ExperiencePoints)
 * [INPUT]  From: ADCEnemyBase::HandleDeath (future)  → GrantXP(LootXP)
 * [OUTPUT] To:   UDCAttributeSet (Experience / Level / Stamina / Attack / Defense)
 * [OUTPUT] To:   UDCHealthComponent (DefaultMaxHealth bump on level-up)
 * [OUTPUT] To:   W_DC_HUDRoot — XP bar / level badge bind to OnExperienceGained
 *
 * [DEPENDS ON] UDCLevelCurve — XP thresholds + per-level stat bumps
 * [DEPENDS ON] UDCAttributeSet on the controlled pawn
 */
UCLASS(ClassGroup = (DeltaCode), meta = (BlueprintSpawnableComponent))
class DELTACODE_API UDCProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCProgressionComponent();

	// ── Configuration ───────────────────────────────────────────────────────

	/** Level curve asset that drives XP thresholds and per-level bonuses. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Progression")
	TSoftObjectPtr<UDCLevelCurve> LevelCurve;

	/** Starting level for new characters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Progression",
	          meta = (ClampMin = "1"))
	int32 StartingLevel = 1;

	// ── Runtime API ─────────────────────────────────────────────────────────

	/** Grant XP to the player. Triggers level-up evaluation. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Progression")
	void GrantXP(int32 Amount);

	/** Direct level set (debug / save-game restore). Skips bonus application. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Progression")
	void SetLevelDirect(int32 NewLevel, int32 NewXP);

	/** Spend skill points (returns true if there were enough). */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Progression")
	bool SpendSkillPoints(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Progression")
	int32 GetCurrentLevel() const { return CurrentLevel; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Progression")
	int32 GetCurrentXP() const { return CurrentXP; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Progression")
	int32 GetUnspentSkillPoints() const { return UnspentSkillPoints; }

	/** XP threshold for the next level, or INT32_MAX if at max level. */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Progression")
	int32 GetXPForNextLevel() const;

	/** XP remaining to next level, or 0 if at max level. */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Progression")
	int32 GetXPToNextLevel() const;

	// ── Save / restore ──────────────────────────────────────────────────────

	/** Copy current progression state into out-params for UDCSaveGame. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Progression|Save")
	void CaptureForSave(int32& OutLevel, int32& OutXP, int32& OutSkillPoints) const;

	/** Restore progression state from UDCSaveGame. Skips bonus re-application. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Progression|Save")
	void RestoreFromSave(int32 InLevel, int32 InXP, int32 InSkillPoints);

	// ── Delegates ───────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Progression")
	FDCOnExperienceGained OnExperienceGained;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Progression")
	FDCOnLeveledUp OnLeveledUp;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Progression")
	FDCOnSkillPointsChanged OnSkillPointsChanged;

protected:
	virtual void BeginPlay() override;

private:
	// Resolve the controlled pawn's ASC + AttributeSet (lazy)
	UAbilitySystemComponent* GetOwnerASC() const;
	UDCAttributeSet* GetOwnerAttributeSet() const;
	UDCHealthComponent* GetOwnerHealth() const;
	ADCCharacterBase* GetControlledCharacter() const;

	// Loads LevelCurve synchronously the first time we need it
	UDCLevelCurve* ResolveLevelCurve() const;

	// Walks up through any pending levels and applies bonuses
	void EvaluateLevelUps();

	// Apply a single level's bonuses to attribute set + health component
	void ApplyLevelBonus(const struct FDCLevelUpBonus& Bonus);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "DeltaCode|Progression",
	          meta = (AllowPrivateAccess = "true"))
	int32 CurrentLevel = 1;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "DeltaCode|Progression",
	          meta = (AllowPrivateAccess = "true"))
	int32 CurrentXP = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "DeltaCode|Progression",
	          meta = (AllowPrivateAccess = "true"))
	int32 UnspentSkillPoints = 0;

	// Cached after first sync load
	UPROPERTY(Transient)
	mutable TObjectPtr<UDCLevelCurve> CachedCurve;
};
