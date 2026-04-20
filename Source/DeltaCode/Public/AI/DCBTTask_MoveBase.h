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
#include "BehaviorTree/BTTaskNode.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "DCBTTask_MoveBase.generated.h"

class AAIController;
class APawn;

/**
 * Shared base for DeltaCode BT move tasks. Handles the mechanics of issuing a
 * move request to the AIController and latching on to ReceiveMoveCompleted so
 * the task finishes when the pawn arrives (or fails to).
 *
 * Derived classes only need to answer one question: "where should we go?" via
 * ResolveDestination. The base handles:
 *   - kicking off MoveToActor / MoveToLocation
 *   - translating the initial EPathFollowingRequestResult into an EBTNodeResult
 *   - waiting on ReceiveMoveCompleted for the async outcome
 *   - cleaning up the delegate on task finish/abort
 *
 * bCreateNodeInstance = true because we rely on a UFUNCTION() delegate handler
 * and want per-BT-instance request IDs without juggling memory structs.
 *
 * [INPUT]  From: BehaviorTree authored by designer on ADCEnemyBase
 * [OUTPUT] To:   AAIController::MoveToActor / MoveToLocation
 */
UCLASS(Abstract)
class DELTACODE_API UBTTask_DCMoveBase : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_DCMoveBase();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
	                                        uint8* NodeMemory) override;

	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp,
	                                      uint8* NodeMemory) override;

	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp,
	                            uint8* NodeMemory,
	                            EBTNodeResult::Type TaskResult) override;

protected:
	/**
	 * Derived classes fill in either OutTargetActor (preferred — keeps the path
	 * updated if the actor moves) or OutTargetLocation. Return false to fail the
	 * task immediately (e.g., no valid destination could be computed this tick).
	 */
	virtual bool ResolveDestination(AAIController& AI,
	                                APawn& Pawn,
	                                UBehaviorTreeComponent& OwnerComp,
	                                AActor*& OutTargetActor,
	                                FVector& OutTargetLocation) const
		PURE_VIRTUAL(UBTTask_DCMoveBase::ResolveDestination, return false;);

	/** How close the pawn needs to be before the move counts as done. */
	UPROPERTY(EditAnywhere, Category = "Move")
	float AcceptanceRadius = 75.0f;

	/** When true and target is an actor, the path updates as the actor moves. */
	UPROPERTY(EditAnywhere, Category = "Move")
	bool bUsePathfinding = true;

private:
	UFUNCTION()
	void HandleMoveFinished(FAIRequestID RequestID, EPathFollowingResult::Type Result);

	/** Owning BT — cached so HandleMoveFinished can report back. */
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;

	/** Controller we subscribed to; needed to unsubscribe cleanly on abort/finish. */
	TWeakObjectPtr<AAIController> CachedAIController;

	/** ID of the in-flight request — only this request's completion finishes us. */
	FAIRequestID PendingRequestID;
};
