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
#include "Components/DCProgressionComponent.h"
#include "Components/DCHealthComponent.h"
#include "GAS/DCAttributeSet.h"
#include "Data/DCLevelCurve.h"
#include "Actors/DCCharacterBase.h"
#include "Actors/DCPlayerControllerBase.h"
#include "AbilitySystemComponent.h"

UDCProgressionComponent::UDCProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDCProgressionComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentLevel = FMath::Max(1, StartingLevel);
	CurrentXP = 0;
	UnspentSkillPoints = 0;

	// Mirror starting level into the attribute set if the pawn is already possessed
	if (UDCAttributeSet* Attrs = GetOwnerAttributeSet())
	{
		Attrs->SetLevel(static_cast<float>(CurrentLevel));
		Attrs->SetExperience(0.0f);
	}
}

// ─── Resolution helpers ──────────────────────────────────────────────────────

ADCCharacterBase* UDCProgressionComponent::GetControlledCharacter() const
{
	if (const ADCPlayerControllerBase* PC = Cast<ADCPlayerControllerBase>(GetOwner()))
	{
		return Cast<ADCCharacterBase>(PC->GetPawn());
	}
	return Cast<ADCCharacterBase>(GetOwner());
}

UAbilitySystemComponent* UDCProgressionComponent::GetOwnerASC() const
{
	if (ADCCharacterBase* Char = GetControlledCharacter())
	{
		return Char->GetAbilitySystemComponent();
	}
	return nullptr;
}

UDCAttributeSet* UDCProgressionComponent::GetOwnerAttributeSet() const
{
	if (UAbilitySystemComponent* ASC = GetOwnerASC())
	{
		return const_cast<UDCAttributeSet*>(ASC->GetSet<UDCAttributeSet>());
	}
	return nullptr;
}

UDCHealthComponent* UDCProgressionComponent::GetOwnerHealth() const
{
	if (ADCCharacterBase* Char = GetControlledCharacter())
	{
		return Char->GetHealthComponent();
	}
	return nullptr;
}

UDCLevelCurve* UDCProgressionComponent::ResolveLevelCurve() const
{
	if (CachedCurve)
	{
		return CachedCurve;
	}
	if (LevelCurve.IsNull())
	{
		return nullptr;
	}
	CachedCurve = LevelCurve.LoadSynchronous();
	return CachedCurve;
}

// ─── XP API ──────────────────────────────────────────────────────────────────

void UDCProgressionComponent::GrantXP(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	CurrentXP += Amount;

	if (UDCAttributeSet* Attrs = GetOwnerAttributeSet())
	{
		Attrs->SetExperience(static_cast<float>(CurrentXP));
	}

	EvaluateLevelUps();

	OnExperienceGained.Broadcast(Amount, CurrentXP, GetXPToNextLevel());
}

void UDCProgressionComponent::SetLevelDirect(int32 NewLevel, int32 NewXP)
{
	CurrentLevel = FMath::Max(1, NewLevel);
	CurrentXP = FMath::Max(0, NewXP);

	if (UDCAttributeSet* Attrs = GetOwnerAttributeSet())
	{
		Attrs->SetLevel(static_cast<float>(CurrentLevel));
		Attrs->SetExperience(static_cast<float>(CurrentXP));
	}
}

bool UDCProgressionComponent::SpendSkillPoints(int32 Amount)
{
	if (Amount <= 0 || UnspentSkillPoints < Amount)
	{
		return false;
	}
	UnspentSkillPoints -= Amount;
	OnSkillPointsChanged.Broadcast(UnspentSkillPoints);
	return true;
}

int32 UDCProgressionComponent::GetXPForNextLevel() const
{
	const UDCLevelCurve* Curve = ResolveLevelCurve();
	if (!Curve)
	{
		return MAX_int32;
	}
	const int32 NextLevel = CurrentLevel + 1;
	if (NextLevel > Curve->GetMaxLevel())
	{
		return MAX_int32;
	}
	return Curve->GetXPThresholdForLevel(NextLevel);
}

int32 UDCProgressionComponent::GetXPToNextLevel() const
{
	const int32 Threshold = GetXPForNextLevel();
	if (Threshold == MAX_int32)
	{
		return 0;
	}
	return FMath::Max(0, Threshold - CurrentXP);
}

// ─── Save / restore ──────────────────────────────────────────────────────────

void UDCProgressionComponent::CaptureForSave(int32& OutLevel, int32& OutXP,
                                             int32& OutSkillPoints) const
{
	OutLevel = CurrentLevel;
	OutXP = CurrentXP;
	OutSkillPoints = UnspentSkillPoints;
}

void UDCProgressionComponent::RestoreFromSave(int32 InLevel, int32 InXP, int32 InSkillPoints)
{
	SetLevelDirect(InLevel, InXP);
	UnspentSkillPoints = FMath::Max(0, InSkillPoints);
	OnSkillPointsChanged.Broadcast(UnspentSkillPoints);
}

// ─── Level-up evaluation ─────────────────────────────────────────────────────

void UDCProgressionComponent::EvaluateLevelUps()
{
	UDCLevelCurve* Curve = ResolveLevelCurve();
	if (!Curve)
	{
		return;
	}

	const int32 MaxLevel = Curve->GetMaxLevel();

	while (CurrentLevel < MaxLevel)
	{
		const int32 NextLevel = CurrentLevel + 1;
		FDCLevelUpBonus Bonus;
		if (!Curve->GetBonusForLevel(NextLevel, Bonus))
		{
			break;
		}
		if (CurrentXP < Bonus.XPThreshold)
		{
			break;
		}

		CurrentLevel = NextLevel;
		ApplyLevelBonus(Bonus);

		UnspentSkillPoints += Bonus.SkillPointsAwarded;

		if (UDCAttributeSet* Attrs = GetOwnerAttributeSet())
		{
			Attrs->SetLevel(static_cast<float>(CurrentLevel));
		}

		OnLeveledUp.Broadcast(CurrentLevel, Bonus.SkillPointsAwarded);
		if (Bonus.SkillPointsAwarded > 0)
		{
			OnSkillPointsChanged.Broadcast(UnspentSkillPoints);
		}
	}
}

void UDCProgressionComponent::ApplyLevelBonus(const FDCLevelUpBonus& Bonus)
{
	if (UDCAttributeSet* Attrs = GetOwnerAttributeSet())
	{
		if (Bonus.StaminaBonus != 0.0f)
		{
			const float NewMaxStam = Attrs->GetMaxStamina() + Bonus.StaminaBonus;
			Attrs->SetMaxStamina(NewMaxStam);
			Attrs->SetStamina(NewMaxStam); // refill on level-up
		}
		if (Bonus.AttackPowerBonus != 0.0f)
		{
			Attrs->SetAttackPower(Attrs->GetAttackPower() + Bonus.AttackPowerBonus);
		}
		if (Bonus.DefenseBonus != 0.0f)
		{
			Attrs->SetDefense(Attrs->GetDefense() + Bonus.DefenseBonus);
		}
	}

	// Bump engine-authoritative max health and refill the bar
	if (Bonus.HealthBonus != 0.0f)
	{
		if (UDCHealthComponent* Health = GetOwnerHealth())
		{
			Health->AdjustMaxHealth(Bonus.HealthBonus, /*bRefill*/ true);
		}
	}
}
