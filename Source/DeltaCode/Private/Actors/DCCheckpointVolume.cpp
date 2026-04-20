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
#include "Actors/DCCheckpointVolume.h"
#include "Actors/DCGameInstance.h"
#include "Actors/DCPlayerControllerBase.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

ADCCheckpointVolume::ADCCheckpointVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->SetBoxExtent(FVector(150.0f, 150.0f, 100.0f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
}

void ADCCheckpointVolume::BeginPlay()
{
	Super::BeginPlay();

	if (Trigger)
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(
			this, &ADCCheckpointVolume::HandleOverlapBegin);
	}
}

void ADCCheckpointVolume::HandleOverlapBegin(UPrimitiveComponent* /*OverlappedComp*/,
                                             AActor* OtherActor,
                                             UPrimitiveComponent* /*OtherComp*/,
                                             int32 /*OtherBodyIndex*/,
                                             bool /*bFromSweep*/,
                                             const FHitResult& /*SweepResult*/)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}
	if (ADCPlayerControllerBase* PC = Cast<ADCPlayerControllerBase>(Pawn->GetController()))
	{
		TryTrigger(PC);
	}
}

bool ADCCheckpointVolume::TryTrigger(ADCPlayerControllerBase* Player)
{
	if (!Player)
	{
		return false;
	}

	if (bOneShot && bHasFired)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const float Now = World->GetTimeSeconds();
	if (!bOneShot && CooldownSeconds > 0.0f
	    && (Now - LastTriggerTimeSeconds) < CooldownSeconds)
	{
		return false;
	}

	UDCGameInstance* GI = Cast<UDCGameInstance>(GetGameInstance());
	if (!GI)
	{
		return false;
	}

	GI->SaveGameToDisk();

	bHasFired = true;
	LastTriggerTimeSeconds = Now;

	OnCheckpointTriggered.Broadcast(Player);
	return true;
}
