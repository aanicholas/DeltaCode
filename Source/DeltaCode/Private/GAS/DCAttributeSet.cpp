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
#include "GAS/DCAttributeSet.h"
#include "GameplayEffectExtension.h"

UDCAttributeSet::UDCAttributeSet()
{
	InitStamina(100.0f);
	InitMaxStamina(100.0f);
	InitAttackPower(10.0f);
	InitDefense(0.0f);
	InitExperience(0.0f);
	InitLevel(1.0f);
}

void UDCAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Clamp current stamina against max
	if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(1.0f, NewValue);
	}
	else if (Attribute == GetExperienceAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
	else if (Attribute == GetLevelAttribute())
	{
		NewValue = FMath::Max(1.0f, NewValue);
	}
	else if (Attribute == GetDefenseAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
	else if (Attribute == GetAttackPowerAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
}

void UDCAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Re-clamp stamina after a GE has executed (instant modifiers bypass PreAttributeChange on base)
	if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));
	}
}
