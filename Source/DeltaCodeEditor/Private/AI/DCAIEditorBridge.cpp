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
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTreeGraph.h"
#include "Components/ActorComponent.h"
#include "Editor.h"
#include "EditorBuildUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "SubobjectData.h"
#include "SubobjectDataHandle.h"
#include "SubobjectDataSubsystem.h"
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

	// Step 2 — find the BlackboardAsset UPROPERTY by name. The property lacks
	// EditAnywhere/BlueprintReadWrite specifiers (which is why Python's
	// set_editor_property refuses to write it), but FObjectProperty::
	// SetObjectPropertyValue_InContainer operates at the data layer and
	// ignores those specifiers — same access path the BT editor uses.
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

	// Step 4 — propagate the BB change into the BT's editor graph. The graph
	// caches BB references inside decorator nodes (UBTDecorator_BlackboardBase
	// and subclasses), and on next load those cached references override
	// BlackboardAsset — symptom of the BT_DC_Enemy_Template -> BB_Template
	// stickiness we saw after duplication. UpdateBlackboardChange walks the
	// graph and refreshes each decorator's BB pointer to match the new asset.
	//
	// BTGraph is WITH_EDITORONLY_DATA on the BT; we're in an editor module
	// so it's always present. If the cast fails the graph wasn't created
	// (BT authored programmatically with no editor graph) — that's fine,
	// no decorators to update.
#if WITH_EDITORONLY_DATA
	if (UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(BT->BTGraph))
	{
		Graph->UpdateBlackboardChange();
	}
#endif

	// Step 5 — save. SaveLoadedAsset persists the live in-memory object
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

