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
#include "UObject/Object.h"
#include "DCAIEditorBridge.generated.h"

/**
 * Editor-only Python bridge for protected UPROPERTY mutations on AIModule assets.
 *
 * UBehaviorTree::BlackboardAsset is a UPROPERTY(EditAnywhere) but the C++ member
 * is declared `protected`, which means UE Python's set_editor_property rejects
 * the write — the Python access check honours the C++ access modifier even
 * though the editor itself edits the field through the BT editor UI. This
 * bridge exposes the property via FObjectProperty reflection (a data-level
 * operation, not a member access), so dc_create_ai_assets.py can wire up the
 * duplicated BT_DC_Enemy's BlackboardAsset programmatically.
 *
 * Exposed to Python as `unreal.DCAIEditorBridge.set_behavior_tree_blackboard(...)`.
 *
 * [INPUT]  From: dc_create_ai_assets.py — BT/BB asset paths
 * [OUTPUT] To:   The BT asset (writes BlackboardAsset, marks dirty, saves)
 */
UCLASS()
class DELTACODEEDITOR_API UDCAIEditorBridge : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Set a BehaviorTree's BlackboardAsset property (protected in UE5.7+ and
	 * therefore not writable via Python's set_editor_property). Both paths
	 * are full asset paths like /Game/.../BT_Foo (no .uasset suffix; the
	 * .ObjectName trailing suffix is optional — LoadObject handles both).
	 *
	 * Returns true on success. OutMessage is a one-line summary suitable for
	 * the panel status bar or Output Log — either describes the change made
	 * or the specific failure reason. Participates in the editor's undo
	 * transaction system, so Ctrl+Z reverts an accidental rewire.
	 *
	 * Python call shape:
	 *   ok, msg = unreal.DCAIEditorBridge.set_behavior_tree_blackboard(
	 *       "/Game/DeltaCode/AI/BT_DC_Enemy",
	 *       "/Game/DeltaCode/AI/BB_DC_Enemy_Default")
	 */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|AI")
	static bool SetBehaviorTreeBlackboard(const FString& BTPath,
	                                      const FString& BBPath,
	                                      FString& OutMessage);

	/**
	 * Synchronously rebuild the editor world's navigation mesh via
	 * FEditorBuildUtils::EditorBuild(BuildAIPaths) — the same code path the
	 * "Build > Build Paths" menu uses. Blocks until tiles are queryable, so
	 * PIE-started AI can pathfind immediately. The previous Python path
	 * (`RebuildNavigation` console command) was asynchronous — PIE could
	 * start before tiles existed, leaving AI pathfinding broken even when
	 * the BB and BT were wired correctly.
	 *
	 * Python call:
	 *   ok, msg = unreal.DCAIEditorBridge.rebuild_navigation_mesh()
	 */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|AI")
	static bool RebuildNavigationMesh(FString& OutMessage);
};
