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
#include "DCInteractionComponent.generated.h"

// ─── Detection mode ──────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EDCInteractionDetectionMode : uint8
{
	// Line trace forward from the player's camera (first person / over-shoulder)
	LineTraceFromCamera  UMETA(DisplayName = "Line Trace From Camera"),
	// Sphere overlap around the controlled pawn (top-down / proximity games)
	OverlapAroundPawn    UMETA(DisplayName = "Overlap Around Pawn"),
};

// ─── Delegates ───────────────────────────────────────────────────────────────

// Fires when the focused interactable target changes (may be null when lost)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCOnInteractionFocusChanged,
	AActor*, NewTarget, const FText&, PromptText);

// Fires immediately after the player successfully interacts with a target
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnInteracted, AActor*, Target);

/**
 * Detects IDCInteractable actors in front of or around the player and dispatches
 * interaction events. Attaches to ADCPlayerControllerBase.
 *
 * [INPUT]  From: ADCPlayerControllerBase → via Enhanced Input IA_DC_Interact action
 * [OUTPUT] To:   IDCInteractable::Execute_OnInteracted() on the focused target
 * [OUTPUT] To:   ADCHUDBase (via OnFocusChanged delegate) → shows/hides interact prompt
 */
UCLASS(ClassGroup = (DeltaCode), BlueprintType,
       meta = (BlueprintSpawnableComponent))
class DELTACODE_API UDCInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCInteractionComponent();

	// ── Detection config ────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Interaction")
	EDCInteractionDetectionMode DetectionMode = EDCInteractionDetectionMode::LineTraceFromCamera;

	// Maximum reach distance for line trace (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Interaction",
	          meta = (ClampMin = "1.0", EditCondition =
	                  "DetectionMode == EDCInteractionDetectionMode::LineTraceFromCamera"))
	float TraceDistance = 300.0f;

	// Radius for overlap detection (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Interaction",
	          meta = (ClampMin = "1.0", EditCondition =
	                  "DetectionMode == EDCInteractionDetectionMode::OverlapAroundPawn"))
	float OverlapRadius = 200.0f;

	// How often to re-scan for targets (seconds). 0 disables periodic scan.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Interaction",
	          meta = (ClampMin = "0.0"))
	float ScanInterval = 0.1f;

	// Collision channel used for detection
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	// Debug draw the detection volume / trace
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Interaction|Debug")
	bool bDrawDebug = false;

	// ── Delegates ───────────────────────────────────────────────────────────

	// [EVENT] Fires: OnFocusChanged — consumed by ADCHUDBase (prompt show/hide)
	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Interaction")
	FDCOnInteractionFocusChanged OnFocusChanged;

	// [EVENT] Fires: OnInteracted — consumed by quest subsystem, analytics, etc.
	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Interaction")
	FDCOnInteracted OnInteracted;

	// ── API ─────────────────────────────────────────────────────────────────

	// Called by the player controller when the interact input fires.
	// [INPUT] From: ADCPlayerControllerBase → Enhanced Input IA_DC_Interact action
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Interaction")
	void TryInteract();

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Interaction")
	AActor* GetFocusedTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Interaction")
	bool HasFocusedTarget() const { return CurrentTarget.IsValid(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Run a detection scan; called by the repeating timer
	void ScanForTarget();

	// Line trace detection implementation
	AActor* PerformCameraTrace() const;

	// Overlap sphere detection implementation
	AActor* PerformOverlapScan() const;

	// Accept a new target; fires OnFocusChanged if the target changed
	void SetFocusedTarget(AActor* NewTarget);

	// Retrieve the owning player controller (cached)
	class APlayerController* GetOwningController() const;

	TWeakObjectPtr<AActor> CurrentTarget;

	FTimerHandle ScanTimerHandle;
};
