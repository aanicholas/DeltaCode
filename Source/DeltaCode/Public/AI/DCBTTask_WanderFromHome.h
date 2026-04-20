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
#include "AI/DCBTTask_MoveBase.h"
#include "DCBTTask_WanderFromHome.generated.h"

/**
 * Picks a random navmesh point within WanderRadius of the HomeLocation
 * blackboard vector (or the pawn's current location if HomeLocation isn't set)
 * and moves there. One wander hop per execution — designers loop it via a
 * Selector / Wait pair in the BT.
 */
UCLASS(MinimalAPI, meta = (DisplayName = "DC Wander From Home"))
class UBTTask_DCWanderFromHome : public UBTTask_DCMoveBase
{
	GENERATED_BODY()

public:
	UBTTask_DCWanderFromHome();

protected:
	virtual bool ResolveDestination(AAIController& AI,
	                                APawn& Pawn,
	                                UBehaviorTreeComponent& OwnerComp,
	                                AActor*& OutTargetActor,
	                                FVector& OutTargetLocation) const override;

	/** Max distance a single wander hop may take the pawn from HomeLocation. */
	UPROPERTY(EditAnywhere, Category = "Wander",
	          meta = (ClampMin = "50.0"))
	float WanderRadius = 800.0f;
};
