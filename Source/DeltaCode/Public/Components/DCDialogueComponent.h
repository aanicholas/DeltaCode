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
#include "Components/ActorComponent.h"
#include "Types/DCDataDefinitions.h"
#include "DCDialogueComponent.generated.h"

class UDCNPCDefinition;
class ADCPlayerControllerBase;
class UDCMenuWidgetBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnConversationStarted,
	ADCPlayerControllerBase*, Initiator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnDialogueNodeEntered,
	const FDCDialogueNode&, Node);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDCOnConversationEnded);

/**
 * Runtime dialogue state machine. Lives on ADCNPCBase and is driven by the
 * NPC's UDCNPCDefinition::DialogueNodes tree.
 *
 * The UMG dialogue panel (W_DC_DialoguePanel) subscribes to OnNodeEntered and
 * renders SpeakerText + filtered Responses. Player selections arrive back as
 * ChooseResponse(Index).
 *
 * [INPUT]  From: ADCNPCBase::OnInteracted              → StartConversation()
 * [INPUT]  From: W_DC_DialoguePanel response buttons   → ChooseResponse()
 * [OUTPUT] To:   UDCNarrativeStateComponent::AddTags   — GrantedTags on response
 * [OUTPUT] To:   ADCHUDBase::PushWidget(DialoguePanel) — UI lifecycle
 * [OUTPUT] To:   W_DC_DialoguePanel                    — via OnNodeEntered event
 *
 * [DEPENDS ON] UDCNPCDefinition (Layer 2B) — dialogue tree source
 * [DEPENDS ON] UDCNarrativeStateComponent  — tag gating + tag granting
 * [DEPENDS ON] UDCInventoryComponent       — RequiredItem checks on responses
 */
UCLASS(ClassGroup = (DeltaCode), meta = (BlueprintSpawnableComponent))
class DELTACODE_API UDCDialogueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCDialogueComponent();

	// The dialogue panel widget class pushed on the HUD when a conversation starts
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Dialogue")
	TSubclassOf<UDCMenuWidgetBase> DialogueWidgetClass;

	/** Cache the definition the NPC was spawned with. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Dialogue")
	void SetNPCDefinition(UDCNPCDefinition* InDefinition);

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Dialogue")
	UDCNPCDefinition* GetNPCDefinition() const { return NPCDefinition; }

	/** Begin a conversation. Pushes the dialogue widget and enters the root node. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Dialogue")
	bool StartConversation(ADCPlayerControllerBase* Initiator);

	/** End the current conversation and pop the dialogue widget. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Dialogue")
	void EndConversation();

	/** Pick a response by index within the currently filtered available list. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Dialogue")
	void ChooseResponse(int32 AvailableIndex);

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Dialogue")
	bool IsConversationActive() const { return bActive; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Dialogue")
	FName GetCurrentNodeID() const { return CurrentNodeID; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Dialogue")
	bool GetCurrentNode(FDCDialogueNode& OutNode) const;

	/** Returns the subset of the current node's responses the player qualifies for. */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Dialogue")
	TArray<FDCDialogueResponse> GetAvailableResponses() const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Dialogue")
	ADCPlayerControllerBase* GetInitiator() const { return CurrentInitiator.Get(); }

	// ── Delegates ───────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Dialogue")
	FDCOnConversationStarted OnConversationStarted;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Dialogue")
	FDCOnDialogueNodeEntered OnNodeEntered;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Dialogue")
	FDCOnConversationEnded OnConversationEnded;

private:
	void EnterNode(FName NodeID);

	// Returns true if the initiator passes RequiredTags + RequiredItem checks
	bool IsResponseAvailable(const FDCDialogueResponse& Response) const;

	UPROPERTY(Transient)
	TObjectPtr<UDCNPCDefinition> NPCDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UDCMenuWidgetBase> ActiveWidget;

	TWeakObjectPtr<ADCPlayerControllerBase> CurrentInitiator;

	FName CurrentNodeID = NAME_None;
	bool bActive = false;
};
