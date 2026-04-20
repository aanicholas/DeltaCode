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
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "DCAttributeSet.generated.h"

// ─── Attribute accessor macros ───────────────────────────────────────────────
// Standard UE pattern — generates a strongly-typed accessor + getter/setter/initter.

#define DC_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Primary attribute set for DeltaCode characters.
 *
 * Engine-authoritative Health/Shield continue to live on UDCHealthComponent —
 * this set exposes progression + GAS-modifiable stats (Stamina, AttackPower,
 * Defense, Experience, Level) so gameplay effects can modify them cleanly.
 *
 * [INPUT]  From: UDCProgressionComponent → SetExperience / SetLevel
 * [INPUT]  From: Gameplay Effects (buffs, consumables)
 * [OUTPUT] To:   UDCProgressionComponent — OnAttributeChanged(Experience) triggers level-up check
 * [OUTPUT] To:   Any GAS ability / UI bound via GetExperienceAttribute()
 *
 * [DEPENDS ON] UAbilitySystemComponent on ADCCharacterBase
 */
UCLASS()
class DELTACODE_API UDCAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UDCAttributeSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute,
	                                float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	// ── Stamina ─────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Attributes|Stamina")
	FGameplayAttributeData Stamina;
	DC_ATTRIBUTE_ACCESSORS(UDCAttributeSet, Stamina)

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Attributes|Stamina")
	FGameplayAttributeData MaxStamina;
	DC_ATTRIBUTE_ACCESSORS(UDCAttributeSet, MaxStamina)

	// ── Combat ──────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Attributes|Combat")
	FGameplayAttributeData AttackPower;
	DC_ATTRIBUTE_ACCESSORS(UDCAttributeSet, AttackPower)

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Attributes|Combat")
	FGameplayAttributeData Defense;
	DC_ATTRIBUTE_ACCESSORS(UDCAttributeSet, Defense)

	// ── Progression ─────────────────────────────────────────────────────────
	// Experience is a running total. Level is advanced by UDCProgressionComponent
	// when Experience crosses a threshold from UDCLevelCurve.

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Attributes|Progression")
	FGameplayAttributeData Experience;
	DC_ATTRIBUTE_ACCESSORS(UDCAttributeSet, Experience)

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Attributes|Progression")
	FGameplayAttributeData Level;
	DC_ATTRIBUTE_ACCESSORS(UDCAttributeSet, Level)
};
