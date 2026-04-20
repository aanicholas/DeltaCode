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
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "DCPlayerControllerBase.generated.h"

class UDCInteractionComponent;
class UDCInventoryComponent;
class UDCNarrativeStateComponent;
class UDCProgressionComponent;
class UDCSkillTreeComponent;
class UDCInputConfig;
class UDCPauseMenuWidget;
class UInputAction;

/**
 * Base player controller for all DeltaCode games.
 * Owns the interaction component and forwards Enhanced Input actions.
 * Inventory and other gameplay components attach here in Layer 2C+.
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API ADCPlayerControllerBase : public APlayerController,
                                              public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	ADCPlayerControllerBase();

	// ── IGenericTeamAgentInterface ──────────────────────────────────────────
	// Player is team 0 by default. Enemies with non-zero team ids see the
	// player as hostile via the faction/affiliation resolution on the enemy
	// AI controller. Can be overridden at runtime (disguise mechanics etc.).
	virtual FGenericTeamId GetGenericTeamId() const override { return PlayerTeamId; }
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override
	{
		PlayerTeamId = NewTeamID;
	}

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Components")
	UDCInteractionComponent* GetInteractionComponent() const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Components")
	UDCInventoryComponent* GetInventoryComponent() const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Components")
	UDCNarrativeStateComponent* GetNarrativeState() const { return NarrativeState; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Components")
	UDCProgressionComponent* GetProgressionComponent() const { return ProgressionComponent; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Components")
	UDCSkillTreeComponent* GetSkillTreeComponent() const { return SkillTreeComponent; }

	// Input configuration — set on the controller Blueprint or in a subclass default
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Input")
	TObjectPtr<UDCInputConfig> InputConfig;

	/** Pause-menu widget class pushed on Input.Pause. Assign to W_DC_PauseMenu in BP. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|UI")
	TSubclassOf<UDCPauseMenuWidget> PauseMenuClass;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;

	// [EVENT] Handler for Input.Interact — forwards to UDCInteractionComponent::TryInteract()
	void OnInteractPressed();

	// [EVENT] Handler for Input.Pause — pushes PauseMenuClass onto the HUD widget stack.
	void OnPausePressed();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDCInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDCInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDCNarrativeStateComponent> NarrativeState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDCProgressionComponent> ProgressionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDCSkillTreeComponent> SkillTreeComponent;

private:
	// Player team id — authored on the controller BP, defaults to 0.
	FGenericTeamId PlayerTeamId = FGenericTeamId(0);
};
