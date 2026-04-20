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
#include "GameFramework/Actor.h"
#include "DCCheckpointVolume.generated.h"

class UBoxComponent;
class ADCPlayerControllerBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnCheckpointTriggered,
	ADCPlayerControllerBase*, Player);

/**
 * Placed-in-level trigger that saves the game when the player pawn overlaps it.
 * Designed for hand-authored save points (bonfires, beds, elevator landings).
 *
 * [INPUT]  From: player pawn overlap (ECC_Pawn box trace)
 * [OUTPUT] To:   UDCGameInstance::SaveGameToDisk
 * [EVENT]  Fires: OnCheckpointTriggered — UI plays a "Saved" toast
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API ADCCheckpointVolume : public AActor
{
	GENERATED_BODY()

public:
	ADCCheckpointVolume();

	/** Trigger volume. Set Collision → Query, ECC_Pawn overlap on the BP. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeltaCode|Checkpoint")
	TObjectPtr<UBoxComponent> Trigger;

	/** If true, the volume fires at most once per level load. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Checkpoint")
	bool bOneShot = false;

	/** Minimum seconds between triggers. Ignored when bOneShot is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Checkpoint",
	          meta = (ClampMin = "0.0"))
	float CooldownSeconds = 30.0f;

	/** Optional tag written into the save's narrative state when tripped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Checkpoint")
	FName CheckpointID = NAME_None;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Checkpoint")
	FDCOnCheckpointTriggered OnCheckpointTriggered;

	/** Manual trigger entry point — UI "Save Here" buttons call this directly. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Checkpoint")
	bool TryTrigger(ADCPlayerControllerBase* Player);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                        bool bFromSweep, const FHitResult& SweepResult);

private:
	bool bHasFired = false;
	// Initialized well below any plausible world time so the first overlap always fires.
	float LastTriggerTimeSeconds = -1.0e8f;
};
