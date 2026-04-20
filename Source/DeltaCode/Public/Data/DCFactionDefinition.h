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
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DCFactionDefinition.generated.h"

// ─── Stance ──────────────────────────────────────────────────────────────────
// Higher-level relationship between two factions (or between a faction and the
// player). UDCReputationSubsystem resolves numeric reputation into one of these
// bands at query time so gameplay code doesn't have to think in raw integers.

UENUM(BlueprintType)
enum class EDCFactionStance : uint8
{
	Allied     UMETA(DisplayName = "Allied"),
	Friendly   UMETA(DisplayName = "Friendly"),
	Neutral    UMETA(DisplayName = "Neutral"),
	Unfriendly UMETA(DisplayName = "Unfriendly"),
	Hostile    UMETA(DisplayName = "Hostile"),
};

// ─── Default stance entry ────────────────────────────────────────────────────
// One row per "other faction" in a faction's defaults table. Kept as a USTRUCT
// (rather than a TMap<FGameplayTag,EDCFactionStance>) so designers get a nicely
// editable array in the details panel with the target faction tag filtered.

USTRUCT(BlueprintType)
struct DELTACODE_API FDCFactionDefaultStance
{
	GENERATED_BODY()

	// Faction tag of the "other" side of the relationship (e.g. Faction.Raiders)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Faction",
	          meta = (Categories = "Faction"))
	FGameplayTag OtherFaction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Faction")
	EDCFactionStance Stance = EDCFactionStance::Neutral;
};

/**
 * Faction definition data asset.
 * Created as DA_DC_Faction_[Name] in Content/DeltaCode/Data/Factions/.
 *
 * Factions are identified by a FGameplayTag (Faction.*) so narrative tags,
 * dialogue gating, and quest prerequisites can reference them uniformly.
 * TeamID maps onto Unreal's FGenericTeamId for perception affiliation so
 * AISense_Sight can resolve friendly/enemy/neutral without DeltaCode code.
 *
 * [OUTPUT] To: UDCFactionComponent — per-NPC faction identity
 * [OUTPUT] To: UDCReputationSubsystem — seed InitialPlayerReputation on first query
 * [OUTPUT] To: ADCEnemyAIController::GetTeamAttitudeTowards — perception affiliation
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API UDCFactionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// ── Identity ────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AssetRegistrySearchable,
	          Category = "DeltaCode|Faction")
	FName FactionID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Faction")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Faction",
	          meta = (MultiLine = "true"))
	FText Description;

	// Tag used by dialogue/quest/narrative systems to refer to this faction
	// (e.g. Faction.CrimsonGuard). Always under the "Faction" root tag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AssetRegistrySearchable,
	          Category = "DeltaCode|Faction", meta = (Categories = "Faction"))
	FGameplayTag FactionTag;

	// ── Team affiliation (for AI perception) ───────────────────────────────
	// Maps onto FGenericTeamId. 0 is reserved for the player; 255 is neutral.
	// Enemy controllers read this via UDCFactionComponent on possession.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Faction",
	          meta = (ClampMin = "0", ClampMax = "255"))
	uint8 TeamID = 1;

	// ── Default stances ─────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Faction",
	          meta = (TitleProperty = "OtherFaction"))
	TArray<FDCFactionDefaultStance> DefaultStances;

	// Stance used for any faction not listed in DefaultStances
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Faction")
	EDCFactionStance FallbackStance = EDCFactionStance::Neutral;

	// ── Player reputation ───────────────────────────────────────────────────

	// Starting reputation the player has with this faction at new-game time.
	// The reputation subsystem seeds from this the first time it's queried.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Faction|Reputation")
	int32 InitialPlayerReputation = 0;

	// Upper / lower clamp for the player's reputation with this faction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Faction|Reputation")
	int32 MinReputation = -1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Faction|Reputation")
	int32 MaxReputation = 1000;

	// Reputation thresholds (inclusive lower bound) that map numeric reputation
	// onto an EDCFactionStance band for the player. Values outside these bands
	// fall through to FallbackStance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Faction|Reputation")
	int32 AlliedThreshold = 500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Faction|Reputation")
	int32 FriendlyThreshold = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Faction|Reputation")
	int32 UnfriendlyThreshold = -100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeltaCode|Faction|Reputation")
	int32 HostileThreshold = -500;

	// ── Helpers ─────────────────────────────────────────────────────────────

	/** Look up the authored stance toward another faction; returns FallbackStance if unlisted. */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Faction")
	EDCFactionStance GetDefaultStanceToward(const FGameplayTag& OtherFactionTag) const;

	/** Resolve a raw player-reputation value into a stance band. */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Faction")
	EDCFactionStance ResolvePlayerStance(int32 Reputation) const;
};
