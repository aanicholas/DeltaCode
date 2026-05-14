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
#include "AI/DCAIEditorBridge.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "Editor.h"
#include "EditorBuildUtils.h"
#include "Engine/World.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogDCAIEditorBridge, Log, All);

bool UDCAIEditorBridge::SetBehaviorTreeBlackboard(
	const FString& BTPath,
	const FString& BBPath,
	FString& OutMessage)
{
	// Step 1 — load both assets. LoadObject accepts /Game/Foo/Bar and resolves
	// to /Game/Foo/Bar.Bar automatically; if a project hits an edge case where
	// it doesn't, the load fails here with a clear message tied to the path.
	UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
	if (!BT)
	{
		OutMessage = FString::Printf(TEXT("BT load failed: %s"), *BTPath);
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}
	UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *BBPath);
	if (!BB)
	{
		OutMessage = FString::Printf(TEXT("BB load failed: %s"), *BBPath);
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	// Step 2 — find the protected BlackboardAsset UPROPERTY by name. The
	// reflection layer ignores C++ access modifiers — access enforcement
	// happens only at the language level when generated headers expand, so
	// FindPropertyByName + FObjectProperty::SetObjectPropertyValue_InContainer
	// is unaffected by the property being declared `protected`.
	FObjectProperty* Prop = CastField<FObjectProperty>(
		UBehaviorTree::StaticClass()->FindPropertyByName(
			TEXT("BlackboardAsset")));
	if (!Prop)
	{
		OutMessage = TEXT(
			"BehaviorTree.BlackboardAsset property not found via reflection — "
			"engine may have renamed it.");
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	// Step 3 — register the change with the transaction system (so Ctrl+Z
	// works), write through reflection, then fire PostEditChangeProperty so
	// the editor sees the BT as modified and any open BT editor tab refreshes
	// its Details panel.
	BT->Modify();
	Prop->SetObjectPropertyValue_InContainer(BT, BB);
	FPropertyChangedEvent ChangeEvent(Prop, EPropertyChangeType::ValueSet);
	BT->PostEditChangeProperty(ChangeEvent);

	// Step 4 — save. SaveLoadedAsset persists the live in-memory object
	// rather than re-reading from disk, so the property we just set is what
	// gets serialised. Mirrors the fix we already shipped on the Python side
	// (save_loaded_asset vs save_asset) for the same UE asset-save quirk.
	UEditorAssetSubsystem* AssetSub = GEditor
		? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
		: nullptr;
	if (!AssetSub || !AssetSub->SaveLoadedAsset(BT))
	{
		OutMessage = FString::Printf(
			TEXT("Property set but save failed: %s"), *BTPath);
		UE_LOG(LogDCAIEditorBridge, Warning, TEXT("%s"), *OutMessage);
		return false;
	}

	OutMessage = FString::Printf(TEXT("BT %s -> BB %s"), *BTPath, *BBPath);
	UE_LOG(LogDCAIEditorBridge, Log, TEXT("%s"), *OutMessage);
	return true;
}

bool UDCAIEditorBridge::RebuildNavigationMesh(FString& OutMessage)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		OutMessage = TEXT("No editor world available for navmesh rebuild");
		UE_LOG(LogDCAIEditorBridge, Warning, TEXT("%s"), *OutMessage);
		return false;
	}

	// FEditorBuildUtils::EditorBuild routes through the same editor pipeline
	// the "Build > Build Paths" menu uses — blocking, with progress UI, and
	// most importantly synchronous enough that tiles are queryable by the
	// time we return (unlike the `RebuildNavigation` console command, which
	// only queues an async tile rebuild).
	const bool bOk = FEditorBuildUtils::EditorBuild(
		World, FBuildOptions::BuildAIPaths);

	if (bOk)
	{
		OutMessage = TEXT("NavMesh rebuilt synchronously via FEditorBuildUtils::EditorBuild");
		UE_LOG(LogDCAIEditorBridge, Log, TEXT("%s"), *OutMessage);
	}
	else
	{
		OutMessage = TEXT("FEditorBuildUtils::EditorBuild returned false — check Output Log for nav build errors");
		UE_LOG(LogDCAIEditorBridge, Warning, TEXT("%s"), *OutMessage);
	}
	return bOk;
}
