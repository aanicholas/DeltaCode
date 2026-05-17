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
#include "Actors/DCEnemyAIController.h"
#include "AI/DCAIBlackboardKeys.h"
#include "AI/DCEnemyAIData.h"
#include "AI/DCEnemyAIPawn.h"
#include "Components/DCFactionComponent.h"
#include "Data/DCFactionDefinition.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "UObject/UnrealType.h"

namespace
{
	// Reflection-based Lyra hook. Sets ULyraPawnExtensionComponent.PawnData on
	// the AI-possessed pawn before Super::OnPossess fires, so the chain
	//   PossessedBy -> HandleControllerChanged -> CheckDefaultInitialization
	//   -> OnPawnReadyToInitialize.Broadcast
	// completes and ABP_Mannequin_Base's locomotion state machine activates.
	//
	// Without PawnData set, IsPawnReadyToInitialize() returns false, the init
	// walk halts, and the anim BP stays in a pre-init state — symptom is "AI
	// moves but mesh slides with no locomotion animation". Player pawns get
	// PawnData from the Lyra experience system; AI pawns spawned outside an
	// experience never receive it, which is the case we're patching here.
	//
	// SimplePawnData is a stock Lyra asset at /Game/Characters/Heroes/
	// SimplePawnData/ — an empty-but-valid LyraPawnData. Using it keeps the
	// AI path independent of any specific Lyra experience content.
	//
	// All reflection — no LyraGame link dependency, so DeltaCode runtime still
	// compiles in non-Lyra projects. Early-outs cleanly when the
	// LyraPawnExtensionComponent class isn't loaded (non-Lyra project) or
	// when the pawn doesn't have one (non-Lyra-parented enemy).
	void TryInitLyraPawnData(APawn* InPawn)
	{
		if (!InPawn) return;

		UClass* PawnExtClass = FindObject<UClass>(
			nullptr, TEXT("/Script/LyraGame.LyraPawnExtensionComponent"));
		if (!PawnExtClass) return;

		UActorComponent* PawnExt = InPawn->FindComponentByClass(PawnExtClass);
		if (!PawnExt) return;

		FObjectProperty* PawnDataProp = CastField<FObjectProperty>(
			PawnExtClass->FindPropertyByName(TEXT("PawnData")));
		if (!PawnDataProp) return;

		if (PawnDataProp->GetObjectPropertyValue_InContainer(PawnExt))
		{
			return;  // already set (e.g. instance override on a placed actor)
		}

		UObject* PawnData = StaticLoadObject(
			UObject::StaticClass(), nullptr,
			TEXT("/Game/Characters/Heroes/SimplePawnData/SimplePawnData.SimplePawnData"));
		if (!PawnData)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[DC] SimplePawnData not found at /Game/Characters/Heroes/"
				     "SimplePawnData/SimplePawnData — ABP_Mannequin_Base will not "
				     "initialize for %s. Lyra version may have moved the asset."),
				*InPawn->GetName());
			return;
		}

		PawnDataProp->SetObjectPropertyValue_InContainer(PawnExt, PawnData);

		// Trigger OnRep_PawnData so the standard Lyra init listeners fire —
		// same code path replication uses when PawnData propagates to clients.
		// This is what wakes up the init state machine so HandleControllerChanged
		// can complete its walk on the next Super::OnPossess.
		if (UFunction* OnRepFunc = PawnExtClass->FindFunctionByName(
			TEXT("OnRep_PawnData")))
		{
			PawnExt->ProcessEvent(OnRepFunc, nullptr);
		}

		UE_LOG(LogTemp, Log,
			TEXT("[DC] Set LyraPawnExtensionComponent.PawnData → SimplePawnData "
			     "on %s — ABP_Mannequin_Base init unblocked."),
			*InPawn->GetName());
	}
}

