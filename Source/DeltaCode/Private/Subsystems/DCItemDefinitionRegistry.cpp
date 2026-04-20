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
#include "Subsystems/DCItemDefinitionRegistry.h"
#include "Data/DCItemDefinition.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

void UDCItemDefinitionRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	DiscoverItems();

	bIsReady = true;
	OnRegistryReady.Broadcast();
}

void UDCItemDefinitionRegistry::Deinitialize()
{
	ItemsByID.Empty();
	bIsReady = false;
	Super::Deinitialize();
}

void UDCItemDefinitionRegistry::DiscoverItems()
{
	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// In the editor the registry may still be scanning; ensure it's ready before query
	TArray<FString> PathsToScan;
	PathsToScan.Add(TEXT("/DeltaCode"));
	PathsToScan.Add(TEXT("/Game"));
	AssetRegistry.ScanPathsSynchronous(PathsToScan, /*bForceRescan*/ false);

	// Collect every asset whose class derives from UDCItemDefinition
	FARFilter Filter;
	Filter.bRecursiveClasses = true;
	Filter.ClassPaths.Add(UDCItemDefinition::StaticClass()->GetClassPathName());

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	for (const FAssetData& AssetData : Assets)
	{
		UDCItemDefinition* Definition = Cast<UDCItemDefinition>(AssetData.GetAsset());
		if (!Definition)
		{
			continue;
		}

		if (Definition->ItemID.IsNone())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("DeltaCode: Item definition '%s' has no ItemID — skipped."),
				*AssetData.GetObjectPathString());
			continue;
		}

		if (ItemsByID.Contains(Definition->ItemID))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("DeltaCode: Duplicate ItemID '%s' in '%s' — original kept."),
				*Definition->ItemID.ToString(),
				*AssetData.GetObjectPathString());
			continue;
		}

		ItemsByID.Add(Definition->ItemID, Definition);
	}

	UE_LOG(LogTemp, Display,
		TEXT("DeltaCode: ItemDefinitionRegistry loaded %d item definitions."),
		ItemsByID.Num());
}

UDCItemDefinition* UDCItemDefinitionRegistry::FindItemByHandle(
	const FDCItemHandle& Handle) const
{
	return FindItemByID(Handle.ItemID);
}

UDCItemDefinition* UDCItemDefinitionRegistry::FindItemByID(FName ItemID) const
{
	if (ItemID.IsNone())
	{
		return nullptr;
	}
	const TObjectPtr<UDCItemDefinition>* Found = ItemsByID.Find(ItemID);
	return Found ? Found->Get() : nullptr;
}

void UDCItemDefinitionRegistry::GetAllItems(
	TArray<UDCItemDefinition*>& OutItems) const
{
	OutItems.Reset(ItemsByID.Num());
	for (const TPair<FName, TObjectPtr<UDCItemDefinition>>& Pair : ItemsByID)
	{
		if (Pair.Value)
		{
			OutItems.Add(Pair.Value);
		}
	}
}

void UDCItemDefinitionRegistry::GetAllItemsOfType(
	EDCItemType ItemType, TArray<UDCItemDefinition*>& OutItems) const
{
	OutItems.Reset();
	for (const TPair<FName, TObjectPtr<UDCItemDefinition>>& Pair : ItemsByID)
	{
		if (Pair.Value && Pair.Value->ItemType == ItemType)
		{
			OutItems.Add(Pair.Value);
		}
	}
}

bool UDCItemDefinitionRegistry::RegisterItem(UDCItemDefinition* ItemDefinition)
{
	if (!ItemDefinition || ItemDefinition->ItemID.IsNone())
	{
		return false;
	}
	if (ItemsByID.Contains(ItemDefinition->ItemID))
	{
		return false;
	}
	ItemsByID.Add(ItemDefinition->ItemID, ItemDefinition);
	return true;
}
