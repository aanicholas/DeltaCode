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
#include "Actors/DCNPCBase.h"
#include "Actors/DCPlayerControllerBase.h"
#include "Components/DCDialogueComponent.h"
#include "Data/DCNPCDefinition.h"

ADCNPCBase::ADCNPCBase()
{
	PrimaryActorTick.bCanEverTick = false;

	DialogueComponent = CreateDefaultSubobject<UDCDialogueComponent>(
		TEXT("DialogueComponent"));
}

void ADCNPCBase::BeginPlay()
{
	Super::BeginPlay();

	if (!NPCDefinitionAsset.IsNull())
	{
		LoadedDefinition = NPCDefinitionAsset.LoadSynchronous();
		if (DialogueComponent && LoadedDefinition)
		{
			DialogueComponent->SetNPCDefinition(LoadedDefinition);
		}
	}
}

// ─── IDCInteractable ─────────────────────────────────────────────────────────

void ADCNPCBase::OnInteracted_Implementation(APawn* Interactor)
{
	if (!Interactor || !DialogueComponent || !LoadedDefinition)
	{
		return;
	}

	ADCPlayerControllerBase* PC =
		Cast<ADCPlayerControllerBase>(Interactor->GetController());
	if (!PC)
	{
		return;
	}

	DialogueComponent->StartConversation(PC);
}

FText ADCNPCBase::GetInteractionPrompt_Implementation() const
{
	const FText Name = LoadedDefinition && !LoadedDefinition->DisplayName.IsEmpty()
		? LoadedDefinition->DisplayName
		: FText::FromString(TEXT("NPC"));

	return FText::Format(
		NSLOCTEXT("DeltaCode", "TalkToPromptFmt", "Talk to {0}"), Name);
}

bool ADCNPCBase::CanInteract_Implementation(APawn* /*Interactor*/) const
{
	return LoadedDefinition != nullptr
	    && !LoadedDefinition->DialogueNodes.IsEmpty()
	    && DialogueComponent != nullptr
	    && !DialogueComponent->IsConversationActive();
}
