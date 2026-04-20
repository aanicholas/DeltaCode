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
#include "GameFramework/GameModeBase.h"
#include "DCGameModeBase.generated.h"

/**
 * Base game mode for all DeltaCode-generated games.
 * Sets default PlayerController, Pawn, and HUD classes.
 * Subclassed per mission template in Blueprint: B_DC_GameMode_[Template].
 */
UCLASS(BlueprintType, Blueprintable)
class DELTACODE_API ADCGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADCGameModeBase();
};
