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
#include "Components/DCDialogueComponent.h"
#include "Actors/DCPlayerControllerBase.h"
#include "Actors/DCHUDBase.h"
#include "Components/DCInventoryComponent.h"
#include "Components/DCNarrativeStateComponent.h"
#include "Data/DCNPCDefinition.h"
#include "Subsystems/DCQuestSubsystem.h"
#include "UI/DCMenuWidgetBase.h"
#include "Engine/GameInstance.h"

UDCDialogueComponent::UDCDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDCDialogueComponent::SetNPCDefinition(UDCNPCDefinition* InDefinition)
{
	NPCDefinition = InDefinition;
}

// ─── Conversation lifecycle ──────────────────────────────────────────────────

bool UDCDialogueComponent::StartConversation(ADCPlayerControllerBase* Initiator)
{
	if (bActive || !Initiator || !NPCDefinition)
	{
		return false;
	}

	if (NPCDefinition->DialogueNodes.IsEmpty() ||
	    NPCDefinition->RootDialogueNodeID.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DCDialogueComponent: NPC '%s' has no dialogue tree."),
			*NPCDefinition->NPCID.ToString());
		return false;
	}

	CurrentInitiator = Initiator;
	bActive = true;

	// Push the dialogue widget onto the HUD stack
	if (DialogueWidgetClass)
	{
		if (ADCHUDBase* HUD = Cast<ADCHUDBase>(Initiator->GetHUD()))
		{
			ActiveWidget = Cast<UDCMenuWidgetBase>(HUD->PushWidget(DialogueWidgetClass));
		}
	}

	OnConversationStarted.Broadcast(Initiator);
	EnterNode(NPCDefinition->RootDialogueNodeID);
	return true;
}

void UDCDialogueComponent::EndConversation()
{
	if (!bActive)
	{
		return;
	}

	bActive = false;
	CurrentNodeID = NAME_None;

	if (ActiveWidget)
	{
		ActiveWidget->CloseMenu();
		ActiveWidget = nullptr;
	}

	// [OUTPUT] To: UDCQuestSubsystem — TalkToNPC objective progression
	if (NPCDefinition && !NPCDefinition->NPCID.IsNone())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UDCQuestSubsystem* Quests = GI->GetSubsystem<UDCQuestSubsystem>())
				{
					Quests->NotifyNPCTalked(NPCDefinition->NPCID);
				}
			}
		}
	}

	CurrentInitiator.Reset();
	OnConversationEnded.Broadcast();
}

void UDCDialogueComponent::EnterNode(FName NodeID)
{
	if (NodeID.IsNone() || !NPCDefinition)
	{
		EndConversation();
		return;
	}

	FDCDialogueNode Node;
	if (!NPCDefinition->FindDialogueNode(NodeID, Node))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DCDialogueComponent: node '%s' not found in NPC '%s'."),
			*NodeID.ToString(), *NPCDefinition->NPCID.ToString());
		EndConversation();
		return;
	}

	CurrentNodeID = NodeID;

	// Grant the node's execution tags when it is entered
	if (!Node.ExecutionTags.IsEmpty())
	{
		if (ADCPlayerControllerBase* PC = CurrentInitiator.Get())
		{
			if (UDCNarrativeStateComponent* Narrative =
				PC->FindComponentByClass<UDCNarrativeStateComponent>())
			{
				Narrative->AddTags(Node.ExecutionTags);
			}
		}
	}

	OnNodeEntered.Broadcast(Node);

	// If the node has no responses at all, end the conversation after the UI
	// renders the final line. The widget is expected to hold on the last node
	// until the player hits a "Close" button, so we intentionally don't auto-end.
}

// ─── Response selection ──────────────────────────────────────────────────────

void UDCDialogueComponent::ChooseResponse(int32 AvailableIndex)
{
	if (!bActive)
	{
		return;
	}

	const TArray<FDCDialogueResponse> Available = GetAvailableResponses();
	if (!Available.IsValidIndex(AvailableIndex))
	{
		return;
	}

	const FDCDialogueResponse& Chosen = Available[AvailableIndex];

	// Apply granted tags
	if (!Chosen.GrantedTags.IsEmpty())
	{
		if (ADCPlayerControllerBase* PC = CurrentInitiator.Get())
		{
			if (UDCNarrativeStateComponent* Narrative =
				PC->FindComponentByClass<UDCNarrativeStateComponent>())
			{
				Narrative->AddTags(Chosen.GrantedTags);
			}
		}
	}

	// Transition — NAME_None terminates the conversation
	if (Chosen.NextNodeID.IsNone())
	{
		EndConversation();
		return;
	}
	EnterNode(Chosen.NextNodeID);
}

// ─── Query helpers ───────────────────────────────────────────────────────────

bool UDCDialogueComponent::GetCurrentNode(FDCDialogueNode& OutNode) const
{
	if (!NPCDefinition || CurrentNodeID.IsNone())
	{
		return false;
	}
	return NPCDefinition->FindDialogueNode(CurrentNodeID, OutNode);
}

TArray<FDCDialogueResponse> UDCDialogueComponent::GetAvailableResponses() const
{
	TArray<FDCDialogueResponse> Out;

	FDCDialogueNode Node;
	if (!GetCurrentNode(Node))
	{
		return Out;
	}

	for (const FDCDialogueResponse& Response : Node.Responses)
	{
		if (IsResponseAvailable(Response))
		{
			Out.Add(Response);
		}
	}
	return Out;
}

bool UDCDialogueComponent::IsResponseAvailable(const FDCDialogueResponse& Response) const
{
	ADCPlayerControllerBase* PC = CurrentInitiator.Get();
	if (!PC)
	{
		return false;
	}

	if (!Response.RequiredTags.IsEmpty())
	{
		const UDCNarrativeStateComponent* Narrative =
			PC->FindComponentByClass<UDCNarrativeStateComponent>();
		if (!Narrative || !Narrative->HasAllTags(Response.RequiredTags))
		{
			return false;
		}
	}

	if (Response.RequiredItem.IsValid())
	{
		const UDCInventoryComponent* Inventory = PC->GetInventoryComponent();
		if (!Inventory || !Inventory->HasItem(Response.RequiredItem, 1))
		{
			return false;
		}
	}
	return true;
}
