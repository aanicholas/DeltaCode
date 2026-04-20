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
#include "Actors/DCDualPerspectiveCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

ADCDualPerspectiveCharacter::ADCDualPerspectiveCharacter()
{
	// First-person camera mounted on the capsule at eye height. The follow
	// camera is inherited from ADCThirdPersonCharacter and remains the default.
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FirstPersonCameraOffset);
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetAutoActivate(false);

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(FirstPersonCamera);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->CastShadow = false;
	FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADCDualPerspectiveCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Reset any design-time-serialised state and apply the authored initial view.
	CurrentPerspective = (InitialCameraMode == EDCCameraMode::FirstPerson)
		? EDCCameraMode::FirstPerson
		: EDCCameraMode::ThirdPerson;
	ApplyCurrentPerspective();
}

void ADCDualPerspectiveCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!TogglePerspectiveAction)
	{
		return;
	}
	if (UEnhancedInputComponent* EnhancedInput =
		Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInput->BindAction(TogglePerspectiveAction, ETriggerEvent::Started,
			this, &ADCDualPerspectiveCharacter::OnTogglePerspectiveInput);
	}
}

void ADCDualPerspectiveCharacter::OnTogglePerspectiveInput(const FInputActionValue& /*Value*/)
{
	// Designer-locked story modes gate the runtime toggle here; C++ / BP calls
	// to TogglePerspective() still work for scripted sequences.
	if (!bAllowPlayerPerspectiveToggle)
	{
		return;
	}
	TogglePerspective();
}

void ADCDualPerspectiveCharacter::TogglePerspective()
{
	const EDCCameraMode NextMode = (CurrentPerspective == EDCCameraMode::FirstPerson)
		? EDCCameraMode::ThirdPerson
		: EDCCameraMode::FirstPerson;
	SetPerspective(NextMode);
}

void ADCDualPerspectiveCharacter::SetPerspective(EDCCameraMode NewMode)
{
	// DualPerspective isn't a runtime view; collapse onto ThirdPerson.
	if (NewMode == EDCCameraMode::DualPerspective)
	{
		NewMode = EDCCameraMode::ThirdPerson;
	}
	if (NewMode == CurrentPerspective)
	{
		return;
	}
	CurrentPerspective = NewMode;
	ApplyCurrentPerspective();
	OnPerspectiveChanged.Broadcast(CurrentPerspective);
}

void ADCDualPerspectiveCharacter::ApplyCurrentPerspective()
{
	UCameraComponent* Boom = GetFollowCamera();
	const bool bFirstPerson = (CurrentPerspective == EDCCameraMode::FirstPerson);

	if (FirstPersonCamera) { FirstPersonCamera->SetActive(bFirstPerson); }
	if (Boom)              { Boom->SetActive(!bFirstPerson); }

	// Hide the third-person body from the owner while in first person so the
	// player doesn't see a torso clipping through the camera. Other clients
	// still see the full body.
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetOwnerNoSee(bFirstPerson);
	}

	// [OUTPUT] To: PlayerCameraManager — SetViewTargetWithBlend applies
	// PerspectiveBlendTime using the engine's default blend curve.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FViewTargetTransitionParams Params;
		Params.BlendTime = PerspectiveBlendTime;
		Params.BlendFunction = VTBlend_Cubic;
		PC->SetViewTarget(this, Params);
	}
}
