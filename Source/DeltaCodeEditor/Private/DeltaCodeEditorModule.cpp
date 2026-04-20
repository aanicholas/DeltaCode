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
#include "DeltaCodeEditorModule.h"
#include "DeltaCodeEditorLog.h"
#include "UI/DeltaCodeCommands.h"
#include "UI/DeltaCodeStyle.h"
#include "UI/SDeltaCodeGeneratorPanel.h"

#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FDeltaCodeEditorModule"

DEFINE_LOG_CATEGORY(LogDeltaCodeEditor);

IMPLEMENT_MODULE(FDeltaCodeEditorModule, DeltaCodeEditor)

namespace DeltaCodeEditorPrivate
{
	static const FName GeneratorTabId(TEXT("DeltaCodeTab"));
}

void FDeltaCodeEditorModule::StartupModule()
{
	FDeltaCodeStyle::Initialize();
	FDeltaCodeCommands::Register();

	PluginCommands = MakeShared<FUICommandList>();
	RegisterCommandMappings();

	RegisterTabSpawners();

	// UToolMenus is only safe to touch after the subsystem is ready; defer if needed.
	if (UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(
				this, &FDeltaCodeEditorModule::RegisterMenus));
	}
}

void FDeltaCodeEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DeltaCodeEditorPrivate::GeneratorTabId);

	if (FDeltaCodeCommands::IsRegistered())
	{
		FDeltaCodeCommands::Unregister();
	}

	FDeltaCodeStyle::Shutdown();

	PluginCommands.Reset();
}

// ─── Commands ────────────────────────────────────────────────────────────────

void FDeltaCodeEditorModule::RegisterCommandMappings()
{
	PluginCommands->MapAction(
		FDeltaCodeCommands::Get().OpenGeneratorPanel,
		FExecuteAction::CreateRaw(this, &FDeltaCodeEditorModule::OnOpenGeneratorPanel),
		FCanExecuteAction());
}

void FDeltaCodeEditorModule::OnOpenGeneratorPanel()
{
	FGlobalTabmanager::Get()->TryInvokeTab(DeltaCodeEditorPrivate::GeneratorTabId);
}

// ─── Tab spawner ─────────────────────────────────────────────────────────────

void FDeltaCodeEditorModule::RegisterTabSpawners()
{
	FGlobalTabmanager::Get()
		->RegisterNomadTabSpawner(
			DeltaCodeEditorPrivate::GeneratorTabId,
			FOnSpawnTab::CreateRaw(this, &FDeltaCodeEditorModule::OnSpawnGeneratorTab))
		.SetDisplayName(LOCTEXT("GeneratorTabTitle", "DeltaCode"))
		.SetTooltipText(LOCTEXT("GeneratorTabTooltip",
			"Open the DeltaCode AI generation panel."))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetIcon(FSlateIcon(FDeltaCodeStyle::GetStyleSetName(), "DeltaCode.OpenGeneratorPanel"));
}

TSharedRef<SDockTab> FDeltaCodeEditorModule::OnSpawnGeneratorTab(const FSpawnTabArgs& /*SpawnArgs*/)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SDeltaCodeGeneratorPanel)
		];
}

// ─── Menus (toolbar + Window menu) ───────────────────────────────────────────

void FDeltaCodeEditorModule::RegisterMenus()
{
	// Owner scope lets UnregisterOwner(this) in ShutdownModule tear everything
	// registered inside this call cleanly.
	FToolMenuOwnerScoped OwnerScoped(this);

	// Toolbar button on the Level Editor play/toolbar row.
	if (UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar"))
	{
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
		FToolMenuEntry Entry = FToolMenuEntry::InitToolBarButton(
			FDeltaCodeCommands::Get().OpenGeneratorPanel);
		Entry.SetCommandList(PluginCommands);
		Entry.Icon = FSlateIcon("DeltaCodeStyle", "DeltaCode.OpenGeneratorPanel");
		Section.AddEntry(Entry);
	}
}

#undef LOCTEXT_NAMESPACE
