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
#include "UObject/Interface.h"
#include "DCEnemyAIPawn.generated.h"

class UBehaviorTree;

UINTERFACE(BlueprintType, MinimalAPI)
class UDCEnemyAIPawn : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contract a pawn must implement for ADCEnemyAIController to drive it.
 *
 * Decouples the controller from any concrete pawn class so a pawn parented
 * off ALyraCharacter (B_DC_LyraEnemyBase) can run the same BT/BB pair as a
 * pawn parented off ADCEnemyBase. Implement natively when the BT field lives
 * on a C++ UPROPERTY (ADCEnemyBase), or override in Blueprint when the field
 * is added on a BP-only enemy class.
 */
class DELTACODE_API IDCEnemyAIPawn
{
	GENERATED_BODY()

public:
	/**
	 * Behaviour tree the controller should StartTree() at possession time.
	 * Return nullptr to opt out of BT-driven AI for this pawn.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DeltaCode|AI")
	UBehaviorTree* GetEnemyBehaviorTree() const;
	virtual UBehaviorTree* GetEnemyBehaviorTree_Implementation() const { return nullptr; }
};
