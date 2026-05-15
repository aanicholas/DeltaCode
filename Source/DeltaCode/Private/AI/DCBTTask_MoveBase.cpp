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
#include "AI/DCBTTask_MoveBase.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "GameFramework/Pawn.h"

UBTTask_DCMoveBase::UBTTask_DCMoveBase()
{
	NodeName = TEXT("DC Move Base");
	// Per-instance state: we bind a UFUNCTION delegate and track FAIRequestID.
	bCreateNodeInstance = true;
	bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBTTask_DCMoveBase::ExecuteTask(UBehaviorTreeComponent& OwnerComp,
                                                    uint8* /*NodeMemory*/)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	if (!AI)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = AI->GetPawn();
	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	AActor* TargetActor = nullptr;
	FVector TargetLocation = FVector::ZeroVector;
	if (!ResolveDestination(*AI, *Pawn, OwnerComp, TargetActor, TargetLocation))
	{
		return EBTNodeResult::Failed;
	}

	FAIMoveRequest MoveReq;
	MoveReq.SetAcceptanceRadius(AcceptanceRadius);
	MoveReq.SetUsePathfinding(bUsePathfinding);
	MoveReq.SetAllowPartialPath(true);
	MoveReq.SetReachTestIncludesAgentRadius(true);
	MoveReq.SetCanStrafe(false);
	if (TargetActor)
	{
		MoveReq.SetGoalActor(TargetActor);
	}
	else
	{
		MoveReq.SetGoalLocation(TargetLocation);
	}

	FPathFollowingRequestResult Request = AI->MoveTo(MoveReq);
	switch (Request.Code)
	{
	case EPathFollowingRequestResult::AlreadyAtGoal:
		return EBTNodeResult::Succeeded;

	case EPathFollowingRequestResult::Failed:
		return EBTNodeResult::Failed;

	case EPathFollowingRequestResult::RequestSuccessful:
	{
		// Stash state + subscribe to completion so we can finish latently.
		PendingRequestID    = Request.MoveId;
		CachedOwnerComp     = &OwnerComp;
		CachedAIController  = AI;

		AI->ReceiveMoveCompleted.AddDynamic(this, &UBTTask_DCMoveBase::HandleMoveFinished);
		return EBTNodeResult::InProgress;
	}

	default:
		return EBTNodeResult::Failed;
	}
}

EBTNodeResult::Type UBTTask_DCMoveBase::AbortTask(UBehaviorTreeComponent& OwnerComp,
                                                  uint8* /*NodeMemory*/)
{
	if (AAIController* AI = OwnerComp.GetAIOwner())
	{
		if (PendingRequestID.IsValid())
		{
			AI->StopMovement();
		}
	}
	// OnTaskFinished handles the delegate cleanup.
	return EBTNodeResult::Aborted;
}

void UBTTask_DCMoveBase::OnTaskFinished(UBehaviorTreeComponent& /*OwnerComp*/,
                                        uint8* /*NodeMemory*/,
                                        EBTNodeResult::Type /*TaskResult*/)
{
	if (AAIController* AI = CachedAIController.Get())
	{
		AI->ReceiveMoveCompleted.RemoveDynamic(
			this, &UBTTask_DCMoveBase::HandleMoveFinished);
	}
	CachedOwnerComp.Reset();
	CachedAIController.Reset();
	PendingRequestID = FAIRequestID::InvalidRequest;
}

void UBTTask_DCMoveBase::HandleMoveFinished(FAIRequestID RequestID,
                                            EPathFollowingResult::Type Result)
{
	// Ignore stale completions from a previous request on the same controller.
	if (!PendingRequestID.IsValid() || RequestID != PendingRequestID)
	{
		return;
	}

	UBehaviorTreeComponent* OwnerComp = CachedOwnerComp.Get();
	if (!OwnerComp)
	{
		return;
	}

	const bool bSucceeded = (Result == EPathFollowingResult::Success);
	FinishLatentTask(*OwnerComp,
		bSucceeded ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
}
