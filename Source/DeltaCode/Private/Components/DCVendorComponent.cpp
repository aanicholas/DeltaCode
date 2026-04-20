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
#include "Components/DCVendorComponent.h"
#include "Actors/DCPlayerControllerBase.h"
#include "Actors/DCHUDBase.h"
#include "Components/DCInventoryComponent.h"
#include "Data/DCVendorDefinition.h"
#include "Data/DCItemDefinition.h"
#include "Subsystems/DCItemDefinitionRegistry.h"
#include "UI/DCMenuWidgetBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UDCVendorComponent::UDCVendorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDCVendorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (VendorDefinition)
	{
		RebuildStockFromDefinition();
	}
}

UDCItemDefinitionRegistry* UDCVendorComponent::GetRegistry() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UDCItemDefinitionRegistry>();
		}
	}
	return nullptr;
}

void UDCVendorComponent::SetVendorDefinition(UDCVendorDefinition* InDefinition)
{
	VendorDefinition = InDefinition;
	if (HasBegunPlay() && VendorDefinition)
	{
		RebuildStockFromDefinition();
	}
}

void UDCVendorComponent::RebuildStockFromDefinition()
{
	CurrentStock.Reset();
	LastRestockTimes.Reset();

	if (!VendorDefinition)
	{
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	CurrentStock = VendorDefinition->DefaultStock;
	LastRestockTimes.Init(Now, CurrentStock.Num());

	OnStockChanged.Broadcast();
}

// ─── Shop lifecycle ──────────────────────────────────────────────────────────

bool UDCVendorComponent::OpenShop(ADCPlayerControllerBase* Customer)
{
	if (bShopOpen || !Customer || !VendorDefinition)
	{
		return false;
	}

	RestockNow();

	CurrentCustomer = Customer;
	bShopOpen = true;

	if (VendorWidgetClass)
	{
		if (ADCHUDBase* HUD = Cast<ADCHUDBase>(Customer->GetHUD()))
		{
			ActiveWidget = Cast<UDCMenuWidgetBase>(HUD->PushWidget(VendorWidgetClass));
		}
	}

	OnShopOpened.Broadcast(Customer);
	return true;
}

void UDCVendorComponent::CloseShop()
{
	if (!bShopOpen)
	{
		return;
	}

	bShopOpen = false;

	if (ActiveWidget)
	{
		ActiveWidget->CloseMenu();
		ActiveWidget = nullptr;
	}

	CurrentCustomer.Reset();
	OnShopClosed.Broadcast();
}

// ─── Restock ─────────────────────────────────────────────────────────────────

void UDCVendorComponent::RestockNow()
{
	if (!VendorDefinition || CurrentStock.IsEmpty())
	{
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	bool bChanged = false;

	for (int32 Index = 0; Index < CurrentStock.Num(); ++Index)
	{
		const FDCEconomyEntry& DefaultEntry = VendorDefinition->DefaultStock[Index];
		FDCEconomyEntry& LiveEntry = CurrentStock[Index];

		if (DefaultEntry.RestockIntervalSeconds <= 0.0f)
		{
			continue; // never restocks
		}

		const double Since = Now - LastRestockTimes[Index];
		if (Since >= DefaultEntry.RestockIntervalSeconds)
		{
			// Reset runtime count to the original default
			if (LiveEntry.StockQuantity != DefaultEntry.StockQuantity)
			{
				LiveEntry.StockQuantity = DefaultEntry.StockQuantity;
				bChanged = true;
			}
			LastRestockTimes[Index] = Now;
		}
	}

	if (bChanged)
	{
		OnStockChanged.Broadcast();
	}
}

// ─── Buying ──────────────────────────────────────────────────────────────────

EDCVendorResult UDCVendorComponent::TryBuy(int32 StockIndex, int32 Quantity)
{
	if (!bShopOpen)
	{
		return EDCVendorResult::Failed_Closed;
	}
	if (Quantity <= 0 || !CurrentStock.IsValidIndex(StockIndex))
	{
		return EDCVendorResult::Failed_Invalid;
	}

	ADCPlayerControllerBase* Buyer = CurrentCustomer.Get();
	if (!Buyer)
	{
		return EDCVendorResult::Failed_Closed;
	}
	UDCInventoryComponent* Inv = Buyer->GetInventoryComponent();
	if (!Inv)
	{
		return EDCVendorResult::Failed_Invalid;
	}

	FDCEconomyEntry& Entry = CurrentStock[StockIndex];
	if (!Entry.ItemHandle.IsValid())
	{
		return EDCVendorResult::Failed_UnknownItem;
	}

	// Stock availability
	if (Entry.StockQuantity != -1 && Entry.StockQuantity < Quantity)
	{
		return EDCVendorResult::Failed_OutOfStock;
	}

	const int32 TotalPrice = Entry.Price * Quantity;
	const FDCItemHandle CurrencyHandle(Entry.CurrencyType);

	// Currency check
	if (TotalPrice > 0 && !Inv->HasItem(CurrencyHandle, TotalPrice))
	{
		return EDCVendorResult::Failed_InsufficientFunds;
	}

	// Try to add the goods — this may partially fail due to capacity / uniqueness
	int32 AmountAdded = 0;
	const EDCInventoryResult AddResult = Inv->AddItem(Entry.ItemHandle, Quantity, AmountAdded);
	if (AmountAdded <= 0)
	{
		return EDCVendorResult::Failed_BuyerInventoryFull;
	}

	// Charge exactly for what was delivered (handles partial accept)
	const int32 ChargedPrice = Entry.Price * AmountAdded;
	if (ChargedPrice > 0)
	{
		Inv->RemoveItem(CurrencyHandle, ChargedPrice);
	}

	// Decrement stock
	if (Entry.StockQuantity != -1)
	{
		Entry.StockQuantity = FMath::Max(0, Entry.StockQuantity - AmountAdded);
	}

	OnPurchaseCompleted.Broadcast(Entry.ItemHandle, AmountAdded, ChargedPrice);
	OnStockChanged.Broadcast();

	return (AddResult == EDCInventoryResult::Success)
		? EDCVendorResult::Success
		: EDCVendorResult::Success; // partial accept is still a completed transaction
}

// ─── Selling ─────────────────────────────────────────────────────────────────

bool UDCVendorComponent::VendorAcceptsSaleOf(const UDCItemDefinition& ItemDef) const
{
	if (!VendorDefinition || !VendorDefinition->bAcceptsSales)
	{
		return false;
	}
	if (VendorDefinition->RestrictedItemTypes.Contains(ItemDef.ItemType))
	{
		return false;
	}
	return true;
}

int32 UDCVendorComponent::GetBuybackPrice(const FDCItemHandle& ItemHandle) const
{
	if (!VendorDefinition || !ItemHandle.IsValid())
	{
		return 0;
	}
	const UDCItemDefinitionRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		return 0;
	}
	const UDCItemDefinition* ItemDef = Registry->FindItemByHandle(ItemHandle);
	if (!ItemDef)
	{
		return 0;
	}
	const float Multiplier = FMath::Max(0.0f, VendorDefinition->BuybackMultiplier);
	return FMath::FloorToInt(ItemDef->BaseValue * Multiplier);
}

EDCVendorResult UDCVendorComponent::TrySell(const FDCItemHandle& ItemHandle, int32 Quantity)
{
	if (!bShopOpen)
	{
		return EDCVendorResult::Failed_Closed;
	}
	if (Quantity <= 0 || !ItemHandle.IsValid() || !VendorDefinition)
	{
		return EDCVendorResult::Failed_Invalid;
	}

	ADCPlayerControllerBase* Seller = CurrentCustomer.Get();
	if (!Seller)
	{
		return EDCVendorResult::Failed_Closed;
	}
	UDCInventoryComponent* Inv = Seller->GetInventoryComponent();
	if (!Inv)
	{
		return EDCVendorResult::Failed_Invalid;
	}

	UDCItemDefinitionRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		return EDCVendorResult::Failed_Invalid;
	}
	UDCItemDefinition* ItemDef = Registry->FindItemByHandle(ItemHandle);
	if (!ItemDef)
	{
		return EDCVendorResult::Failed_UnknownItem;
	}

	if (!VendorAcceptsSaleOf(*ItemDef))
	{
		return EDCVendorResult::Failed_VendorRejects;
	}

	if (!Inv->HasItem(ItemHandle, Quantity))
	{
		return EDCVendorResult::Failed_SellerLacksItem;
	}

	const int32 UnitPrice = GetBuybackPrice(ItemHandle);
	const int32 TotalPayout = UnitPrice * Quantity;

	// Remove the item first — if that fails we don't pay out
	const EDCInventoryResult RemoveResult = Inv->RemoveItem(ItemHandle, Quantity);
	if (RemoveResult != EDCInventoryResult::Success)
	{
		return EDCVendorResult::Failed_SellerLacksItem;
	}

	// Credit the seller in the vendor's currency
	if (TotalPayout > 0)
	{
		int32 AmountAdded = 0;
		Inv->AddItem(FDCItemHandle(VendorDefinition->CurrencyType),
		             TotalPayout, AmountAdded);
	}

	OnSaleCompleted.Broadcast(ItemHandle, Quantity, TotalPayout);
	return EDCVendorResult::Success;
}
