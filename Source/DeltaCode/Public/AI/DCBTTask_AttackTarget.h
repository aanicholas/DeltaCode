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
#include "DCBTTask_AttackTarget.generated.h"

/**
 * Fires the pawn's equipped weapon once at the current TargetActor.
 *
 * Intentionally thin: the weapon owns its own cooldown/reload state, so this
 * task just asks "please attempt to fire" and reports back whether the weapon
 * accepted the request. Designers gate attack cadence with a Wait decorator
 * or a Cooldown decorator on the parent node.
 *
 * Fails if the pawn has no equipment component, no equipped weapon, or the
 * weapon rejected the fire request (on cooldown / out of ammo / mid-reload).
 */
UCLASS(MinimalAPI, meta = (DisplayName = "DC Attack Target"))
class UBTTask_DCAttackTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_DCAttackTarget();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
	                                        uint8* NodeMemory) override;
};
