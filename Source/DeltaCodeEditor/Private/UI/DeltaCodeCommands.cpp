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
#include "UI/DeltaCodeCommands.h"

#define LOCTEXT_NAMESPACE "FDeltaCodeCommands"

void FDeltaCodeCommands::RegisterCommands()
{
	UI_COMMAND(OpenGeneratorPanel,
		"DeltaCode",
		"Open the DeltaCode generator panel",
		EUserInterfaceActionType::Button,
		FInputChord());
}

#undef LOCTEXT_NAMESPACE
