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
#include "Actors/DCCharacterBase.h"
#include "DCThirdPersonCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 * Third-person player character. Adds a spring arm + follow camera on top of
 * ADCCharacterBase. Character rotation is controlled by movement direction;
 * the controller yaw drives the spring arm only.
 *
 * Designers derive B_DC_ThirdPersonCharacter (or a project-specific variant)
 * from this class and assign it as the default pawn in ADCGameModeBase.
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API ADCThirdPersonCharacter : public ADCCharacterBase
{
	GENERATED_BODY()

public:
	ADCThirdPersonCharacter();

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Camera")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Camera")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	// Default boom length for this character. Exposed so a Blueprint child can
	// tune camera distance without needing to touch the native subobject.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DeltaCode|Camera")
	float DefaultCameraArmLength = 400.0f;
};
