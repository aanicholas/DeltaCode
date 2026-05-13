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
#include "Python/DCLevelScriptingBridge.h"
#include "DeltaCodeEditorLog.h"

#include "HAL/PlatformFileManager.h"
#include "IPythonScriptPlugin.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "DCLevelScriptingBridge"

namespace DCLevelScriptingBridgePrivate
{
	static const TCHAR* PluginName             = TEXT("DeltaCode");
	static const TCHAR* DangerZoneScriptPath   = TEXT("Content/Python/dc_danger_zone.py");
	static const TCHAR* InspectorScriptPath    = TEXT("Content/Python/dc_inspect_project.py");
	static const TCHAR* SetupLyraScriptPath    = TEXT("Content/Python/dc_setup_lyra.py");

	/** Resolve a path inside the plugin Content/ folder to its absolute form. */
	static FString ResolvePluginScript(const TCHAR* RelativePath)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
		if (!Plugin.IsValid())
		{
			// Fallback — uncommon, but if the plugin descriptor can't be resolved we
			// still want a best-guess path so the error message points somewhere useful.
			return FPaths::ConvertRelativePathToFull(
				FPaths::Combine(FPaths::ProjectPluginsDir(), PluginName, RelativePath));
		}
		return FPaths::ConvertRelativePathToFull(
			FPaths::Combine(Plugin->GetBaseDir(), RelativePath));
	}

	/**
	 * Build the one-shot Python statement that imports dc_danger_zone via importlib
	 * and calls run_danger_zone(<slug>). Using a raw string literal for the path keeps
	 * Windows backslashes intact without us having to escape per-platform.
	 */
	static FString BuildCommand(const FString& AbsoluteScriptPath, const FString& TemplateSlug)
	{
		return FString::Printf(
			TEXT("import importlib.util; "
			     "_dc_spec = importlib.util.spec_from_file_location('dc_danger_zone', r'%s'); "
			     "_dc_mod = importlib.util.module_from_spec(_dc_spec); "
			     "_dc_spec.loader.exec_module(_dc_mod); "
			     "_dc_mod.run_danger_zone('%s')"),
			*AbsoluteScriptPath,
			*TemplateSlug);
	}

	/** Same shape as BuildCommand but invokes dc_inspect_project(<topic>). */
	static FString BuildInspectorCommand(const FString& AbsoluteScriptPath, const FString& TopicSlug)
	{
		return FString::Printf(
			TEXT("import importlib.util; "
			     "_dc_spec = importlib.util.spec_from_file_location('dc_inspect_project', r'%s'); "
			     "_dc_mod = importlib.util.module_from_spec(_dc_spec); "
			     "_dc_spec.loader.exec_module(_dc_mod); "
			     "_dc_mod.dc_inspect_project('%s')"),
			*AbsoluteScriptPath,
			*TopicSlug);
	}

	/** Invokes dc_inspect_project.write_scan_for_llm(<output_path>, <topic>) —
	 *  runs inspector silently for the given topic, formats for LLM, writes
	 *  to disk. Caller reads back. */
	static FString BuildInspectorForLLMCommand(const FString& AbsoluteScriptPath,
	                                           const FString& OutputPath,
	                                           const FString& TopicSlug)
	{
		return FString::Printf(
			TEXT("import importlib.util; "
			     "_dc_spec = importlib.util.spec_from_file_location('dc_inspect_project', r'%s'); "
			     "_dc_mod = importlib.util.module_from_spec(_dc_spec); "
			     "_dc_spec.loader.exec_module(_dc_mod); "
			     "_dc_mod.write_scan_for_llm(r'%s', '%s')"),
			*AbsoluteScriptPath,
			*OutputPath,
			*TopicSlug);
	}
}

bool FDCLevelScriptingBridge::IsPythonAvailable()
{
	IPythonScriptPlugin* Python = IPythonScriptPlugin::Get();
	return Python && Python->IsPythonAvailable();
}

