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
#include "Actors/DCPlayerControllerBase.h"
#include "Actors/DCGameInstance.h"
#include "Actors/DCHUDBase.h"
#include "Components/DCInteractionComponent.h"
#include "Components/DCInventoryComponent.h"
#include "Components/DCNarrativeStateComponent.h"
#include "Components/DCProgressionComponent.h"
#include "Components/DCSkillTreeComponent.h"
#include "Input/DCInputConfig.h"
#include "Types/DCGameplayTags.h"
#include "UI/DCPauseMenuWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"

ADCPlayerControllerBase::ADCPlayerControllerBase()
{
	InteractionComponent = CreateDefaultSubobject<UDCInteractionComponent>(
		TEXT("InteractionComponent"));

	InventoryComponent = CreateDefaultSubobject<UDCInventoryComponent>(
		TEXT("InventoryComponent"));

	NarrativeState = CreateDefaultSubobject<UDCNarrativeStateComponent>(
		TEXT("NarrativeState"));

	ProgressionComponent = CreateDefaultSubobject<UDCProgressionComponent>(
		TEXT("ProgressionComponent"));

	SkillTreeComponent = CreateDefaultSubobject<UDCSkillTreeComponent>(
		TEXT("SkillTreeComponent"));
}

UDCInteractionComponent* ADCPlayerControllerBase::GetInteractionComponent() const
{
	return InteractionComponent;
}

UDCInventoryComponent* ADCPlayerControllerBase::GetInventoryComponent() const
{
	return InventoryComponent;
}

void ADCPlayerControllerBase::BeginPlay()
{
	Super::BeginPlay();

	// Apply the default mapping context from the input config, if supplied
	if (InputConfig && InputConfig->DefaultMappingContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(InputConfig->DefaultMappingContext, 0);
		}
	}
}

void ADCPlayerControllerBase::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputConfig)
	{
		return;
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	// [INPUT] From: Enhanced Input IA_DC_Interact — resolved via tag lookup in UDCInputConfig
	if (UInputAction* InteractAction =
		InputConfig->FindActionByTag(FDCGameplayTags::Input_Interact))
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started,
			this, &ADCPlayerControllerBase::OnInteractPressed);
	}

	// [INPUT] From: Enhanced Input IA_DC_Pause — resolved via tag lookup in UDCInputConfig
	if (UInputAction* PauseAction =
		InputConfig->FindActionByTag(FDCGameplayTags::Input_Pause))
	{
		EnhancedInput->BindAction(PauseAction, ETriggerEvent::Started,
			this, &ADCPlayerControllerBase::OnPausePressed);
	}
}

void ADCPlayerControllerBase::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Re-sync progression state into the new pawn's attribute set
	if (ProgressionComponent)
	{
		ProgressionComponent->SetLevelDirect(
			ProgressionComponent->GetCurrentLevel(),
			ProgressionComponent->GetCurrentXP());
	}

	// [INPUT] From: UDCSaveGame (via UDCGameInstance) — rehydrate on first possess.
	// No-op when the save payload was freshly created (WasLoadedFromDisk == false).
	if (UDCGameInstance* GI = Cast<UDCGameInstance>(GetGameInstance()))
	{
		GI->ApplySaveToPlayer(this);
	}
}

void ADCPlayerControllerBase::OnInteractPressed()
{
	// [OUTPUT] To: UDCInteractionComponent::TryInteract() — dispatches to focused target
	if (InteractionComponent)
	{
		InteractionComponent->TryInteract();
	}
}

void ADCPlayerControllerBase::OnPausePressed()
{
	if (!PauseMenuClass)
	{
		return;
	}
	// [OUTPUT] To: ADCHUDBase::PushWidget — the HUD stack handles input mode and
	// the pause widget itself calls UGameplayStatics::SetGamePaused on construct.
	if (ADCHUDBase* HUD = Cast<ADCHUDBase>(GetHUD()))
	{
		HUD->PushWidget(PauseMenuClass);
	}
}
