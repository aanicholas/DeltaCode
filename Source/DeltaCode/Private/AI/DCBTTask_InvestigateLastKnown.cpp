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

#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_DCInvestigateLastKnown::UBTTask_DCInvestigateLastKnown()
{
	NodeName = TEXT("DC Investigate Last Known");
	AcceptanceRadius = 100.0f;
}

EBTNodeResult::Type UBTTask_DCInvestigateLastKnown::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
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
