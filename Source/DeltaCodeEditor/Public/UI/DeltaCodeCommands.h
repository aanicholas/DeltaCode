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
#include "Framework/Commands/Commands.h"
#include "UI/DeltaCodeStyle.h"

/**
 * UI commands for the DeltaCode editor plugin.
 *
 * Registered at module startup, unregistered at shutdown. The module maps
 * these to actions on its FUICommandList, which is then attached to toolbar
 * and menu entries via UToolMenus.
 *
 * [INPUT]  From: FDeltaCodeEditorModule::StartupModule
 * [OUTPUT] To:   Level editor toolbar, Window menu, keyboard shortcuts
 */
class DELTACODEEDITOR_API FDeltaCodeCommands : public TCommands<FDeltaCodeCommands>
{
public:
	FDeltaCodeCommands()
		: TCommands<FDeltaCodeCommands>(
			TEXT("DeltaCode"),
			NSLOCTEXT("Contexts", "DeltaCode", "DeltaCode"),
			NAME_None,
			FDeltaCodeStyle::GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

	/** Opens (or focuses) the DeltaCode generator nomad tab. */
	TSharedPtr<FUICommandInfo> OpenGeneratorPanel;
};
