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
#include "Actors/DCThirdPersonCharacter.h"
#include "Types/DCCoreTypes.h"
#include "DCDualPerspectiveCharacter.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnPerspectiveChanged,
	EDCCameraMode, NewMode);

/**
 * Third-person character that can blend to a first-person view at runtime.
 *
 * Extends ADCThirdPersonCharacter (inheriting its spring arm + follow camera)
 * and adds a second UCameraComponent at eye height for the first-person view.
 * A dedicated Input Action (IA_DC_TogglePerspective) drives TogglePerspective()
 * at the player's request — the developer can lock that interaction by setting
 * bAllowPlayerPerspectiveToggle to false, in which case the toggle is only
 * reachable from C++ / Blueprint (scripted sequences, cinematics, etc.).
 *
 * Camera blending is handled via SetViewTargetWithBlend on the owning
 * PlayerController so the transition uses UE's built-in view-target blend
 * curve rather than a hand-rolled Tick-based interpolation.
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API ADCDualPerspectiveCharacter : public ADCThirdPersonCharacter
{
	GENERATED_BODY()

public:
	ADCDualPerspectiveCharacter();

	/** Initial view on possession. Also the view SetPerspective() resets to on recall. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DeltaCode|Camera")
	EDCCameraMode InitialCameraMode = EDCCameraMode::ThirdPerson;

	/**
	 * When true, IA_DC_TogglePerspective invokes TogglePerspective() directly.
	 * When false, only C++ / Blueprint gameplay code can swap perspectives —
	 * useful for story modes that should stay locked to one view.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DeltaCode|Camera")
	bool bAllowPlayerPerspectiveToggle = true;

	/** Blend duration between perspectives, in seconds. 0 = instant swap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DeltaCode|Camera",
	          meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float PerspectiveBlendTime = 0.35f;

	/** Input action that drives the runtime toggle. Assign to IA_DC_TogglePerspective. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Input")
	TObjectPtr<UInputAction> TogglePerspectiveAction;

	/** Swap between third- and first-person views using PerspectiveBlendTime. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Camera")
	void TogglePerspective();

	/** Directly set a perspective. DualPerspective is treated as ThirdPerson. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Camera")
	void SetPerspective(EDCCameraMode NewMode);

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Camera")
	EDCCameraMode GetCurrentPerspective() const { return CurrentPerspective; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Camera")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Camera")
	FDCOnPerspectiveChanged OnPerspectiveChanged;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// Optional first-person arms mesh. Visible only to the owner.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DeltaCode|Camera")
	FVector FirstPersonCameraOffset = FVector(0.0f, 0.0f, 64.0f);

private:
	// Enhanced Input handler — honours bAllowPlayerPerspectiveToggle.
	void OnTogglePerspectiveInput(const FInputActionValue& Value);

	/** Apply the camera + blend for CurrentPerspective. */
	void ApplyCurrentPerspective();

	EDCCameraMode CurrentPerspective = EDCCameraMode::ThirdPerson;
};
