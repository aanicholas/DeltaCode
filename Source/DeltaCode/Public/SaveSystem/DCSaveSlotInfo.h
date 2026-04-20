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
#include "DCSaveSlotInfo.generated.h"

/**
 * Lightweight save-slot metadata for the slot-select UI. Projected from a
 * full UDCSaveGame by UDCGameInstance — the widget never holds the full save.
 *
 * [INPUT]  From: UDCGameInstance::EnumerateSlots / GetSlotInfo
 * [OUTPUT] To:   W_DC_SaveSlotPanel — list item fields + thumbnail fallback
 */
USTRUCT(BlueprintType)
struct DELTACODE_API FDCSaveSlotInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Save")
	int32 SlotIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Save")
	FString SlotName;

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Save")
	bool bExists = false;

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Save")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Save")
	int32 PlayerLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Save")
	float TotalPlayTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Save")
	FDateTime LastSaveTimestamp;

	UPROPERTY(BlueprintReadOnly, Category = "DeltaCode|Save")
	FString LevelName;
};