FString FDCLevelScriptingBridge::TemplateSlug(EDCMissionTemplate Template)
{
	switch (Template)
	{
	case EDCMissionTemplate::Extraction:    return TEXT("extraction");
	case EDCMissionTemplate::Arena:         return TEXT("arena");
	case EDCMissionTemplate::QuestHub:      return TEXT("questhub");
	case EDCMissionTemplate::ReactiveStory: return TEXT("reactivestory");
	default:                                return TEXT("extraction");
	}
}

FString FDCLevelScriptingBridge::GetScriptPath()
{
	using namespace DCLevelScriptingBridgePrivate;
	return ResolvePluginScript(DangerZoneScriptPath);
}

FString FDCLevelScriptingBridge::InspectorTopicSlug(EDCInspectorTopic Topic)
{
	switch (Topic)
	{
	case EDCInspectorTopic::All:       return TEXT("all");
	case EDCInspectorTopic::Player:    return TEXT("player");
	case EDCInspectorTopic::Enemy:     return TEXT("enemy");
	case EDCInspectorTopic::Combat:    return TEXT("combat");
	case EDCInspectorTopic::Animation: return TEXT("animation");
	case EDCInspectorTopic::Input:     return TEXT("input");
	case EDCInspectorTopic::Meshes:    return TEXT("meshes");
	default:                           return TEXT("all");
	}
}

FString FDCLevelScriptingBridge::GetScanCachePath()
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectIntermediateDir(),
		                TEXT("DeltaCode"),
		                TEXT("scan_for_llm.txt")));
}

bool FDCLevelScriptingBridge::CreateCoreAssets(FString& OutMessage)
{
	using namespace DCLevelScriptingBridgePrivate;

	if (!IsPythonAvailable())
	{
		OutMessage = TEXT(
			"Python Script Plugin is not available. Enable it under "
			"Edit > Plugins > Scripting > Python Editor Script Plugin, then restart the editor.");
		UE_LOG(LogDeltaCodeEditor, Warning, TEXT("[CreateCoreAssets] %s"), *OutMessage);
		return false;
	}

	const FString ScriptPath = GetScriptPath();

	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ScriptPath))
	{
		OutMessage = FString::Printf(
			TEXT("DeltaCode script not found at %s."), *ScriptPath);
		UE_LOG(LogDeltaCodeEditor, Error, TEXT("[CreateCoreAssets] %s"), *OutMessage);
		return false;
	}

	const FString Command = FString::Printf(
		TEXT("import importlib.util; "
		     "_dc_spec = importlib.util.spec_from_file_location('dc_danger_zone', r'%s'); "
		     "_dc_mod = importlib.util.module_from_spec(_dc_spec); "
		     "_dc_spec.loader.exec_module(_dc_mod); "
		     "_dc_mod.create_core_blueprints()"),
		*ScriptPath);

	UE_LOG(LogDeltaCodeEditor, Log, TEXT("[CreateCoreAssets] creating core Blueprint assets…"));

	const bool bOk = IPythonScriptPlugin::Get()->ExecPythonCommand(*Command);

	if (bOk)
	{
		OutMessage = TEXT("Core Blueprint assets created. Check Output Log for details.");
	}
	else
	{
		OutMessage = TEXT("Core asset creation failed. Check Output Log (LogPython) for the traceback.");
		UE_LOG(LogDeltaCodeEditor, Error, TEXT("[CreateCoreAssets] %s"), *OutMessage);
	}

	return bOk;
}

