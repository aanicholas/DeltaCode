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
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/DCCoreTypes.h"
#include "DCItemDefinitionRegistry.generated.h"

class UDCItemDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDCOnItemRegistryReady);

/**
 * Discovers and indexes every UDCItemDefinition (and subclass) in the project.
 * Other systems resolve FDCItemHandle → UDCItemDefinition* exclusively through
 * this registry — never by direct asset reference.
 *
 * [OUTPUT] To: UDCInventoryComponent::AddItem()   — definition lookup by handle
 * [OUTPUT] To: UDCWeaponComponent::Initialize()   — cast result to UDCWeaponDefinition
 * [OUTPUT] To: UDCVendorComponent::BuildStock()   — iterates stock entries
 * [OUTPUT] To: UDCLootComponent::RollLoot()       — samples all items of a type
 *
 * Discovery strategy: on Initialize(), scan the AssetRegistry synchronously for
 * every UDCItemDefinition-derived asset, load each, and index by ItemID.
 */
UCLASS()
class DELTACODE_API UDCItemDefinitionRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// UGameInstanceSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Lookup ──────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Items")
	UDCItemDefinition* FindItemByHandle(const FDCItemHandle& Handle) const;

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Items")
	UDCItemDefinition* FindItemByID(FName ItemID) const;

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Items")
	void GetAllItems(TArray<UDCItemDefinition*>& OutItems) const;

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Items")
	void GetAllItemsOfType(EDCItemType ItemType,
	                       TArray<UDCItemDefinition*>& OutItems) const;

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Items")
	bool IsReady() const { return bIsReady; }

	// Fires once discovery completes — systems that depend on the registry
	// should gate on this rather than assuming ordering.
	// [EVENT] Fires: OnRegistryReady — consumed by inventory, vendor, quest init
	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Items")
	FDCOnItemRegistryReady OnRegistryReady;

	// Manual registration — useful for unit tests or runtime-generated items
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Items")
	bool RegisterItem(UDCItemDefinition* ItemDefinition);

private:
	// Discover + load all UDCItemDefinition assets via AssetRegistry
	void DiscoverItems();

	UPROPERTY()
	TMap<FName, TObjectPtr<UDCItemDefinition>> ItemsByID;

	bool bIsReady = false;
};
