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
#include "Actors/DCCharacterBase.h"
#include "Components/DCEquipmentComponent.h"
#include "Components/DCHealthComponent.h"
#include "GAS/DCAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ADCCharacterBase::ADCCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(
		TEXT("AbilitySystemComponent"));

	EquipmentComponent = CreateDefaultSubobject<UDCEquipmentComponent>(
		TEXT("EquipmentComponent"));

	HealthComponent = CreateDefaultSubobject<UDCHealthComponent>(
		TEXT("HealthComponent"));

	// AttributeSet must be a default subobject so the ASC's GetSet<>() can find it.
	AttributeSet = CreateDefaultSubobject<UDCAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* ADCCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float ADCCharacterBase::ApplyDamageTier_Implementation(EDCDamageTier Tier,
                                                        const FGameplayTagContainer& SourceTags,
                                                        AActor* Instigator,
                                                        AActor* Causer)
{
	// [OUTPUT] To: UDCHealthComponent — authoritative damage bookkeeping.
	// Lethal short-circuits via Kill(); Magnitude returned reflects the
	// pre-clamp delta so callers (UI hit numbers) see the intended hit.
	if (!HealthComponent || HealthComponent->IsDead() || Tier == EDCDamageTier::None)
	{
		return 0.0f;
	}

	if (Tier == EDCDamageTier::Lethal)
	{
		const float Headroom = HealthComponent->GetCurrentHealth();
		HealthComponent->Kill(Instigator, Causer);
		return Headroom;
	}

	const FDCDamageConfig Config;
	const float Magnitude = Config.ResolveMagnitude(Tier);
	HealthComponent->ApplyDamage(Magnitude, Instigator, Causer);
	return Magnitude;
}

bool ADCCharacterBase::CanReceiveDamage_Implementation() const
{
	return HealthComponent && !HealthComponent->IsDead();
}

void ADCCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// Establish ASC ownership + register the attribute set + seed from BaseStats
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		if (AttributeSet)
		{
			AbilitySystemComponent->AddSpawnedAttribute(AttributeSet);
			AttributeSet->SetAttackPower(BaseStats.AttackPower);
			AttributeSet->SetDefense(BaseStats.Defense);
		}
	}

	// Add default input mapping context via Enhanced Input subsystem
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void ADCCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput =
		Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered,
				this, &ADCCharacterBase::OnMoveTriggered);
		}
		if (LookAction)
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered,
				this, &ADCCharacterBase::OnLookTriggered);
		}
		if (JumpAction)
		{
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started,
				this, &ACharacter::Jump);
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed,
				this, &ACharacter::StopJumping);
		}
	}
}

void ADCCharacterBase::OnMoveTriggered(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection =
			FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection =
			FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ADCCharacterBase::OnLookTriggered(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}
