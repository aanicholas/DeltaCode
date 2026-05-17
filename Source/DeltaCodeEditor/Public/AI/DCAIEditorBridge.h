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
 * Editor-only Python bridge for AIModule asset mutations that Python's
 * reflection layer can't reach.
 *
 * Two unrelated UE5.7 quirks force this bridge to exist:
 *
 *  1. UBehaviorTree::BlackboardAsset is a UPROPERTY() (no EditAnywhere /
 *     BlueprintReadWrite specifier), so Python's set_editor_property refuses
 *     to write it. We bypass that via FObjectProperty reflection, which is a
 *     data-level write unaffected by Edit/Blueprint specifiers.
 *
 *  2. FSubobjectData (the struct returned by USubobjectDataSubsystem when
 *     gathering components on a Blueprint) is wrapped by Python in a way that
 *     leaves the underlying UObject inaccessible: to_tuple() returns empty,
 *     and the library accessors aren't reflected in 5.7 — empirically
 *     verified. So component-add from Python can't verify what it added.
 *     C++ has direct access to FSubobjectData::GetObject() so we move the
 *     entire add + verify cycle here.
 *
 * Exposed to Python as `unreal.DCAIEditorBridge.<method_name>(...)`.
 *
 * [INPUT]  From: dc_create_ai_assets.py, dc_danger_zone.py
 * [OUTPUT] To:   BT, BB, and Blueprint assets (writes properties, saves)
 */
UCLASS()
class DELTACODEEDITOR_API UDCAIEditorBridge : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Set a BehaviorTree's BlackboardAsset property (lacks EditAnywhere in
	 * UE5.7+ and therefore not writable via Python's set_editor_property).
	 * Both paths are full asset paths like /Game/.../BT_Foo (no .uasset
	 * suffix; the .ObjectName trailing suffix is optional — LoadObject
	 * handles both).
	 *
	 * After writing the property, propagates the change into the BT's
	 * editor graph via UBehaviorTreeGraph::UpdateBlackboardChange() — without
	 * this step, cached BB references inside graph decorator nodes
	 * (UBTDecorator_BlackboardBase et al.) override the BlackboardAsset
	 * write on next load, which is the root cause of the
	 * BT_DC_Enemy → BB_DC_Enemy_Template regression after Template duplication.
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
	 * Add a component of the given class to a Blueprint asset, parented to
	 * the actor root. Idempotent — returns success without re-adding if a
	 * component of the same class is already present.
	 *
	 * This replaces a Python-side implementation that called
	 * USubobjectDataSubsystem::AddNewSubobject and then walked the resulting
	 * FSubobjectData to verify the add. The verify step is impossible from
	 * Python in UE5.7 because the wrapper exposes neither the BPFunctionLibrary
	 * accessors nor a usable to_tuple() on FSubobjectData. C++ can reach
	 * FSubobjectData::GetObject() directly, so the whole cycle lives here.
	 *
	 * BlueprintPath: /Game/-style path to the BP (no .uasset suffix).
	 * ComponentClassPath: /Script/Module.ClassName path (no /Game/ prefix),
	 *   e.g. "/Script/DeltaCode.DCEnemyAIData".
	 *
	 * Compiles and saves the BP on success. Returns false (with a diagnostic
	 * in OutMessage) on any failure — callers decide whether to abort.
	 *
	 * Python call shape:
	 *   ok, msg = unreal.DCAIEditorBridge.add_blueprint_component(
	 *       "/Game/DeltaCode/AI/B_DC_LyraEnemyBase",
	 *       "/Script/DeltaCode.DCEnemyAIData")
	 */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|AI")
	static bool AddBlueprintComponent(const FString& BlueprintPath,
	                                  const FString& ComponentClassPath,
	                                  FString& OutMessage);

	/**
	 * Set a UObject-typed UPROPERTY on a Blueprint's component template,
	 * resolved by component class. Used to wire DCEnemyAIData.BehaviorTree
	 * inside B_DC_LyraEnemyBase after the component has been added — the
	 * "find the template" step shares the same dead Python reflection path
	 * that AddBlueprintComponent works around, so the lookup runs in C++ via
	 * UBlueprint::SimpleConstructionScript and the property write goes through
	 * FObjectProperty reflection.
	 *
	 * Generic on purpose: PropertyName resolves at runtime so a future
	 * caller can use this for, say, DCFactionComponent.Team without changing
	 * the bridge. Only handles object-typed UPROPERTYs; scalar/string props
	 * would need a different signature.
	 *
	 * Compiles and saves the BP on success. Idempotent — writes the same
	 * value as a no-op (the editor handles the unchanged case).
	 *
	 * Python call shape:
	 *   ok, msg = unreal.DCAIEditorBridge.set_blueprint_component_object_property(
	 *       "/Game/DeltaCode/AI/B_DC_LyraEnemyBase",
	 *       "/Script/DeltaCode.DCEnemyAIData",
	 *       "BehaviorTree",
	 *       "/Game/DeltaCode/AI/BT_DC_Enemy")
	 */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|AI")
	static bool SetBlueprintComponentObjectProperty(
		const FString& BlueprintPath,
		const FString& ComponentClassPath,
		const FString& PropertyName,
		const FString& ObjectPath,
		FString& OutMessage);

	/**
	 * Add a key entry to a UBlackboardData asset, mirroring what
	 * UBlackboardData::UpdatePersistentKey<T>() does in C++. Python's
	 * set_editor_property on the Keys TArray works for the FName but doesn't
	 * persist the Instanced UBlackboardKeyType pointer in this UE5.7 build —
	 * Keys land in the asset with KeyType=null, and BTTasks looking up the
	 * key by name fail at runtime with "Failed to find key '<name>'".
	 *
	 * Idempotent: a key with the same name as an existing entry is a no-op.
	 *
	 * KeyTypeClassPath: /Script/AIModule.BlackboardKeyType_Object (or _Vector,
	 *   _Bool, _Float, _Int, _Name, _String, _Rotator, _Enum, _Class, _Struct).
	 * BaseClassPath: only meaningful for KeyType_Object — restricts the key
	 *   to instances of that class (e.g. "/Script/Engine.Actor"). Empty for
	 *   non-Object key types.
	 *
	 * Python call shape:
	 *   ok, msg = unreal.DCAIEditorBridge.add_blackboard_key(
	 *       "/Game/DeltaCode/AI/BB_DC_Enemy_Default",
	 *       "TargetActor",
	 *       "/Script/AIModule.BlackboardKeyType_Object",
	 *       "/Script/Engine.Actor")
	 */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|AI")
	static bool AddBlackboardKey(const FString& BBPath,
	                             const FString& KeyName,
	                             const FString& KeyTypeClassPath,
	                             const FString& BaseClassPath,
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
