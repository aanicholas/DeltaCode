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
#include "AIController.h"
#include "GenericTeamAgentInterface.h"
#include "DCEnemyAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UBehaviorTreeComponent;
class UBlackboardComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCOnTargetPerceived,
	AActor*, NewTarget, AActor*, OldTarget);

/**
 * Base AI controller for DeltaCode enemies. Owns an AIPerceptionComponent
 * pre-wired with a sight sense and exposes the currently perceived target.
 *
 * Subclass behaviour (behaviour tree, attack cadence, flee logic) is layered
 * on top — this base only answers "who am I chasing right now?".
 *
 * [INPUT]  From: UAIPerceptionComponent — sight updates
 * [OUTPUT] To:   ADCEnemyBase (via OnTargetPerceived) — triggers aggro behaviour
 */
UCLASS(Blueprintable)
class DELTACODE_API ADCEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	ADCEnemyAIController();

	UFUNCTION(BlueprintPure, Category = "DeltaCode|AI")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|AI")
	bool HasTarget() const { return CurrentTarget.IsValid(); }

	// ── Sight configuration ─────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|AI|Sight")
	float SightRadius = 2500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|AI|Sight")
	float LoseSightRadius = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|AI|Sight")
	float PeripheralVisionAngle = 90.0f;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|AI")
	FDCOnTargetPerceived OnTargetPerceived;

	// ── IGenericTeamAgentInterface (inherited from AAIController) ──────────
	// Team id is read from the possessed pawn's UDCFactionComponent on possess.
	// GetTeamAttitudeTowards consults the faction's DefaultStances so AI
	// perception sees hostiles / friendlies / neutrals without needing its own
	// affiliation table.
	virtual FGenericTeamId GetGenericTeamId() const override { return AssignedTeamId; }
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionComponent> DCPerceptionComponent;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	// ── Behavior tree / blackboard ──────────────────────────────────────────
	//
	// BT + Blackboard live on the controller (not the pawn) so possessed-by-
	// player takeover or AI death leaves them in a clean state. The pawn
	// (ADCEnemyBase) owns the BehaviorTree asset pointer — the controller just
	// runs whatever tree the pawn specifies at possession time.

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBlackboardComponent> BlackboardComponent;

private:
	UFUNCTION()
	void HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	TWeakObjectPtr<AActor> CurrentTarget;

	// Cached team id — defaults to NoTeam until OnPossess reads from the pawn's
	// UDCFactionComponent (or an explicit SetGenericTeamId call overrides it).
	FGenericTeamId AssignedTeamId = FGenericTeamId::NoTeam;
};
