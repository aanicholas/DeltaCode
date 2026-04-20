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
#include "Components/DCInteractionComponent.h"
#include "Interfaces/DCInteractable.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

UDCInteractionComponent::UDCInteractionComponent()
{
	// Timer-driven — no per-frame tick needed
	PrimaryComponentTick.bCanEverTick = false;
}

void UDCInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// [DEPENDS ON] Owner must be a PlayerController (or contain one up the chain)
	if (ScanInterval > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(ScanTimerHandle, this,
			&UDCInteractionComponent::ScanForTarget, ScanInterval, true);
	}
}

void UDCInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScanTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

APlayerController* UDCInteractionComponent::GetOwningController() const
{
	return Cast<APlayerController>(GetOwner());
}

void UDCInteractionComponent::ScanForTarget()
{
	AActor* NewTarget = nullptr;

	switch (DetectionMode)
	{
		case EDCInteractionDetectionMode::LineTraceFromCamera:
			NewTarget = PerformCameraTrace();
			break;
		case EDCInteractionDetectionMode::OverlapAroundPawn:
			NewTarget = PerformOverlapScan();
			break;
	}

	// Filter non-interactables out
	if (NewTarget && !NewTarget->Implements<UDCInteractable>())
	{
		NewTarget = nullptr;
	}

	// Respect CanInteract() contract
	if (NewTarget)
	{
		APawn* InteractorPawn = GetOwningController() ?
			GetOwningController()->GetPawn() : nullptr;
		if (!IDCInteractable::Execute_CanInteract(NewTarget, InteractorPawn))
		{
			NewTarget = nullptr;
		}
	}

	SetFocusedTarget(NewTarget);
}

AActor* UDCInteractionComponent::PerformCameraTrace() const
{
	APlayerController* PC = GetOwningController();
	if (!PC)
	{
		return nullptr;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * TraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DCInteractionTrace), false);
	if (APawn* OwnerPawn = PC->GetPawn())
	{
		Params.AddIgnoredActor(OwnerPawn);
	}
	Params.AddIgnoredActor(PC);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit, CameraLocation, TraceEnd, TraceChannel, Params);

	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), CameraLocation, TraceEnd,
			bHit ? FColor::Green : FColor::Red, false, ScanInterval, 0, 1.0f);
	}

	return bHit ? Hit.GetActor() : nullptr;
}

AActor* UDCInteractionComponent::PerformOverlapScan() const
{
	APlayerController* PC = GetOwningController();
	if (!PC)
	{
		return nullptr;
	}

	APawn* OwnerPawn = PC->GetPawn();
	if (!OwnerPawn)
	{
		return nullptr;
	}

	const FVector Origin = OwnerPawn->GetActorLocation();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DCInteractionOverlap), false);
	Params.AddIgnoredActor(OwnerPawn);
	Params.AddIgnoredActor(PC);

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity,
		TraceChannel, FCollisionShape::MakeSphere(OverlapRadius), Params);

	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Origin, OverlapRadius, 16,
			Overlaps.Num() > 0 ? FColor::Green : FColor::Red, false, ScanInterval);
	}

	// Pick the nearest interactable
	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const FOverlapResult& Result : Overlaps)
	{
		AActor* Actor = Result.GetActor();
		if (!Actor || !Actor->Implements<UDCInteractable>())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Origin, Actor->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Actor;
		}
	}
	return Best;
}

void UDCInteractionComponent::SetFocusedTarget(AActor* NewTarget)
{
	AActor* OldTarget = CurrentTarget.Get();
	if (OldTarget == NewTarget)
	{
		return;
	}

	CurrentTarget = NewTarget;

	FText Prompt = FText::GetEmpty();
	if (NewTarget)
	{
		Prompt = IDCInteractable::Execute_GetInteractionPrompt(NewTarget);
	}

	// [OUTPUT] To: ADCHUDBase → shows/hides interact prompt widget
	OnFocusChanged.Broadcast(NewTarget, Prompt);
}

void UDCInteractionComponent::TryInteract()
{
	AActor* Target = CurrentTarget.Get();
	if (!Target)
	{
		return;
	}

	APawn* InteractorPawn = GetOwningController() ?
		GetOwningController()->GetPawn() : nullptr;

	if (!IDCInteractable::Execute_CanInteract(Target, InteractorPawn))
	{
		return;
	}

	// [OUTPUT] To: IDCInteractable::Execute_OnInteracted on the target actor
	IDCInteractable::Execute_OnInteracted(Target, InteractorPawn);

	// [OUTPUT] To: Quest subsystem, analytics, achievement trackers
	OnInteracted.Broadcast(Target);
}
