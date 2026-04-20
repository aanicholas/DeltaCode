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
#include "AI/DCBTTask_AttackTarget.h"
#include "AI/DCAIBlackboardKeys.h"

#include "AIController.h"
#include "Actors/DCCharacterBase.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/DCEquipmentComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetMathLibrary.h"

UBTTask_DCAttackTarget::UBTTask_DCAttackTarget()
{
	NodeName = TEXT("DC Attack Target");
}

EBTNodeResult::Type UBTTask_DCAttackTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp,
                                                        uint8* /*NodeMemory*/)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	if (!AI)
	{
		return EBTNodeResult::Failed;
	}

	ADCCharacterBase* Character = Cast<ADCCharacterBase>(AI->GetPawn());
	if (!Character)
	{
		return EBTNodeResult::Failed;
	}

	UDCEquipmentComponent* Equipment = Character->GetEquipmentComponent();
	if (!Equipment)
	{
		return EBTNodeResult::Failed;
	}

	// Face the target before firing so the weapon's aim direction lines up.
	// The BT's parallel MoveToTarget usually keeps us roughly oriented, but a
	// stationary attack (e.g. turret archetype) still needs this.
	if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
	{
		if (AActor* Target = Cast<AActor>(BB->GetValueAsObject(DCAIBlackboardKeys::TargetActor)))
		{
			const FRotator Look = UKismetMathLibrary::FindLookAtRotation(
				Character->GetActorLocation(), Target->GetActorLocation());
			// Yaw-only so we don't tip the mesh when the player is above/below.
			Character->SetActorRotation(FRotator(0.0f, Look.Yaw, 0.0f));
		}
	}

	return Equipment->FireEquipped()
		? EBTNodeResult::Succeeded
		: EBTNodeResult::Failed;
}