bool UDCAIEditorBridge::AddBlueprintComponent(
	const FString& BlueprintPath,
	const FString& ComponentClassPath,
	FString& OutMessage)
{
	// Step 1 — load the Blueprint and the component UClass. LoadClass takes
	// the /Script/Module.ClassName form (no _C suffix); the engine handles
	// the native-class to UClass* resolution internally.
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!BP)
	{
		OutMessage = FString::Printf(TEXT("Blueprint load failed: %s"),
		                             *BlueprintPath);
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	UClass* ComponentClass = LoadClass<UActorComponent>(
		nullptr, *ComponentClassPath);
	if (!ComponentClass)
	{
		OutMessage = FString::Printf(
			TEXT("Component class load failed: %s"), *ComponentClassPath);
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	// Step 2 — get the subobject subsystem and gather existing subobjects.
	// USubobjectDataSubsystem::Get() returns the engine subsystem singleton;
	// it can only be null if the SubobjectDataInterface module isn't loaded,
	// which would mean the editor itself is broken.
	USubobjectDataSubsystem* Sub = USubobjectDataSubsystem::Get();
	if (!Sub)
	{
		OutMessage = TEXT("USubobjectDataSubsystem::Get() returned null");
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	TArray<FSubobjectDataHandle> Handles;
	Sub->K2_GatherSubobjectDataForBlueprint(BP, Handles);
	if (Handles.Num() == 0)
	{
		OutMessage = FString::Printf(
			TEXT("No subobject handles for %s — BP has no actor root?"),
			*BlueprintPath);
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	// Step 3 — idempotency walk. Find any existing template of the same class
	// so a repeated Build Mission run doesn't keep stacking duplicates. Direct
	// FSubobjectData::GetObject() works here because we're in C++ — Python's
	// wrapper for this struct returns empty tuples, which is the bug that
	// motivated moving this entire flow into the bridge.
	for (const FSubobjectDataHandle& H : Handles)
	{
		FSubobjectData Data;
		if (!Sub->K2_FindSubobjectDataFromHandle(H, Data))
		{
			continue;
		}
		const UObject* Obj = Data.GetObject();
		if (Obj && Obj->IsA(ComponentClass))
		{
			OutMessage = FString::Printf(
				TEXT("%s already present on %s — skipping (idempotent)."),
				*ComponentClass->GetName(), *BlueprintPath);
			UE_LOG(LogDCAIEditorBridge, Log, TEXT("%s"), *OutMessage);
			return true;
		}
	}

	// Step 4 — add the component. Handle 0 is conventionally the actor root,
	// which is the only valid parent for non-SceneComponent ActorComponents
	// (DCFaction/Health/Equipment/EnemyAIData are all USceneComponent-free).
	FAddNewSubobjectParams Params;
	Params.ParentHandle = Handles[0];
	Params.NewClass = ComponentClass;
	Params.BlueprintContext = BP;

	FText FailReason;
	FSubobjectDataHandle NewHandle = Sub->AddNewSubobject(Params, FailReason);
	if (!NewHandle.IsValid())
	{
		OutMessage = FString::Printf(
			TEXT("AddNewSubobject failed for %s on %s: %s"),
			*ComponentClass->GetName(), *BlueprintPath,
			*FailReason.ToString());
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	// Step 5 — verify by re-walking. The subsystem can return a valid handle
	// on partial-success edge cases (e.g. a duplicate-name resolution that
	// didn't actually attach), so the authoritative check is "is a template
	// of this class actually present now?".
	TArray<FSubobjectDataHandle> FreshHandles;
	Sub->K2_GatherSubobjectDataForBlueprint(BP, FreshHandles);
	bool bVerified = false;
	for (const FSubobjectDataHandle& H : FreshHandles)
	{
		FSubobjectData Data;
		if (!Sub->K2_FindSubobjectDataFromHandle(H, Data))
		{
			continue;
		}
		const UObject* Obj = Data.GetObject();
		if (Obj && Obj->IsA(ComponentClass))
		{
			bVerified = true;
			break;
		}
	}
	if (!bVerified)
	{
		OutMessage = FString::Printf(
			TEXT("AddNewSubobject returned valid handle but %s not found on "
			     "rescan of %s — component missing at runtime."),
			*ComponentClass->GetName(), *BlueprintPath);
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	// Step 6 — compile the BP so its generated class picks up the new
	// component template, then save. Compile-on-add mirrors what the
	// Components panel does interactively, so the result is identical to a
	// user-driven add.
	FKismetEditorUtilities::CompileBlueprint(BP);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	UEditorAssetSubsystem* AssetSub = GEditor
		? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
		: nullptr;
	if (!AssetSub || !AssetSub->SaveLoadedAsset(BP))
	{
		OutMessage = FString::Printf(
			TEXT("%s added to %s but save failed."),
			*ComponentClass->GetName(), *BlueprintPath);
		UE_LOG(LogDCAIEditorBridge, Warning, TEXT("%s"), *OutMessage);
		return false;
	}

	OutMessage = FString::Printf(TEXT("Added %s to %s"),
	                             *ComponentClass->GetName(), *BlueprintPath);
	UE_LOG(LogDCAIEditorBridge, Log, TEXT("%s"), *OutMessage);
	return true;
}

bool UDCAIEditorBridge::SetBlueprintComponentObjectProperty(
	const FString& BlueprintPath,
	const FString& ComponentClassPath,
	const FString& PropertyName,
	const FString& ObjectPath,
	FString& OutMessage)
{
	// Step 1 — load the BP, component class, and the value object. The value
	// is intentionally typed UObject* so this can wire any object-typed
	// UPROPERTY (BehaviorTree, BlackboardData, mesh, etc.); per-property type
	// checks happen at the reflection layer in step 4.
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!BP)
	{
		OutMessage = FString::Printf(TEXT("Blueprint load failed: %s"),
		                             *BlueprintPath);
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}
	UClass* CompClass = LoadClass<UActorComponent>(nullptr, *ComponentClassPath);
	if (!CompClass)
	{
		OutMessage = FString::Printf(
			TEXT("Component class load failed: %s"), *ComponentClassPath);
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}
	UObject* Value = LoadObject<UObject>(nullptr, *ObjectPath);
	if (!Value)
	{
		OutMessage = FString::Printf(TEXT("Object load failed: %s"),
		                             *ObjectPath);
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	// Step 2 — find the component template via the BP's SimpleConstructionScript.
	// This is the storage layer SubobjectDataSubsystem reads from; reaching it
	// directly bypasses the FSubobjectData wrapper that Python can't unpack
	// in UE5.7. USCS_Node::ComponentTemplate is a direct UActorComponent*
	// reference — no struct decoding needed.
	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
	if (!SCS)
	{
		OutMessage = FString::Printf(
			TEXT("%s has no SimpleConstructionScript — BP malformed?"),
			*BlueprintPath);
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	UActorComponent* Template = nullptr;
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node && Node->ComponentTemplate
			&& Node->ComponentTemplate->IsA(CompClass))
		{
			Template = Node->ComponentTemplate;
			break;
		}
	}
	if (!Template)
	{
		OutMessage = FString::Printf(
			TEXT("No %s component template found on %s — "
			     "AddBlueprintComponent should run first."),
			*CompClass->GetName(), *BlueprintPath);
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	// Step 3 — resolve the property on the component class. FindPropertyByName
	// walks the class chain so a UPROPERTY declared on a base class is found
	// too. We require the property to be an FObjectProperty so the value-type
	// check is enforced at the reflection layer (writing a UBlueprint* into a
	// TObjectPtr<UBehaviorTree> would fail here, not at runtime).
	FObjectProperty* Prop = CastField<FObjectProperty>(
		CompClass->FindPropertyByName(*PropertyName));
	if (!Prop)
	{
		OutMessage = FString::Printf(
			TEXT("Property %s not found on %s as an object property — "
			     "renamed or wrong type?"),
			*PropertyName, *CompClass->GetName());
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	// Step 4 — write. Both the template and the BP need Modify() so the
	// transaction system sees both as changed; PostEditChangeProperty on the
	// template fires any per-property hooks (none on our DC components today,
	// but cheap insurance for the future).
	Template->Modify();
	BP->Modify();
	Prop->SetObjectPropertyValue_InContainer(Template, Value);
	FPropertyChangedEvent ChangeEvent(Prop, EPropertyChangeType::ValueSet);
	Template->PostEditChangeProperty(ChangeEvent);

	// Step 5 — compile + save. The compile is what propagates the template
	// property change into the generated class's CDO, so runtime FindComponent
	// queries see the new value.
	FKismetEditorUtilities::CompileBlueprint(BP);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	UEditorAssetSubsystem* AssetSub = GEditor
		? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
		: nullptr;
	if (!AssetSub || !AssetSub->SaveLoadedAsset(BP))
	{
		OutMessage = FString::Printf(
			TEXT("%s.%s set on %s but save failed."),
			*CompClass->GetName(), *PropertyName, *BlueprintPath);
		UE_LOG(LogDCAIEditorBridge, Warning, TEXT("%s"), *OutMessage);
		return false;
	}

	OutMessage = FString::Printf(TEXT("%s.%s -> %s on %s"),
	                             *CompClass->GetName(), *PropertyName,
	                             *ObjectPath, *BlueprintPath);
	UE_LOG(LogDCAIEditorBridge, Log, TEXT("%s"), *OutMessage);
	return true;
}

bool UDCAIEditorBridge::AddBlackboardKey(
	const FString& BBPath,
	const FString& KeyName,
	const FString& KeyTypeClassPath,
	const FString& BaseClassPath,
	FString& OutMessage)
{
	// Step 1 — load BB and resolve the key type class. KeyType must be a
	// UBlackboardKeyType subclass; LoadClass with that template enforces it,
	// so a /Script/Engine.Actor path (for example) fails here instead of
	// at runtime.
	UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *BBPath);
	if (!BB)
	{
		OutMessage = FString::Printf(TEXT("BB load failed: %s"), *BBPath);
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	UClass* KeyTypeClass = LoadClass<UBlackboardKeyType>(
		nullptr, *KeyTypeClassPath);
	if (!KeyTypeClass)
	{
		OutMessage = FString::Printf(
			TEXT("KeyType load failed (not a UBlackboardKeyType subclass?): %s"),
			*KeyTypeClassPath);
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	// Step 2 — idempotency. Same-name entry already present means this call
	// is a no-op; we don't try to validate that the existing entry's KeyType
	// matches, because callers re-run Build Mission expecting "do nothing if
	// it's already there."
	const FName KeyFName(*KeyName);
	for (const FBlackboardEntry& Existing : BB->Keys)
	{
		if (Existing.EntryName == KeyFName)
		{
			OutMessage = FString::Printf(
				TEXT("Key '%s' already on %s — skipping (idempotent)."),
				*KeyName, *BBPath);
			UE_LOG(LogDCAIEditorBridge, Log, TEXT("%s"), *OutMessage);
			return true;
		}
	}

	// Step 3 — build the entry. KeyType is created with BB as Outer, matching
	// UBlackboardData::UpdatePersistentKey<T>() — the Instanced specifier on
	// FBlackboardEntry::KeyType requires the inner object's Outer chain to
	// reach the asset, otherwise serialisation drops the pointer (this is
	// what the Python set_editor_property('keys', ...) path was hitting).
	FBlackboardEntry Entry;
	Entry.EntryName = KeyFName;
	Entry.KeyType = NewObject<UBlackboardKeyType>(BB, KeyTypeClass);
	if (!Entry.KeyType)
	{
		OutMessage = FString::Printf(
			TEXT("NewObject<UBlackboardKeyType>(%s) returned null"),
			*KeyTypeClass->GetName());
		UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
		return false;
	}

	// Step 3a — optional BaseClass for Object keys. The cast is class-typed,
	// so a BaseClassPath supplied with a non-Object key type is silently
	// ignored — matches the previous Python contract where base_class was
	// only meaningful for Object keys.
	if (!BaseClassPath.IsEmpty())
	{
		if (UBlackboardKeyType_Object* ObjType =
		    Cast<UBlackboardKeyType_Object>(Entry.KeyType))
		{
			UClass* BaseClass = LoadClass<UObject>(nullptr, *BaseClassPath);
			if (!BaseClass)
			{
				OutMessage = FString::Printf(
					TEXT("BaseClass load failed: %s"), *BaseClassPath);
				UE_LOG(LogDCAIEditorBridge, Error, TEXT("%s"), *OutMessage);
				return false;
			}
			ObjType->BaseClass = BaseClass;
		}
	}

	// Step 4 — append + dirty + save. MarkPackageDirty mirrors what
	// UpdatePersistentKey does; without it the editor doesn't know the BB
	// needs saving and SaveLoadedAsset becomes a no-op.
	BB->Modify();
	BB->Keys.Add(Entry);
	BB->MarkPackageDirty();

	// PostEditChangeProperty triggers UpdateKeyIDs / FirstKeyID recomputation
	// so runtime lookups by name resolve immediately, without needing to
	// reload the asset.
	if (FProperty* KeysProp = UBlackboardData::StaticClass()
	    ->FindPropertyByName(TEXT("Keys")))
	{
		FPropertyChangedEvent ChangeEvent(KeysProp, EPropertyChangeType::ArrayAdd);
		BB->PostEditChangeProperty(ChangeEvent);
	}

	UEditorAssetSubsystem* AssetSub = GEditor
		? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
		: nullptr;
	if (!AssetSub || !AssetSub->SaveLoadedAsset(BB))
	{
		OutMessage = FString::Printf(
			TEXT("Key '%s' added to %s but save failed."),
			*KeyName, *BBPath);
		UE_LOG(LogDCAIEditorBridge, Warning, TEXT("%s"), *OutMessage);
		return false;
	}

	OutMessage = FString::Printf(TEXT("Key '%s' (%s) added to %s"),
	                             *KeyName, *KeyTypeClass->GetName(), *BBPath);
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
