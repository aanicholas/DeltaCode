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
#include "AI/DCBTTask_InvestigateLastKnown.h"
#include "AI/DCAIBlackboardKeys.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

UBTTask_DCInvestigateLastKnown::UBTTask_DCInvestigateLastKnown()
{
	NodeName = TEXT("DC Investigate Last Known");
	AcceptanceRadius = 100.0f;
}

EBTNodeResult::Type UBTTask_DCInvestigateLastKnown::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// TEMP DIAGNOSTIC (remove once movement is verified).
	UE_LOG(LogTemp, Display,
		TEXT("[DC-BT] Investigate::ExecuteTask enter on %s"),
		OwnerComp.GetAIOwner() && OwnerComp.GetAIOwner()->GetPawn()
			? *OwnerComp.GetAIOwner()->GetPawn()->GetName()
			: TEXT("<no pawn>"));

	if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
	{
		BB->SetValueAsBool(DCAIBlackboardKeys::bIsInvestigating, true);
	}
	return Super::ExecuteTask(OwnerComp, NodeMemory);
}

void UBTTask_DCInvestigateLastKnown::OnTaskFinished(UBehaviorTreeComponent& OwnerComp,
                                                    uint8* NodeMemory,
                                                    EBTNodeResult::Type TaskResult)
{
	// Clear the flag however we finish — aborted / failed / succeeded.
	if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
	{
		BB->SetValueAsBool(DCAIBlackboardKeys::bIsInvestigating, false);
	}
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

bool UBTTask_DCInvestigateLastKnown::ResolveDestination(AAIController& /*AI*/,
                                                        APawn& /*Pawn*/,
                                                        UBehaviorTreeComponent& OwnerComp,
                                                        AActor*& /*OutTargetActor*/,
                                                        FVector& OutTargetLocation) const
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB)
	{
		return false;
	}

	// IsVectorValueSet guards against the "never set" default of FVector::ZeroVector
	// masquerading as a real destination.
	if (!BB->IsVectorValueSet(DCAIBlackboardKeys::LastKnownLocation))
	{
		return false;
	}

	OutTargetLocation = BB->GetValueAsVector(DCAIBlackboardKeys::LastKnownLocation);
	return true;
}
