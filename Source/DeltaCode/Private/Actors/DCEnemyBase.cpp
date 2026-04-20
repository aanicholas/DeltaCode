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
#include "Actors/DCEnemyBase.h"
#include "Actors/DCEnemyAIController.h"
#include "Actors/DCPickupBase.h"
#include "Components/DCHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/DCLootTableDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Subsystems/DCQuestSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

ADCEnemyBase::ADCEnemyBase()
{
	// Possessed by the DeltaCode AI controller on spawn / placement
	AIControllerClass = ADCEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void ADCEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	if (UDCHealthComponent* Health = FindComponentByClass<UDCHealthComponent>())
	{
		Health->OnHealthChanged.AddDynamic(this, &ADCEnemyBase::HandleHealthChanged);
		Health->OnDied.AddDynamic(this, &ADCEnemyBase::HandleDeath);
	}
}

void ADCEnemyBase::HandleHealthChanged(float NewHealth, float OldHealth, float Delta,
                                        AActor* DamageInstigator)
{
	// Only react to damage, not heals
	if (Delta < 0.0f)
	{
		OnHitReact(-Delta, DamageInstigator);
	}
}

void ADCEnemyBase::HandleDeath(AActor* KilledBy, AActor* DamageCauser)
{
	// Stop AI + movement + collision so the corpse can fall cleanly
	if (AController* C = GetController())
	{
		C->UnPossess();
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->DisableMovement();
		Movement->StopMovementImmediately();
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetSimulatePhysics(true);
	}

	SpawnLootDrops();

	// [OUTPUT] To: UDCQuestSubsystem — KillTarget objective progression
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDCQuestSubsystem* Quests = GI->GetSubsystem<UDCQuestSubsystem>())
		{
			Quests->NotifyEnemyKilled(this, KilledBy);
		}
	}

	OnDeathFX(KilledBy);

	if (CorpseLifetime > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(CorpseTimer,
				[WeakThis = TWeakObjectPtr<ADCEnemyBase>(this)]()
				{
					if (ADCEnemyBase* Self = WeakThis.Get())
					{
						Self->Destroy();
					}
				},
				CorpseLifetime, /*bLoop*/ false);
		}
	}
}

void ADCEnemyBase::SpawnLootDrops()
{
	if (!LootPickupClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Collect drops from both lanes into a single list so the scatter/spawn
	// loop doesn't need to know where a given drop came from.
	TArray<FDCInventoryEntry> Drops;

	// ── Inline lane ────────────────────────────────────────────────────────
	for (const FDCLootDrop& Drop : LootTable)
	{
		if (!Drop.ItemHandle.IsValid() || Drop.Quantity <= 0)
		{
			continue;
		}
		if (Drop.Chance < 1.0f && FMath::FRand() > Drop.Chance)
		{
			continue;
		}

		FDCInventoryEntry Entry;
		Entry.ItemHandle = Drop.ItemHandle;
		Entry.StackCount = Drop.Quantity;
		Drops.Add(Entry);
	}

	// ── Shared DA lane ─────────────────────────────────────────────────────
	// LoadSynchronous is acceptable on death: it's a one-shot event and the
	// table is a lightweight data-only asset. Swap to async load if profiling
	// shows a hitch on enemy deaths in crowded fights.
	if (!SharedLootTable.IsNull())
	{
		if (const UDCLootTableDefinition* Table = SharedLootTable.LoadSynchronous())
		{
			Table->RollDrops(Drops);
		}
	}

	if (Drops.IsEmpty())
	{
		return;
	}

	const FVector Origin = GetActorLocation();

	for (const FDCInventoryEntry& Drop : Drops)
	{
		if (!Drop.ItemHandle.IsValid() || Drop.StackCount <= 0)
		{
			continue;
		}

		// Scatter drops in a small XY circle around the corpse
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Radius = FMath::FRandRange(20.0f, 80.0f);
		const FVector SpawnLoc = Origin + FVector(
			FMath::Cos(Angle) * Radius,
			FMath::Sin(Angle) * Radius,
			30.0f);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ADCPickupBase* Pickup = World->SpawnActor<ADCPickupBase>(
			LootPickupClass, SpawnLoc, FRotator::ZeroRotator, Params);
		if (Pickup)
		{
			Pickup->InitializeFromHandle(Drop.ItemHandle, Drop.StackCount);
		}
	}
}
