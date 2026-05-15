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
#include "Components/ActorComponent.h"
#include "DCEnemyAIData.generated.h"

class UBehaviorTree;

/**
 * Per-enemy AI configuration carried on a pawn that can't expose a
 * BehaviorTree UPROPERTY natively (i.e. BP-only enemies parented off a
 * non-DeltaCode C++ class such as ALyraCharacter).
 *
 * ADCEnemyAIController falls back to FindComponentByClass<UDCEnemyAIData>()
 * when IDCEnemyAIPawn doesn't return a tree, so any Lyra-parented enemy BP
 * with this component attached and BehaviorTree set in its class defaults
 * runs the same BT/BB pair as ADCEnemyBase — no concrete-class cast needed.
 *
 * ADCEnemyBase exposes BehaviorTree directly as a UPROPERTY (and implements
 * IDCEnemyAIPawn natively) so it does NOT need this component. Adding it is
 * harmless but redundant.
 */
UCLASS(ClassGroup = (DeltaCode), meta = (BlueprintSpawnableComponent))
class DELTACODE_API UDCEnemyAIData : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCEnemyAIData();

	/**
	 * Behaviour tree the AI controller starts at possession time. Points at
	 * the same BT_DC_Enemy asset that ADCEnemyBase carries, but reachable
	 * for non-DCEnemyBase pawns via component lookup.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;
};
