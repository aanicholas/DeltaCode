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
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Interfaces/IHttpRequest.h"
#include "Settings/DeltaCodeSettings.h" // for EDCGenerationMode / EDCMissionTemplate
#include "Advisor/DCCodeBlock.h"

class SMultiLineEditableTextBox;
class SVerticalBox;
template <typename ItemType> class SListView;
template <typename OptionType> class SComboBox;
struct FDCAnthropicResponse;

/** Tracks which operation the panel is currently running. */
UENUM()
enum class EDCPanelActivity : uint8
{
	Idle,
	Generating,
	BuildingMission,
	CreatingAssets,
	Asking,
};

/**
 * One parsed-block entry tracked by the panel list view.
 *
 * Wraps FDCCodeBlock with enough UI state to render an "Apply" row and reflect
 * the outcome after the user clicks Apply (or Apply All).
 */
struct FDCCodeBlockEntry
{
	FDCCodeBlock Block;
	bool bApplied = false;
	EDCApplyResult LastResult = EDCApplyResult::Success;
	FString OutputPath;
	FString ErrorDetail;
};

/**
 * Main generator panel for the DeltaCode editor plugin.
 *
 * Hosts the mode/template selectors, prompt input, response display, and the
 * parsed-files list view that drives Safe Mode disk writes.
 *
 * [INPUT]  From: level editor toolbar button → FGlobalTabmanager
 * [OUTPUT] To:   FDCAnthropicClient → Anthropic /v1/messages; FDCFileWriter → disk
 */
class DELTACODEEDITOR_API SDeltaCodeGeneratorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDeltaCodeGeneratorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SDeltaCodeGeneratorPanel() override;

private:
	// ── Mode / template options ────────────────────────────────────────────

	TArray<TSharedPtr<EDCGenerationMode>> ModeOptions;
	TArray<TSharedPtr<EDCMissionTemplate>> TemplateOptions;
	TArray<TSharedPtr<EDCInspectorTopic>> InspectorTopicOptions;
	TSharedPtr<EDCGenerationMode> SelectedMode;
	TSharedPtr<EDCMissionTemplate> SelectedTemplate;
	TSharedPtr<EDCInspectorTopic> SelectedInspectorTopic;

	TSharedRef<SWidget> MakeModeOption(TSharedPtr<EDCGenerationMode> Option);
	TSharedRef<SWidget> MakeTemplateOption(TSharedPtr<EDCMissionTemplate> Option);
	TSharedRef<SWidget> MakeInspectorTopicOption(TSharedPtr<EDCInspectorTopic> Option);
	FText GetCurrentModeText() const;
	FText GetCurrentTemplateText() const;
	FText GetCurrentInspectorTopicText() const;
	void OnModeChanged(TSharedPtr<EDCGenerationMode> NewMode, ESelectInfo::Type Info);
	void OnTemplateChanged(TSharedPtr<EDCMissionTemplate> NewTemplate, ESelectInfo::Type Info);
	void OnInspectorTopicChanged(TSharedPtr<EDCInspectorTopic> NewTopic, ESelectInfo::Type Info);

	// ── Prompt + response widgets ──────────────────────────────────────────

	TSharedPtr<SMultiLineEditableTextBox> PromptBox;
	TSharedPtr<SMultiLineEditableTextBox> ResponseBox;

	FText ResponseText;
	FText StatusText;

	// ── Generate / cancel state ────────────────────────────────────────────

	FHttpRequestPtr ActiveRequest;
	EDCPanelActivity CurrentActivity = EDCPanelActivity::Idle;
	bool bWasCancelled = false;

	FReply OnGenerateClicked();
	FReply OnCancelClicked();
	void OnResponseReceived(const FDCAnthropicResponse& Response);

	/**
	 * Danger Zone "Build Mission" — runs the Python level pipeline directly
	 * for the currently selected template. Always prompts via FMessageDialog
	 * before invoking the bridge (Hard Constraint #2).
	 */
	FReply OnBuildMissionClicked();

	/** Creates core Blueprint assets under /Game/DeltaCode/Core/ via Python. */
	FReply OnCreateCoreAssetsClicked();

	/** Safe Mode "Run Inspector" — read-only project scan via Python bridge. */
	FReply OnRunInspectorClicked();

	/**
	 * Safe Mode "Ask DeltaCode" — runs the inspector silently, prepends the
	 * formatted scan to the user's prompt, sends to Anthropic with the Ask
	 * system prompt, and shows the prose response in the Response box.
	 * Bypasses the code-block parser; clears any prior parsed entries.
	 */
	FReply OnAskClicked();

	/**
	 * Anthropic callback for the Ask flow. Plain prose into ResponseBox; no
	 * code-block parsing. Clears Entries so prose isn't shown above stale
	 * Generate output.
	 */
	void OnAskResponseReceived(const FDCAnthropicResponse& Response);

	bool IsBusy() const;
	bool IsGenerateEnabled() const;
	bool IsBuildMissionEnabled() const;
	bool IsCreateCoreAssetsEnabled() const;
	bool IsAskEnabled() const;
	EVisibility GetCancelVisibility() const;
	EVisibility GetTemplateRowVisibility() const;
	EVisibility GetDangerWarningVisibility() const;
	EVisibility GetInspectorRowVisibility() const;
	EVisibility GetSpinnerVisibility() const;

	FText GetStatusText() const       { return StatusText; }
	FText GetResponseText() const     { return ResponseText; }
	FText GetPromptHintText() const;
	FText GetGenerateButtonText() const;
	FText GetBuildMissionButtonText() const;
	FText GetCreateCoreAssetsButtonText() const;
	FText GetAskButtonText() const;

	// ── Parsed-file list view ──────────────────────────────────────────────

	TArray<TSharedPtr<FDCCodeBlockEntry>> Entries;
	TSharedPtr<SListView<TSharedPtr<FDCCodeBlockEntry>>> EntriesList;

	TSharedRef<ITableRow> GenerateEntryRow(TSharedPtr<FDCCodeBlockEntry> Entry,
	                                       const TSharedRef<STableViewBase>& OwnerTable);

	FReply OnApplyEntryClicked(TSharedPtr<FDCCodeBlockEntry> Entry);
	FReply OnApplyAllClicked();

	/** Applies a single entry and refreshes its row — returns true on success. */
	bool ApplyEntryInPlace(const TSharedPtr<FDCCodeBlockEntry>& Entry);

	EVisibility GetEntriesPanelVisibility() const;
	FText GetApplyAllSummaryText() const;
};
