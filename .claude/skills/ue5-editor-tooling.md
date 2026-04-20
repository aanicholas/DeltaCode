# SKILL: ue5-editor-tooling
# DeltaCode Plugin — Slate UI, Editor Subsystems, API Key Storage

## Purpose
This skill governs the construction of DeltaCode's Editor-facing interface: the docked
panel, mode toggle (Safe / Danger Zone), prompt input, response display, toolbar button,
and the API key management system. It also covers UDeveloperSettings — the correct UE5
pattern for per-user configuration that is never committed to source control.

This skill is active whenever DeltaCode generates or modifies Editor UI code.

---

## DeltaCode Panel Architecture

The UI consists of one docked Slate tab registered in the Level Editor:

```
FDeltaCodeEditorModule::StartupModule()
  └── Registers SDockTab "DeltaCodeTab" via FGlobalTabmanager
        └── SDeltaCodePanel (SCompoundWidget)
              ├── Mode Toggle Row        [Safe Mode] [⚡ Danger Zone]
              ├── Context Display        (current mode description)
              ├── Prompt Input           SMultiLineEditableTextBox
              ├── Submit Button          SButton → triggers API call
              ├── Response Display       SScrollBox > STextBlock (streaming)
              └── Status Bar             (model, token count, API key status)
```

---

## Slate Panel Implementation

### SDeltaCodePanel.h
```cpp
// Copyright DeltaCode. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

enum class EDeltaCodeMode : uint8
{
    SafeMode,
    DangerZone
};

class DELTACODEEDITOR_API SDeltaCodePanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDeltaCodePanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    // Mode state
    EDeltaCodeMode CurrentMode = EDeltaCodeMode::SafeMode;
    FReply OnSafeModeClicked();
    FReply OnDangerZoneClicked();

    // Prompt
    TSharedPtr<SMultiLineEditableTextBox> PromptInputBox;
    FReply OnSubmitPrompt();
    void OnAPIResponseReceived(const FString& ResponseText);

    // Response display
    TSharedPtr<STextBlock> ResponseTextBlock;
    TSharedPtr<SScrollBox> ResponseScrollBox;

    // Mode-aware appearance
    FSlateColor GetModeButtonColor(EDeltaCodeMode ForMode) const;
    FText GetModeDescriptionText() const;

    // API key
    bool IsAPIKeyConfigured() const;
    FText GetAPIKeyStatusText() const;
};
```

### SDeltaCodePanel.cpp — Construct() pattern
```cpp
void SDeltaCodePanel::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SVerticalBox)

        // ── Mode Toggle ──────────────────────────────────────────
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.0f, 8.0f, 8.0f, 4.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("SafeModeBtn", "Safe Mode"))
                .ButtonColorAndOpacity(this, &SDeltaCodePanel::GetModeButtonColor,
                                       EDeltaCodeMode::SafeMode)
                .OnClicked(this, &SDeltaCodePanel::OnSafeModeClicked)
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .Padding(4.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("DangerZoneBtn", "⚡ Danger Zone"))
                .ButtonColorAndOpacity(this, &SDeltaCodePanel::GetModeButtonColor,
                                       EDeltaCodeMode::DangerZone)
                .OnClicked(this, &SDeltaCodePanel::OnDangerZoneClicked)
            ]
        ]

        // ── Mode Description ─────────────────────────────────────
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text(this, &SDeltaCodePanel::GetModeDescriptionText)
            .AutoWrapText(true)
        ]

        // ── Prompt Input ─────────────────────────────────────────
        + SVerticalBox::Slot()
        .FillHeight(0.3f)
        .Padding(8.0f, 8.0f)
        [
            SAssignNew(PromptInputBox, SMultiLineEditableTextBox)
            .HintText(LOCTEXT("PromptHint", "Describe what you want DeltaCode to create..."))
        ]

        // ── Submit ───────────────────────────────────────────────
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.0f, 0.0f, 8.0f, 8.0f)
        [
            SNew(SButton)
            .Text(LOCTEXT("SubmitBtn", "Generate"))
            .IsEnabled(this, &SDeltaCodePanel::IsAPIKeyConfigured)
            .OnClicked(this, &SDeltaCodePanel::OnSubmitPrompt)
        ]

        // ── Response Display ─────────────────────────────────────
        + SVerticalBox::Slot()
        .FillHeight(0.7f)
        .Padding(8.0f, 0.0f, 8.0f, 8.0f)
        [
            SAssignNew(ResponseScrollBox, SScrollBox)
            + SScrollBox::Slot()
            [
                SAssignNew(ResponseTextBlock, STextBlock)
                .AutoWrapText(true)
            ]
        ]

        // ── Status Bar ───────────────────────────────────────────
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.0f, 0.0f, 8.0f, 4.0f)
        [
            SNew(STextBlock)
            .Text(this, &SDeltaCodePanel::GetAPIKeyStatusText)
        ]
    ];
}
```

---

## Toolbar Button Registration

Register a toolbar button that opens the DeltaCode panel:

