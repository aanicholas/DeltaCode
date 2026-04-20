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
#include "DCBTTask_MoveToTarget.generated.h"

/**
 * Moves the pawn to the blackboard's TargetActor. If the key is empty or the
 * actor has been destroyed, the task fails immediately so a selector can fall
 * through to wander/investigate branches.
 */
UCLASS(MinimalAPI, meta = (DisplayName = "DC Move To Target"))
class UBTTask_DCMoveToTarget : public UBTTask_DCMoveBase
{
	GENERATED_BODY()

public:
	UBTTask_DCMoveToTarget();

protected:
	virtual bool ResolveDestination(AAIController& AI,
	                                APawn& Pawn,
	                                UBehaviorTreeComponent& OwnerComp,
	                                AActor*& OutTargetActor,
	                                FVector& OutTargetLocation) const override;
};