ADCEnemyAIController::ADCEnemyAIController()
{
	DCPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(
		TEXT("DCPerceptionComponent"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngle;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	DCPerceptionComponent->ConfigureSense(*SightConfig);
	DCPerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());

	// BT + BB components — BlackboardComponent must be created here so
	// UseBlackboard() in OnPossess has something to initialise.
	BehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(
		TEXT("BehaviorTreeComponent"));
	BlackboardComponent = CreateDefaultSubobject<UBlackboardComponent>(
		TEXT("BlackboardComponent"));
}

void ADCEnemyAIController::OnPossess(APawn* InPawn)
{
	// Lyra-aware init MUST run before Super — Super::OnPossess invokes
	// Pawn->PossessedBy, which on ALyraCharacter calls
	// PawnExtComp->HandleControllerChanged. If PawnData isn't set at that
	// moment, the init state machine halts and ABP_Mannequin_Base never
	// transitions out of its pre-init state. See TryInitLyraPawnData above.
	TryInitLyraPawnData(InPawn);

	Super::OnPossess(InPawn);

	UE_LOG(LogTemp, Display, TEXT("[DC] DCEnemyAIController possessing: %s"),
		InPawn ? *InPawn->GetName() : TEXT("<null>"));

	// [INPUT] From: pawn's UDCFactionComponent — drives AI perception affiliation
	if (InPawn)
	{
		if (UDCFactionComponent* Faction =
			InPawn->FindComponentByClass<UDCFactionComponent>())
		{
			AssignedTeamId = FGenericTeamId(Faction->GetTeamID());
		}
	}

	if (DCPerceptionComponent)
	{
		// Push runtime config from CDO so BP-overridden values take effect
		SightConfig->SightRadius = SightRadius;
		SightConfig->LoseSightRadius = LoseSightRadius;
		SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngle;
		DCPerceptionComponent->ConfigureSense(*SightConfig);

		DCPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
			this, &ADCEnemyAIController::HandlePerceptionUpdated);
	}

	// Start the pawn-authored behaviour tree without casting to a concrete
	// pawn class so ADCEnemyBase and B_DC_LyraEnemyBase (parented off
	// ALyraCharacter) can both drive the same controller.
	//
	// Resolution order:
	//   1. IDCEnemyAIPawn — preferred contract for C++ implementers
	//      (ADCEnemyBase implements this natively, returning its UPROPERTY).
	//   2. UDCEnemyAIData component — fallback for BP-only implementers that
	//      can't expose a BT UPROPERTY natively (B_DC_LyraEnemyBase parented
	//      off ALyraCharacter). Python adds the component via SCS at BP
	//      creation time and sets BehaviorTree on the component template.
	if (!InPawn)
	{
		return;
	}

	UBehaviorTree* Tree = nullptr;
	if (InPawn->Implements<UDCEnemyAIPawn>())
	{
		Tree = IDCEnemyAIPawn::Execute_GetEnemyBehaviorTree(InPawn);
	}
	if (!Tree)
	{
		if (UDCEnemyAIData* Data = InPawn->FindComponentByClass<UDCEnemyAIData>())
		{
			Tree = Data->BehaviorTree;
		}
	}
	if (!Tree)
	{
		return;
	}

	if (!Tree->BlackboardAsset)
	{
		// A BT without a blackboard asset can't run — fail loud but don't crash.
		UE_LOG(LogTemp, Warning,
			TEXT("[DC] %s has BehaviorTree %s with no BlackboardAsset — AI will not run."),
			*GetNameSafe(InPawn), *GetNameSafe(Tree));
		return;
	}

	UBlackboardComponent* BBComp = BlackboardComponent.Get();
	if (!UseBlackboard(Tree->BlackboardAsset, BBComp))
	{
		return;
	}

	// Seed HomeLocation with the spawn point so WanderFromHome has a
	// leash origin before any patrol logic overrides it.
	BlackboardComponent->SetValueAsVector(
		DCAIBlackboardKeys::HomeLocation, InPawn->GetActorLocation());

	BehaviorTreeComponent->StartTree(*Tree);
}

void ADCEnemyAIController::OnUnPossess()
{
	if (BehaviorTreeComponent)
	{
		BehaviorTreeComponent->StopTree(EBTStopMode::Safe);
	}
	Super::OnUnPossess();
}

