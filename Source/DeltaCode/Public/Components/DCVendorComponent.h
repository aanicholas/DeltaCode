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
#include "DCVendorComponent.generated.h"

class UDCVendorDefinition;
class UDCItemDefinition;
class UDCItemDefinitionRegistry;
class ADCPlayerControllerBase;
class UDCInventoryComponent;
class UDCMenuWidgetBase;

// ─── Result enum ─────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EDCVendorResult : uint8
{
	Success                   UMETA(DisplayName = "Success"),
	Failed_Closed             UMETA(DisplayName = "Shop Closed"),
	Failed_Invalid            UMETA(DisplayName = "Invalid Arguments"),
	Failed_UnknownItem        UMETA(DisplayName = "Unknown Item"),
	Failed_OutOfStock         UMETA(DisplayName = "Out Of Stock"),
	Failed_InsufficientFunds  UMETA(DisplayName = "Not Enough Currency"),
	Failed_BuyerInventoryFull UMETA(DisplayName = "Buyer Inventory Full"),
	Failed_VendorRejects      UMETA(DisplayName = "Vendor Will Not Buy This"),
	Failed_SellerLacksItem    UMETA(DisplayName = "Seller Doesn't Have Item"),
};

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDCOnVendorStockChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDCOnPurchaseCompleted,
	FDCItemHandle, ItemHandle, int32, Quantity, int32, TotalPrice);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDCOnSaleCompleted,
	FDCItemHandle, ItemHandle, int32, Quantity, int32, TotalPayout);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnShopOpened,
	ADCPlayerControllerBase*, Customer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDCOnShopClosed);

/**
 * Vendor role. Materializes a stock list from UDCVendorDefinition and handles
 * buy / sell transactions against a player's inventory. Currency is modelled
 * as an item handle whose ID matches the entry's CurrencyType FName.
 *
 * [INPUT]  From: ADCNPCBase::OnInteracted (+ dialogue "Shop" response) → OpenShop()
 * [INPUT]  From: W_DC_VendorPanel buttons  → TryBuy / TrySell
 * [OUTPUT] To:   UDCInventoryComponent::AddItem / RemoveItem  — both sides
 * [OUTPUT] To:   W_DC_VendorPanel — binds OnStockChanged / OnPurchaseCompleted
 *
 * [DEPENDS ON] UDCVendorDefinition        — stock + pricing rules
 * [DEPENDS ON] UDCItemDefinitionRegistry  — resolves item + currency handles
 * [DEPENDS ON] UDCInventoryComponent      — currency + item transfer
 */
UCLASS(ClassGroup = (DeltaCode), meta = (BlueprintSpawnableComponent))
class DELTACODE_API UDCVendorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCVendorComponent();

	// Widget pushed on the HUD when the shop opens. Inherits UDCMenuWidgetBase.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeltaCode|Vendor")
	TSubclassOf<UDCMenuWidgetBase> VendorWidgetClass;

	/** Set the definition the component drives from. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Vendor")
	void SetVendorDefinition(UDCVendorDefinition* InDefinition);

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Vendor")
	UDCVendorDefinition* GetVendorDefinition() const { return VendorDefinition; }

	// ── Shop lifecycle ──────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Vendor")
	bool OpenShop(ADCPlayerControllerBase* Customer);

	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Vendor")
	void CloseShop();

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Vendor")
	bool IsShopOpen() const { return bShopOpen; }

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Vendor")
	ADCPlayerControllerBase* GetCurrentCustomer() const { return CurrentCustomer.Get(); }

	// ── Stock query ─────────────────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "DeltaCode|Vendor")
	const TArray<FDCEconomyEntry>& GetStock() const { return CurrentStock; }

	/** Buyback quote for an arbitrary item the customer wants to sell. */
	UFUNCTION(BlueprintPure, Category = "DeltaCode|Vendor")
	int32 GetBuybackPrice(const FDCItemHandle& ItemHandle) const;

	// ── Transactions ────────────────────────────────────────────────────────

	/** Customer buys `Quantity` of the stock entry at `StockIndex`. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Vendor")
	EDCVendorResult TryBuy(int32 StockIndex, int32 Quantity);

	/** Customer sells `Quantity` of `ItemHandle` from their inventory. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Vendor")
	EDCVendorResult TrySell(const FDCItemHandle& ItemHandle, int32 Quantity);

	/** Force-restock entries whose RestockIntervalSeconds has elapsed. */
	UFUNCTION(BlueprintCallable, Category = "DeltaCode|Vendor")
	void RestockNow();

	// ── Delegates ───────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Vendor")
	FDCOnShopOpened OnShopOpened;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Vendor")
	FDCOnShopClosed OnShopClosed;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Vendor")
	FDCOnVendorStockChanged OnStockChanged;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Vendor")
	FDCOnPurchaseCompleted OnPurchaseCompleted;

	UPROPERTY(BlueprintAssignable, Category = "DeltaCode|Vendor")
	FDCOnSaleCompleted OnSaleCompleted;

protected:
	virtual void BeginPlay() override;

private:
	// Materializes CurrentStock from the definition's DefaultStock
	void RebuildStockFromDefinition();

	// Resolve registry for definition lookups
	UDCItemDefinitionRegistry* GetRegistry() const;

	// Returns true if the vendor is configured to buy items of this definition's type
	bool VendorAcceptsSaleOf(const UDCItemDefinition& ItemDef) const;

	UPROPERTY(Transient)
	TObjectPtr<UDCVendorDefinition> VendorDefinition;

	// Live stock — mirrors definition->DefaultStock with runtime-mutated counts
	UPROPERTY(Transient)
	TArray<FDCEconomyEntry> CurrentStock;

	// Per-entry last-restock timestamp (parallel to CurrentStock)
	TArray<double> LastRestockTimes;

	UPROPERTY(Transient)
	TObjectPtr<UDCMenuWidgetBase> ActiveWidget;

	TWeakObjectPtr<ADCPlayerControllerBase> CurrentCustomer;

	bool bShopOpen = false;
};
