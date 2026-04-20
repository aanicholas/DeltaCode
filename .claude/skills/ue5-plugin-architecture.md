# SKILL: ue5-plugin-architecture
# DeltaCode Plugin — Module Structure, UBT, and .uplugin Rules

## Purpose
This skill governs every decision DeltaCode makes about its own internal structure and
any plugin or module scaffolding it generates for the user. It defines the canonical
folder layout, .uplugin format, module types, loading phases, and Build.cs rules for
UE5.5+ projects.

This skill is ALWAYS active. It is the structural foundation every other skill builds on.

---

## DeltaCode's Own Module Structure

DeltaCode uses a two-module architecture:

```
Plugins/DeltaCode/
├── DeltaCode.uplugin
├── Resources/
│   └── Icon128.png
├── Content/                          (CanContainContent = true)
└── Source/
    ├── DeltaCode/                    (Runtime module — generated actors, components)
    │   ├── DeltaCode.Build.cs
    │   ├── Public/
    │   │   ├── DeltaCodeModule.h
    │   │   ├── Actors/
    │   │   ├── Components/
    │   │   └── Interfaces/
    │   └── Private/
    │       └── DeltaCodeModule.cpp
    └── DeltaCodeEditor/              (Editor module — UI, API calls, Python scripting)
        ├── DeltaCodeEditor.Build.cs
        ├── Public/
        │   ├── DeltaCodeEditorModule.h
        │   ├── UI/
        │   ├── Settings/
        │   └── MissionTemplates/
        └── Private/
            └── DeltaCodeEditorModule.cpp
```

### Why Two Modules?
- Runtime module ships with packaged games — contains actor/component classes users place in levels
- Editor module is stripped at cook time — contains Slate UI, HTTP calls to Anthropic, Python bridge
- Mixing Editor-only code (UnrealEd, Slate) into a Runtime module causes packaging failures
- Any header that includes UnrealEd or EditorSubsystem headers MUST live in the Editor module

---

## Canonical .uplugin File

```json
{
  "FileVersion": 3,
  "Version": 1,
  "VersionName": "1.0.0",
  "FriendlyName": "DeltaCode",
  "Description": "AI-powered code generation and level authoring for UE5. Safe Mode and Danger Zone.",
  "Category": "Editor",
  "CreatedBy": "DeltaCode",
  "CreatedByURL": "",
  "DocsURL": "",
  "MarketplaceURL": "",
  "SupportURL": "",
  "CanContainContent": true,
  "IsBetaVersion": false,
  "IsExperimentalVersion": false,
  "Installed": false,
  "Modules": [
    {
      "Name": "DeltaCode",
      "Type": "Runtime",
      "LoadingPhase": "Default"
    },
    {
      "Name": "DeltaCodeEditor",
      "Type": "Editor",
      "LoadingPhase": "PostEngineInit"
    }
  ]
}
```

### LoadingPhase Rules
- `Default` — for Runtime modules with gameplay actors. Safe for most cases.
- `PostEngineInit` — REQUIRED for Editor modules that register Slate tabs, menus, or toolbar buttons.
  Using `Default` for Editor UI registration causes intermittent null pointer crashes at startup.
- `PreDefault` — only for modules that other modules depend on at startup. Not needed for DeltaCode.
- Never use `EarliestPossible` or `PostConfigInit` unless patching engine internals.

---

## Module Type Decision Table

| What the module does | Correct Type |
|---|---|
| Gameplay actors, components, interfaces | Runtime |
| Editor UI (Slate, tabs, toolbar) | Editor |
| Code generation / HTTP calls to Anthropic | Editor |
| Python scripting bridge | Editor |
| Utilities callable from both Editor and game | Runtime |
| Developer-only debug tools (excluded from Shipping) | Developer |

NEVER put Editor-type includes in a Runtime module, even behind #if WITH_EDITOR.
The correct pattern is to put all editor-dependent code in the Editor module and use
interfaces or delegates to communicate back to the Runtime module if needed.

---

## Build.cs Rules

### Runtime Module (DeltaCode.Build.cs)
```csharp
using UnrealBuildTool;

public class DeltaCode : ModuleRules
{
    public DeltaCode(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayAbilities",   // For GAS integration on enemies/pickups
            "GameplayTags",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore",
        });
    }
}
```