bool FDCLevelScriptingBridge::ExecuteDangerZoneScript(EDCMissionTemplate Template, FString& OutMessage)
{
	using namespace DCLevelScriptingBridgePrivate;

	if (!IsPythonAvailable())
	{
		OutMessage = TEXT(
			"Python Script Plugin is not available. Enable it under "
			"Edit > Plugins > Scripting > Python Editor Script Plugin, then restart the editor.");
		UE_LOG(LogDeltaCodeEditor, Warning, TEXT("[DangerZone] %s"), *OutMessage);
		return false;
	}

	const FString ScriptPath = GetScriptPath();

	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ScriptPath))
	{
		OutMessage = FString::Printf(
			TEXT("Danger Zone script not found at %s."), *ScriptPath);
		UE_LOG(LogDeltaCodeEditor, Error, TEXT("[DangerZone] %s"), *OutMessage);
		return false;
	}

	const FString Slug = TemplateSlug(Template);
	const FString Command = BuildCommand(ScriptPath, Slug);

	UE_LOG(LogDeltaCodeEditor, Log,
		TEXT("[DangerZone] executing template='%s' script='%s'"), *Slug, *ScriptPath);

	// Embedded Python runs synchronously on the game thread. Returns false when
	// the script raises an unhandled exception — details land in the Output Log.
	const bool bOk = IPythonScriptPlugin::Get()->ExecPythonCommand(*Command);

	if (bOk)
	{
		OutMessage = FString::Printf(
			TEXT("Danger Zone built: template=%s. See Output Log for spawn details."), *Slug);
	}
	else
	{
		OutMessage = FString::Printf(
			TEXT("Danger Zone script raised an error. Check Output Log (LogPython) for the traceback. template=%s"),
			*Slug);
		UE_LOG(LogDeltaCodeEditor, Error, TEXT("[DangerZone] %s"), *OutMessage);
	}

	return bOk;
}

bool FDCLevelScriptingBridge::RunInspectorForLLM(EDCInspectorTopic Topic,
                                                 FString& OutFormattedScan,
                                                 FString& OutMessage)
{
	using namespace DCLevelScriptingBridgePrivate;

	if (!IsPythonAvailable())
	{
		OutMessage = TEXT(
			"Python Script Plugin is not available. Enable it under "
			"Edit > Plugins > Scripting > Python Editor Script Plugin, then restart the editor.");
		UE_LOG(LogDeltaCodeEditor, Warning, TEXT("[InspectForLLM] %s"), *OutMessage);
		return false;
	}

	const FString ScriptPath = ResolvePluginScript(InspectorScriptPath);
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ScriptPath))
	{
		OutMessage = FString::Printf(
			TEXT("Project inspector script not found at %s."), *ScriptPath);
		UE_LOG(LogDeltaCodeEditor, Error, TEXT("[InspectForLLM] %s"), *OutMessage);
		return false;
	}

	const FString OutputPath = GetScanCachePath();

	const FString Slug = InspectorTopicSlug(Topic);
	const FString Command = BuildInspectorForLLMCommand(ScriptPath, OutputPath, Slug);
	UE_LOG(LogDeltaCodeEditor, Log,
		TEXT("[InspectForLLM] writing scan to %s topic=%s"), *OutputPath, *Slug);

	const bool bOk = IPythonScriptPlugin::Get()->ExecPythonCommand(*Command);
	if (!bOk)
	{
		OutMessage = TEXT(
			"Project inspector raised an error. Check Output Log (LogPython) for the traceback.");
		UE_LOG(LogDeltaCodeEditor, Error, TEXT("[InspectForLLM] %s"), *OutMessage);
		return false;
	}

	if (!FFileHelper::LoadFileToString(OutFormattedScan, *OutputPath))
	{
		OutMessage = FString::Printf(
			TEXT("Failed to read inspector output at %s."), *OutputPath);
		UE_LOG(LogDeltaCodeEditor, Error, TEXT("[InspectForLLM] %s"), *OutMessage);
		return false;
	}

	OutMessage = FString::Printf(
		TEXT("Project scan ready (%d chars, topic=%s)."), OutFormattedScan.Len(), *Slug);
	return true;
}

