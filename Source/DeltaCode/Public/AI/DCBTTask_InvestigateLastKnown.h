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
#include "DCBTTask_InvestigateLastKnown.generated.h"

/**
 * Moves the pawn to the blackboard's LastKnownLocation vector. Designed for a
 * branch like: "lost sight → investigate the spot we last saw the target → if
 * still no target, fall back to wandering near home."
 *
 * Also flips the bIsInvestigating flag on entry/exit so other branches can
 * observe the pawn's alert state.
 */
UCLASS(MinimalAPI, meta = (DisplayName = "DC Investigate Last Known"))
class UBTTask_DCInvestigateLastKnown : public UBTTask_DCMoveBase
{
	GENERATED_BODY()

public:
	UBTTask_DCInvestigateLastKnown();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
	                                        uint8* NodeMemory) override;

	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp,
	                            uint8* NodeMemory,
	                            EBTNodeResult::Type TaskResult) override;

protected:
	virtual bool ResolveDestination(AAIController& AI,
	                                APawn& Pawn,
	                                UBehaviorTreeComponent& OwnerComp,
	                                AActor*& OutTargetActor,
	                                FVector& OutTargetLocation) const override;
};