### Editor Module (DeltaCodeEditor.Build.cs)
```csharp
using UnrealBuildTool;

public class DeltaCodeEditor : ModuleRules
{
    public DeltaCodeEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeltaCode",          // Always depends on our own Runtime module
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "Slate",
            "SlateCore",
            "EditorStyle",
            "EditorSubsystem",
            "HTTP",               // For Anthropic API calls
            "Json",               // For parsing API responses
            "JsonUtilities",
            "LevelEditor",        // For toolbar/menu extension
            "DeveloperSettings",  // For API key storage via UDeveloperSettings
            "PythonScriptPlugin", // For Danger Zone level scripting
        });
    }
}
```

### Critical Build.cs Rules
- Always use `UseExplicitOrSharedPCHs` — UE5.5+ default; improves compile times significantly
- `PublicDependencyModuleNames` — headers are exposed to modules that depend on you. Use sparingly.
- `PrivateDependencyModuleNames` — headers are NOT exposed. Use for most dependencies.
- `DynamicDependencyModuleNames` — for modules loaded at runtime via plugin system. Not needed here.
- Never add `UnrealEd` to a Runtime module's dependencies — causes cook failures

---

## Module Implementation Pattern

### DeltaCodeModule.h (Runtime)
```cpp
// Copyright DeltaCode. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class DELTACODE_API FDeltaCodeModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
```

### DeltaCodeEditorModule.h (Editor)
```cpp
// Copyright DeltaCode. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FDeltaCodeEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenuExtensions();
    void RegisterTabSpawners();
    TSharedPtr<class FUICommandList> PluginCommands;
};
```

### DeltaCodeEditorModule.cpp pattern
```cpp
// Copyright DeltaCode. All Rights Reserved.
#include "DeltaCodeEditorModule.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "FDeltaCodeEditorModule"

IMPLEMENT_MODULE(FDeltaCodeEditorModule, DeltaCodeEditor)

void FDeltaCodeEditorModule::StartupModule()
{
    // Register after engine fully initialized (PostEngineInit loading phase ensures this)
    RegisterMenuExtensions();
    RegisterTabSpawners();
}

void FDeltaCodeEditorModule::ShutdownModule()
{
    // Always unregister — failing to do so causes crashes on hot reload
    FGlobalTabmanager::Get()->UnregisterTabSpawner(FName("DeltaCodeTab"));
}

#undef LOCTEXT_NAMESPACE
```

---

## Content Folder and Asset Organization (Lyra Conventions)

DeltaCode-generated content lives under:
```
Content/DeltaCode/
├── Core/                 (base C++ class child Blueprints — never delete these)
│   ├── B_DC_PickupBase
│   ├── B_DC_EnemyBase
│   └── B_DC_BossBase
├── Missions/
│   ├── Extraction/
│   ├── Destiny/
│   ├── Fallout/
│   └── OpenWorld/
├── Weapons/
├── UI/
│   └── W_DeltaCodePanel
└── Maps/
    └── L_DeltaCode_Test
```

Asset naming follows Lyra conventions (see ue5-asset-naming skill):
- Blueprints: `B_` prefix
- Widget Blueprints: `W_` prefix
- Levels: `L_` prefix
- DeltaCode-generated assets additionally use `DC_` in the base name:
  `B_DC_RiflePickup`, `W_DC_MissionHUD`, `L_DC_ExtractionTest`

---

## Distribution Rules (Giving Away the Plugin)

When DeltaCode is distributed to other developers:

1. API key is NEVER embedded in source or config committed to version control
2. Key stored in `[Project]/Saved/Config/[Platform]/DeltaCode.ini` via UDeveloperSettings
   — this path is .gitignore'd by default in all UE5 projects
3. The .uplugin sets `"Installed": false` for source distribution
4. For binary-only distribution, set `"Installed": true` and use RunUAT BuildPlugin
5. Source/[Module]/*.cpp files can be deleted for binary-only distribution
   but Build.cs files MUST remain even for Blueprint-only access

---

## Anti-Patterns DeltaCode Must Never Generate

❌ A single module containing both gameplay actors AND editor UI
❌ `#include "UnrealEd.h"` or any Editor header in the Runtime module
❌ `LoadingPhase: "Default"` for an Editor module that registers Slate tabs
❌ Using `PublicDependencyModuleNames` for everything — over-exposes headers
❌ Omitting `IMPLEMENT_MODULE` macro in any module's .cpp
❌ Forgetting `ShutdownModule()` cleanup — causes hot reload crashes
❌ Asset paths hardcoded as FString literals — use TSoftObjectPtr or UDataAsset

---

## Skill Version
Version: 1.0.0
Target Engine: UE5.5+
Sources: Epic UE5 Modules Docs, Epic Plugins Docs, Lyra Starter Game analysis
Plugin: DeltaCode
Last Updated: 2026-03
