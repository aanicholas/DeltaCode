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
#include "Components/DCHealthComponent.h"
#include "Actors/DCCharacterBase.h"
#include "Types/DCCoreTypes.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "TimerManager.h"

UDCHealthComponent::UDCHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDCHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxHealth = DefaultMaxHealth;
	MaxShield = bShieldEnabled ? DefaultMaxShield : 0.0f;

	// If the owner is a DeltaCode character, honor its FDCStatBlock
	if (const ADCCharacterBase* Character = Cast<ADCCharacterBase>(GetOwner()))
	{
		const FDCStatBlock& Stats = Character->BaseStats;
		if (Stats.MaxHealth > 0.0f) { MaxHealth = Stats.MaxHealth; }
		if (bShieldEnabled && Stats.MaxShield > 0.0f) { MaxShield = Stats.MaxShield; }
	}

	CurrentHealth = MaxHealth;
	CurrentShield = MaxShield;
	bIsDead = false;
}

// ─── Damage / Heal / Kill ────────────────────────────────────────────────────

void UDCHealthComponent::ApplyDamage(float Amount, AActor* DamageSource,
                                     AActor* DamageCauser)
{
	if (bIsDead || Amount <= 0.0f)
	{
		return;
	}

	const float OldHealth = CurrentHealth;
	float Remaining = Amount;

	if (bShieldEnabled && CurrentShield > 0.0f)
	{
		const float ShieldAbsorb = FMath::Min(CurrentShield, Remaining);
		CurrentShield -= ShieldAbsorb;
		Remaining -= ShieldAbsorb;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - Remaining);

	OnHealthChanged.Broadcast(CurrentHealth, OldHealth, CurrentHealth - OldHealth,
	                          DamageCauser ? DamageCauser : DamageSource);

	if (bShieldEnabled)
	{
		StartShieldRegenCooldown();
	}

	if (CurrentHealth <= 0.0f && !bIsDead)
	{
		bIsDead = true;
		StopShieldRegen();
		OnDied.Broadcast(DamageSource, DamageCauser);
	}
}

void UDCHealthComponent::Heal(float Amount, AActor* Instigator)
{
	if (bIsDead || Amount <= 0.0f)
	{
		return;
	}

	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + Amount);
	OnHealthChanged.Broadcast(CurrentHealth, OldHealth, CurrentHealth - OldHealth,
	                          Instigator);
}

void UDCHealthComponent::Kill(AActor* KilledBy, AActor* DamageCauser)
{
	if (bIsDead)
	{
		return;
	}
	ApplyDamage(CurrentHealth + 1.0f, KilledBy, DamageCauser);
}

void UDCHealthComponent::AdjustMaxHealth(float Delta, bool bRefill)
{
	if (FMath::IsNearlyZero(Delta))
	{
		return;
	}

	const float OldMax = MaxHealth;
	MaxHealth = FMath::Max(1.0f, MaxHealth + Delta);

	const float OldHealth = CurrentHealth;
	if (bRefill && Delta > 0.0f)
	{
		CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + Delta);
	}
	else
	{
		CurrentHealth = FMath::Min(MaxHealth, CurrentHealth);
	}

	OnHealthChanged.Broadcast(CurrentHealth, OldHealth, CurrentHealth - OldHealth, GetOwner());
	(void)OldMax;
}

void UDCHealthComponent::Revive()
{
	StopShieldRegen();

	const float OldHealth = CurrentHealth;
	CurrentHealth = MaxHealth;
	CurrentShield = MaxShield;
	bIsDead = false;

	OnHealthChanged.Broadcast(CurrentHealth, OldHealth, CurrentHealth - OldHealth, nullptr);
	OnRevived.Broadcast();
}

// ─── Shield regen ────────────────────────────────────────────────────────────

void UDCHealthComponent::StartShieldRegenCooldown()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Restart delay every time we take fresh damage
	World->GetTimerManager().ClearTimer(ShieldRegenKickoffTimer);
	World->GetTimerManager().ClearTimer(ShieldRegenTickTimer);

	World->GetTimerManager().SetTimer(ShieldRegenKickoffTimer,
		[WeakThis = TWeakObjectPtr<UDCHealthComponent>(this)]()
		{
			if (UDCHealthComponent* Self = WeakThis.Get())
			{
				Self->TickShieldRegen();
			}
		},
		FMath::Max(0.01f, ShieldRegenDelay), /*bLoop*/ false);
}

void UDCHealthComponent::TickShieldRegen()
{
	UWorld* World = GetWorld();
	if (!World || bIsDead || CurrentShield >= MaxShield)
	{
		return;
	}

	// Tick at 10 Hz — regenerates ShieldRegenRate/sec
	constexpr float TickRate = 0.1f;
	CurrentShield = FMath::Min(MaxShield, CurrentShield + ShieldRegenRate * TickRate);
	OnHealthChanged.Broadcast(CurrentHealth, CurrentHealth, 0.0f, nullptr);

	if (CurrentShield < MaxShield)
	{
		World->GetTimerManager().SetTimer(ShieldRegenTickTimer, this,
			&UDCHealthComponent::TickShieldRegen, TickRate, /*bLoop*/ false);
	}
}

void UDCHealthComponent::StopShieldRegen()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShieldRegenKickoffTimer);
		World->GetTimerManager().ClearTimer(ShieldRegenTickTimer);
	}
}