void ADCEnemyAIController::HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor)
	{
		return;
	}

	AActor* Old = CurrentTarget.Get();

	// Only mirror state into the blackboard when its asset has been set —
	// UseBlackboard() in OnPossess might not have run (e.g. pawn doesn't
	// implement IDCEnemyAIPawn and has no UDCEnemyAIData component), in which
	// case SetValueAs*/ClearValue would key-lookup against a null asset and
	// spam ensures from FBlackboard::FKey::operator==.
	const bool bBBReady =
		BlackboardComponent && BlackboardComponent->GetBlackboardAsset();

	if (Stimulus.WasSuccessfullySensed())
	{
		CurrentTarget = Actor;

		if (bBBReady)
		{
			BlackboardComponent->SetValueAsObject(DCAIBlackboardKeys::TargetActor, Actor);
			BlackboardComponent->SetValueAsVector(
				DCAIBlackboardKeys::LastKnownLocation, Stimulus.StimulusLocation);
		}

		if (Old != Actor)
		{
			OnTargetPerceived.Broadcast(Actor, Old);
		}
	}
	else if (CurrentTarget.Get() == Actor)
	{
		// Lost sight of the current target. Keep LastKnownLocation populated
		// so InvestigateLastKnown has somewhere to go; clear only TargetActor.
		CurrentTarget.Reset();

		if (bBBReady)
		{
			BlackboardComponent->ClearValue(DCAIBlackboardKeys::TargetActor);
			BlackboardComponent->SetValueAsVector(
				DCAIBlackboardKeys::LastKnownLocation, Stimulus.StimulusLocation);
		}

		OnTargetPerceived.Broadcast(nullptr, Old);
	}
}

// ─── IGenericTeamAgentInterface ─────────────────────────────────────────────

void ADCEnemyAIController::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	AssignedTeamId = NewTeamID;
}

ETeamAttitude::Type ADCEnemyAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	// Prefer a direct faction-component comparison: this gives designers full
	// control via the DefaultStances table on UDCFactionDefinition.
	const UDCFactionComponent* OwnFaction = nullptr;
	if (const APawn* MyPawn = GetPawn())
	{
		OwnFaction = MyPawn->FindComponentByClass<UDCFactionComponent>();
	}

	const UDCFactionComponent* OtherFaction =
		Other.FindComponentByClass<UDCFactionComponent>();

	if (OwnFaction && OwnFaction->GetFaction() && OtherFaction && OtherFaction->GetFaction())
	{
		const EDCFactionStance Stance =
			OwnFaction->GetFaction()->GetDefaultStanceToward(OtherFaction->GetFactionTag());
		switch (Stance)
		{
		case EDCFactionStance::Hostile:
		case EDCFactionStance::Unfriendly:
			return ETeamAttitude::Hostile;
		case EDCFactionStance::Allied:
		case EDCFactionStance::Friendly:
			return ETeamAttitude::Friendly;
		case EDCFactionStance::Neutral:
		default:
			return ETeamAttitude::Neutral;
		}
	}

	// Fallback: integer team-id comparison. Different non-neutral teams are
	// treated as hostile, matching AAIController's default affiliation logic.
	const IGenericTeamAgentInterface* OtherTeamAgent =
		Cast<const IGenericTeamAgentInterface>(&Other);
	if (!OtherTeamAgent)
	{
		if (const APawn* OtherPawn = Cast<APawn>(&Other))
		{
			OtherTeamAgent = Cast<const IGenericTeamAgentInterface>(OtherPawn->GetController());
		}
	}
	if (!OtherTeamAgent)
	{
		return ETeamAttitude::Neutral;
	}

	const FGenericTeamId OtherId = OtherTeamAgent->GetGenericTeamId();
	if (AssignedTeamId == FGenericTeamId::NoTeam || OtherId == FGenericTeamId::NoTeam)
	{
		return ETeamAttitude::Neutral;
	}
	return (AssignedTeamId == OtherId) ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
}
