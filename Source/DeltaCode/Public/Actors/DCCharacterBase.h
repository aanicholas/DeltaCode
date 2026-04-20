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
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Interfaces/DCDamageable.h"
#include "Types/DCCoreTypes.h"
#include "DCCharacterBase.generated.h"

class UAbilitySystemComponent;
class UInputMappingContext;
class UInputAction;
class UDCEquipmentComponent;
class UDCHealthComponent;
class UDCAttributeSet;

/**
 * Base character for all DeltaCode player pawns.
 * Integrates Enhanced Input, GAS, and the IDCDamageable interface.
 * Movement, camera, and mesh are configured in Blueprint children.
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API ADCCharacterBase : public ACharacter,
                                       public IAbilitySystemInterface,
                                       public IDCDamageable
{
	GENERATED_BODY()

public:
	ADCCharacterBase();

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// IDCDamageable
	virtual void ApplyDamage_Implementation(float DamageAmount, AActor* DamageSource,
	                                        AActor* DamageCauser) override;
	virtual bool IsDead_Implementation() const override;

	// Base stats — overridden per character Blueprint
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Stats")
	FDCStatBlock BaseStats;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Components")
	UDCEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Components")
	UDCHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Components")
	UDCAttributeSet* GetAttributeSet() const { return AttributeSet; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// Enhanced Input — set in Blueprint or via UDCInputConfig (Layer 2)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Input")
	TObjectPtr<UInputAction> JumpAction;

	// Enhanced Input callbacks
	void OnMoveTriggered(const struct FInputActionValue& Value);
	void OnLookTriggered(const struct FInputActionValue& Value);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDCEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDCHealthComponent> HealthComponent;

	// GAS attribute set — owned subobject so the ASC discovers it automatically
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDCAttributeSet> AttributeSet;
};
