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
#include "AI/DCBTTask_MoveToTarget.h"
#include "AI/DCAIBlackboardKeys.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_DCMoveToTarget::UBTTask_DCMoveToTarget()
{
	NodeName = TEXT("DC Move To Target");
	// Default acceptance radius sits just outside melee range; ranged enemies
	// will typically override in their BT node details panel.
	AcceptanceRadius = 120.0f;
}

bool UBTTask_DCMoveToTarget::ResolveDestination(AAIController& /*AI*/,
                                                APawn& /*Pawn*/,
                                                UBehaviorTreeComponent& OwnerComp,
                                                AActor*& OutTargetActor,
                                                FVector& /*OutTargetLocation*/) const
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB)
	{
		return false;
	}

	UObject* Raw = BB->GetValueAsObject(DCAIBlackboardKeys::TargetActor);
	AActor* Target = Cast<AActor>(Raw);
	if (!IsValid(Target))
	{
		return false;
	}

	OutTargetActor = Target;
	return true;
}
