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
#include "Modules/ModuleManager.h"

class FSpawnTabArgs;
class FUICommandList;
class SDockTab;

/**
 * DeltaCode editor module — owns the level-editor integration surface:
 * commands, toolbar button, Window-menu entry, and the generator nomad tab.
 */
class FDeltaCodeEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Map FDeltaCodeCommands entries to action handlers on PluginCommands. */
	void RegisterCommandMappings();

	/** Register the nomad tab that hosts SDeltaCodeGeneratorPanel. */
	void RegisterTabSpawners();

	/** Register the toolbar button + Window-menu entry via UToolMenus. */
	void RegisterMenus();

	/** Action invoked by FDeltaCodeCommands::OpenGeneratorPanel. */
	void OnOpenGeneratorPanel();

	/** Nomad tab spawn delegate. */
	TSharedRef<SDockTab> OnSpawnGeneratorTab(const FSpawnTabArgs& SpawnArgs);

	TSharedPtr<FUICommandList> PluginCommands;
};
