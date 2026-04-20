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
#include "DCFirstPersonCharacter.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;

/**
 * First-person player character. Adds a camera at eye height and a
 * first-person arms mesh slot that designers can populate in a Blueprint
 * child. Character yaw follows the controller so the body aims where the
 * camera looks.
 *
 * The inherited third-person Mesh is hidden from the owning player via
 * bOwnerNoSee so other clients still see a full body while the owner sees
 * only the first-person arms.
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API ADCFirstPersonCharacter : public ADCCharacterBase
{
	GENERATED_BODY()

public:
	ADCFirstPersonCharacter();

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Camera")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Camera")
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// First-person arms / weapon-only mesh. Attached to the camera so it
	// follows the view exactly. Owner-only visibility so spectators don't see
	// the floating arms.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	// Eye-height offset from the capsule centre. Default matches UE's default
	// BaseEyeHeight; a Blueprint child can override per body type.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DeltaCode|Camera")
	FVector CameraOffset = FVector(0.0f, 0.0f, 64.0f);
};