```cpp
void FDeltaCodeEditorModule::RegisterMenuExtensions()
{
    FLevelEditorModule& LevelEditorModule =
        FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

    TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
    ToolbarExtender->AddToolBarExtension(
        "Settings",
        EExtensionHook::After,
        PluginCommands,
        FToolBarExtensionDelegate::CreateRaw(
            this, &FDeltaCodeEditorModule::AddToolbarButton)
    );
    LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
}
```

---

## API Key Storage — UDeveloperSettings Pattern

This is the CORRECT pattern for storing a per-user API key in UE5.
It uses UDeveloperSettings which saves to `Saved/Config/[Platform]/DeltaCode.ini`.
This path is .gitignore'd by default. Keys NEVER enter source control.

### UDeltaCodeSettings.h
```cpp
// Copyright DeltaCode. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DeltaCodeSettings.generated.h"

UCLASS(Config=DeltaCode, defaultconfig, meta=(DisplayName="DeltaCode"))
class DELTACODEEDITOR_API UDeltaCodeSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UDeltaCodeSettings();

    // Returns the settings category in Project Settings
    virtual FName GetCategoryName() const override { return FName("Plugins"); }

    // The Anthropic API key. Stored in Saved/Config — never committed to source control.
    // Each user must enter their own key obtained from console.anthropic.com
    UPROPERTY(Config, EditAnywhere, Category="DeltaCode|API",
              meta=(DisplayName="Anthropic API Key",
                    Tooltip="Your personal Anthropic API key. Get one at console.anthropic.com"))
    FString AnthropicAPIKey;

    // Target model. Default: claude-sonnet-4-20250514
    UPROPERTY(Config, EditAnywhere, Category="DeltaCode|API",
              meta=(DisplayName="Claude Model"))
    FString ClaudeModel = TEXT("claude-sonnet-4-20250514");

    // Static accessor
    static UDeltaCodeSettings* Get()
    {
        return GetMutableDefault<UDeltaCodeSettings>();
    }

    bool HasValidAPIKey() const
    {
        return !AnthropicAPIKey.IsEmpty() && AnthropicAPIKey.StartsWith(TEXT("sk-ant-"));
    }
};
```

### Accessing the API key anywhere in the Editor module
```cpp
#include "Settings/DeltaCodeSettings.h"

FString APIKey = UDeltaCodeSettings::Get()->AnthropicAPIKey;
if (!UDeltaCodeSettings::Get()->HasValidAPIKey())
{
    // Show first-run modal prompting user to enter key
    ShowAPIKeySetupDialog();
    return;
}
```

### First-Run API Key Dialog
When `HasValidAPIKey()` returns false on panel open, show a modal:
```cpp
void SDeltaCodePanel::ShowAPIKeySetupDialog()
{
    TSharedRef<SWindow> KeyWindow = SNew(SWindow)
        .Title(LOCTEXT("APIKeyTitle", "Enter Your Anthropic API Key"))
        .SizingRule(ESizingRule::FixedSize)
        .ClientSize(FVector2D(500, 160));

    // Content: instruction text + SEditableTextBox + Save button
    // On save: write to UDeltaCodeSettings, validate with lightweight API ping
    // Validation call: POST /v1/messages with max_tokens:1, model:claude-haiku
}
```

---

## Tab Registration Pattern

```cpp
void FDeltaCodeEditorModule::RegisterTabSpawners()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        FName("DeltaCodeTab"),
        FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
        {
            return SNew(SDockTab)
                .TabRole(ETabRole::NomadTab)
                [
                    SNew(SDeltaCodePanel)
                ];
        })
    )
    .SetDisplayName(LOCTEXT("DeltaCodeTabTitle", "DeltaCode"))
    .SetMenuType(ETabSpawnerMenuType::Hidden);
}
```

---

## Danger Zone UI Rules

When Danger Zone mode is active, the panel MUST:
1. Display a red/orange visual indicator — color the mode button `FLinearColor(0.8f, 0.2f, 0.0f)`
2. Show a warning banner: "⚡ DANGER ZONE — Level operations are destructive and cannot be undone."
3. Show a mission type selector (dropdown or button row): Extraction / Destiny / Fallout / Open World
4. Disable the Generate button until a mission type is selected
5. On submit, show a confirmation modal: "This will delete all actors in the current level. Continue?"

Safe Mode colors the active button `FLinearColor(0.1f, 0.5f, 0.2f)` (green).

---

## Anti-Patterns DeltaCode Must Never Generate

❌ Storing the API key in a UPROPERTY with `Config` in the Runtime module
   (Runtime config files can be cooked into shipped games — key would be exposed)
❌ Hardcoding any API key in source code
❌ Using SWindow modally for the main panel — use a docked SDockTab
❌ Registering toolbar extensions in the Runtime module
❌ Calling FGlobalTabmanager in ShutdownModule without first unregistering
❌ Opening Danger Zone operations without a confirmation dialog
❌ Using deprecated EditorStyle:: calls — use FAppStyle:: in UE5.1+

---

## Skill Version
Version: 1.0.0
Target Engine: UE5.5+
Sources: Epic Slate docs, UDeveloperSettings docs, Lyra editor patterns
Plugin: DeltaCode
Last Updated: 2026-03
