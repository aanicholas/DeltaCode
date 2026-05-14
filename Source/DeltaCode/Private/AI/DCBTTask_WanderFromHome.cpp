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
#include "AI/DCBTTask_WanderFromHome.h"
#include "AI/DCAIBlackboardKeys.h"

#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"

UBTTask_DCWanderFromHome::UBTTask_DCWanderFromHome()
{
	NodeName = TEXT("DC Wander From Home");
	AcceptanceRadius = 50.0f;
}

bool UBTTask_DCWanderFromHome::ResolveDestination(AAIController& /*AI*/,
                                                  APawn& Pawn,
                                                  UBehaviorTreeComponent& OwnerComp,
                                                  AActor*& /*OutTargetActor*/,
                                                  FVector& OutTargetLocation) const
{
	// TEMP DIAGNOSTIC (remove once movement is verified): logs every wander
	// resolve attempt so we can see whether the nav system is present, what
	// origin we're searching from, whether the radius query succeeds, and
	// the destination it picks. Tagged [DC-Wander] for easy log filtering.
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(Pawn.GetWorld());
	if (!Nav)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[DC-Wander] %s: NavigationSystemV1 is NULL on world %s"),
			*Pawn.GetName(),
			Pawn.GetWorld() ? *Pawn.GetWorld()->GetName() : TEXT("<null world>"));
		return false;
	}

	// Prefer HomeLocation if it was set at possession; otherwise fall back to
	// the pawn's current location so untethered enemies still wander.
	FVector Origin = Pawn.GetActorLocation();
	bool bHomeFromBB = false;
	if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
	{
		if (BB->IsVectorValueSet(DCAIBlackboardKeys::HomeLocation))
		{
			Origin = BB->GetValueAsVector(DCAIBlackboardKeys::HomeLocation);
			bHomeFromBB = true;
		}
	}

	FNavLocation RandomPoint;
	const bool bFound = Nav->GetRandomReachablePointInRadius(
		Origin, WanderRadius, RandomPoint);

	if (!bFound)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[DC-Wander] %s: GetRandomReachablePointInRadius FAILED. "
			     "Origin=%s (from %s), Radius=%.1f, PawnLoc=%s"),
			*Pawn.GetName(),
			*Origin.ToString(),
			bHomeFromBB ? TEXT("BB.HomeLocation") : TEXT("Pawn.GetActorLocation"),
			WanderRadius,
			*Pawn.GetActorLocation().ToString());
		return false;
	}

	UE_LOG(LogTemp, Display,
		TEXT("[DC-Wander] %s: OK. Origin=%s, Radius=%.1f, Dest=%s"),
		*Pawn.GetName(),
		*Origin.ToString(),
		WanderRadius,
		*RandomPoint.Location.ToString());

	OutTargetLocation = RandomPoint.Location;
	return true;
}
