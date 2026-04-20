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
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(Pawn.GetWorld());
	if (!Nav)
	{
		return false;
	}

	// Prefer HomeLocation if it was set at possession; otherwise fall back to
	// the pawn's current location so untethered enemies still wander.
	FVector Origin = Pawn.GetActorLocation();
	if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
	{
		if (BB->IsVectorValueSet(DCAIBlackboardKeys::HomeLocation))
		{
			Origin = BB->GetValueAsVector(DCAIBlackboardKeys::HomeLocation);
		}
	}

	FNavLocation RandomPoint;
	if (!Nav->GetRandomReachablePointInRadius(Origin, WanderRadius, RandomPoint))
	{
		return false;
	}

	OutTargetLocation = RandomPoint.Location;
	return true;
}