bool FDCLevelScriptingBridge::ExecuteProjectInspector(EDCInspectorTopic Topic, FString& OutMessage)
{
	using namespace DCLevelScriptingBridgePrivate;

	if (!IsPythonAvailable())
	{
		OutMessage = TEXT(
			"Python Script Plugin is not available. Enable it under "
			"Edit > Plugins > Scripting > Python Editor Script Plugin, then restart the editor.");
		UE_LOG(LogDeltaCodeEditor, Warning, TEXT("[Inspect] %s"), *OutMessage);
		return false;
	}

	const FString ScriptPath = ResolvePluginScript(InspectorScriptPath);

	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ScriptPath))
	{
		OutMessage = FString::Printf(
			TEXT("Project inspector script not found at %s."), *ScriptPath);
		UE_LOG(LogDeltaCodeEditor, Error, TEXT("[Inspect] %s"), *OutMessage);
		return false;
	}

	const FString Slug = InspectorTopicSlug(Topic);
	const FString Command = BuildInspectorCommand(ScriptPath, Slug);

	UE_LOG(LogDeltaCodeEditor, Log,
		TEXT("[Inspect] running topic='%s' script='%s'"), *Slug, *ScriptPath);

	const bool bOk = IPythonScriptPlugin::Get()->ExecPythonCommand(*Command);

	if (bOk)
	{
		OutMessage = FString::Printf(
			TEXT("Project inspector complete: topic=%s. See Output Log for the report."), *Slug);
	}
	else
	{
		OutMessage = FString::Printf(
			TEXT("Project inspector raised an error. Check Output Log (LogPython) for the traceback. topic=%s"),
			*Slug);
		UE_LOG(LogDeltaCodeEditor, Error, TEXT("[Inspect] %s"), *OutMessage);
	}

	return bOk;
}

bool FDCLevelScriptingBridge::ExecuteLyraSetup(FString& OutMessage)
{
	using namespace DCLevelScriptingBridgePrivate;

	if (!IsPythonAvailable())
	{
		OutMessage = TEXT(
			"Python Script Plugin is not available. Enable it under "
			"Edit > Plugins > Scripting > Python Editor Script Plugin, then restart the editor.");
		UE_LOG(LogDeltaCodeEditor, Warning, TEXT("[SetupLyra] %s"), *OutMessage);
		return false;
	}

	const FString ScriptPath = ResolvePluginScript(SetupLyraScriptPath);

	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ScriptPath))
	{
		OutMessage = FString::Printf(
			TEXT("DeltaCode setup script not found at %s."), *ScriptPath);
		UE_LOG(LogDeltaCodeEditor, Error, TEXT("[SetupLyra] %s"), *OutMessage);
		return false;
	}

	const FString Command = FString::Printf(
		TEXT("import importlib.util; "
		     "_dc_spec = importlib.util.spec_from_file_location('dc_setup_lyra', r'%s'); "
		     "_dc_mod = importlib.util.module_from_spec(_dc_spec); "
		     "_dc_spec.loader.exec_module(_dc_mod); "
		     "_dc_mod.dc_setup_lyra()"),
		*ScriptPath);

	UE_LOG(LogDeltaCodeEditor, Log, TEXT("[SetupLyra] running dc_setup_lyra…"));

	const bool bOk = IPythonScriptPlugin::Get()->ExecPythonCommand(*Command);

	if (bOk)
	{
		OutMessage = TEXT("Lyra integration setup complete. See Output Log for details.");
	}
	else
	{
		OutMessage = TEXT("Lyra setup raised an error. Check Output Log (LogPython) for the traceback.");
		UE_LOG(LogDeltaCodeEditor, Error, TEXT("[SetupLyra] %s"), *OutMessage);
	}

	return bOk;
}

bool FDCLevelScriptingBridge::IsLyraProjectDetected()
{
	// LyraGame is the canonical Lyra runtime module. If it's loaded the
	// project is built on Lyra and our Lyra-specific assets / GE refs
	// are reachable. Cheap O(1) lookup — safe to call in a Slate
	// visibility callback.
	return FModuleManager::Get().IsModuleLoaded(FName(TEXT("LyraGame")));
}

#undef LOCTEXT_NAMESPACE
