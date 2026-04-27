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
#include "UI/SDeltaCodeGeneratorPanel.h"
#include "API/DCAnthropicClient.h"
#include "Advisor/DCFileWriter.h"
#include "Advisor/DCResponseParser.h"
#include "Prompts/DCPromptBuilder.h"
#include "Python/DCLevelScriptingBridge.h"
#include "Settings/DeltaCodeSettings.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/SListView.h"
#include "Styling/AppStyle.h"
#include "Misc/MessageDialog.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SDeltaCodeGeneratorPanel"

namespace DCPanelPrivate
{
	template <typename T>
	TSharedPtr<T> FindOption(const TArray<TSharedPtr<T>>& Options, T Target)
	{
		for (const TSharedPtr<T>& Option : Options)
		{
			if (Option.IsValid() && *Option == Target)
			{
				return Option;
			}
		}
		return Options.Num() > 0 ? Options[0] : nullptr;
	}

	/** Cheap line count for the UI chip — caps at "many" for huge blocks. */
	static int32 CountLines(const FString& Content)
	{
		if (Content.IsEmpty())
		{
			return 0;
		}
		int32 Lines = 1;
		for (TCHAR Ch : Content)
		{
			if (Ch == TEXT('\n'))
			{
				++Lines;
			}
		}
		return Lines;
	}
}

// ─── Construction / teardown ─────────────────────────────────────────────────

void SDeltaCodeGeneratorPanel::Construct(const FArguments& InArgs)
{
	ModeOptions.Add(MakeShared<EDCGenerationMode>(EDCGenerationMode::Safe));
	ModeOptions.Add(MakeShared<EDCGenerationMode>(EDCGenerationMode::Danger));

	TemplateOptions.Add(MakeShared<EDCMissionTemplate>(EDCMissionTemplate::Extraction));
	TemplateOptions.Add(MakeShared<EDCMissionTemplate>(EDCMissionTemplate::Arena));
	TemplateOptions.Add(MakeShared<EDCMissionTemplate>(EDCMissionTemplate::QuestHub));
	TemplateOptions.Add(MakeShared<EDCMissionTemplate>(EDCMissionTemplate::ReactiveStory));

	InspectorTopicOptions.Add(MakeShared<EDCInspectorTopic>(EDCInspectorTopic::All));
	InspectorTopicOptions.Add(MakeShared<EDCInspectorTopic>(EDCInspectorTopic::Player));
	InspectorTopicOptions.Add(MakeShared<EDCInspectorTopic>(EDCInspectorTopic::Enemy));
	InspectorTopicOptions.Add(MakeShared<EDCInspectorTopic>(EDCInspectorTopic::Combat));
	InspectorTopicOptions.Add(MakeShared<EDCInspectorTopic>(EDCInspectorTopic::Animation));
	InspectorTopicOptions.Add(MakeShared<EDCInspectorTopic>(EDCInspectorTopic::Input));

	const UDeltaCodeSettings* Settings = UDeltaCodeSettings::Get();
	const EDCGenerationMode InitialMode =
		Settings ? Settings->DefaultGenerationMode : EDCGenerationMode::Safe;
	const EDCMissionTemplate InitialTemplate =
		Settings ? Settings->DefaultMissionTemplate : EDCMissionTemplate::Extraction;

	SelectedMode            = DCPanelPrivate::FindOption(ModeOptions, InitialMode);
	SelectedTemplate        = DCPanelPrivate::FindOption(TemplateOptions, InitialTemplate);
	SelectedInspectorTopic  = InspectorTopicOptions[0];

	StatusText = (InitialMode == EDCGenerationMode::Danger)
		? LOCTEXT("StatusIdleDanger", "Danger Zone: Select a template and hit Build Mission.")
		: LOCTEXT("StatusIdleSafe", "Safe Mode: Ask a question or describe what to generate.");

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			// ── Mode + template row ─────────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center).AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[ SNew(STextBlock).Text(LOCTEXT("ModeLabel", "Mode:")) ]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center).AutoWidth().Padding(0.0f, 0.0f, 16.0f, 0.0f)
				[
					SNew(SComboBox<TSharedPtr<EDCGenerationMode>>)
					.OptionsSource(&ModeOptions)
					.InitiallySelectedItem(SelectedMode)
					.OnGenerateWidget(this, &SDeltaCodeGeneratorPanel::MakeModeOption)
					.OnSelectionChanged(this, &SDeltaCodeGeneratorPanel::OnModeChanged)
					[ SNew(STextBlock).Text(this, &SDeltaCodeGeneratorPanel::GetCurrentModeText) ]
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center).AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TemplateLabel", "Template:"))
					.Visibility(this, &SDeltaCodeGeneratorPanel::GetTemplateRowVisibility)
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center).AutoWidth()
				[
					SNew(SComboBox<TSharedPtr<EDCMissionTemplate>>)
					.OptionsSource(&TemplateOptions)
					.InitiallySelectedItem(SelectedTemplate)
					.OnGenerateWidget(this, &SDeltaCodeGeneratorPanel::MakeTemplateOption)
					.OnSelectionChanged(this, &SDeltaCodeGeneratorPanel::OnTemplateChanged)
					.Visibility(this, &SDeltaCodeGeneratorPanel::GetTemplateRowVisibility)
					[ SNew(STextBlock).Text(this, &SDeltaCodeGeneratorPanel::GetCurrentTemplateText) ]
				]
			]

			// ── Danger banner ──────────────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("SettingsEditor.CheckoutWarningBorder"))
				.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f))
				.Padding(6.0f)
				.Visibility(this, &SDeltaCodeGeneratorPanel::GetDangerWarningVisibility)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DangerWarning",
						"Danger Zone can clear and rebuild level content. "
						"Destructive actions always prompt for confirmation before running."))
					.AutoWrapText(true)
					.ColorAndOpacity(FLinearColor(1.0f, 0.714f, 0.0f))
				]
			]

			// ── Inspect Project (Safe Mode) ───────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				.Padding(6.0f)
				.Visibility(this, &SDeltaCodeGeneratorPanel::GetInspectorRowVisibility)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("InspectProjectLabel", "Inspect Project"))
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("InspectProjectHint",
							"Read-only — scans the project content and reports player chars, "
							"enemies, damage system, animation, and input assets to the Output Log."))
						.AutoWrapText(true)
						.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					]

					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center).AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("InspectorTopicLabel", "Topic:"))
						]

						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center).AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
						[
							SNew(SComboBox<TSharedPtr<EDCInspectorTopic>>)
							.OptionsSource(&InspectorTopicOptions)
							.InitiallySelectedItem(SelectedInspectorTopic)
							.OnGenerateWidget(this, &SDeltaCodeGeneratorPanel::MakeInspectorTopicOption)
							.OnSelectionChanged(this, &SDeltaCodeGeneratorPanel::OnInspectorTopicChanged)
							[ SNew(STextBlock).Text(this, &SDeltaCodeGeneratorPanel::GetCurrentInspectorTopicText) ]
						]

						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center).AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("RunInspectorBtn", "Run Inspector"))
							.ToolTipText(LOCTEXT("RunInspectorTooltip",
								"Scan the project for the selected topic. Read-only — no assets are "
								"modified. Results appear in the Output Log."))
							.OnClicked(this, &SDeltaCodeGeneratorPanel::OnRunInspectorClicked)
						]
					]
				]
			]

			// ── Prompt / response splitter ────────────────────────────────
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)

				+ SSplitter::Slot().Value(0.45f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[ SNew(STextBlock).Text(LOCTEXT("PromptLabel", "Prompt")) ]

					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SAssignNew(PromptBox, SMultiLineEditableTextBox)
						.HintText(this, &SDeltaCodeGeneratorPanel::GetPromptHintText)
						.AutoWrapText(true)
						.AllowMultiLine(true)
					]
				]

				+ SSplitter::Slot().Value(0.55f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[ SNew(STextBlock).Text(LOCTEXT("ResponseLabel", "Response")) ]

					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SAssignNew(ResponseBox, SMultiLineEditableTextBox)
						.IsReadOnly(true)
						.AutoWrapText(true)
						.AllowMultiLine(true)
						.Text(this, &SDeltaCodeGeneratorPanel::GetResponseText)
					]
				]
			]

			// ── Parsed-files section ──────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				.Padding(6.0f)
				.Visibility(this, &SDeltaCodeGeneratorPanel::GetEntriesPanelVisibility)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(this, &SDeltaCodeGeneratorPanel::GetApplyAllSummaryText)
						]

						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SButton)
							.Text(LOCTEXT("ApplyAllBtn", "Apply All"))
							.ToolTipText(LOCTEXT("ApplyAllTooltip",
								"Write every block with a proposed path. Existing files are never overwritten."))
							.OnClicked(this, &SDeltaCodeGeneratorPanel::OnApplyAllClicked)
						]
					]

					+ SVerticalBox::Slot().AutoHeight().MaxHeight(240.0f)
					[
						SAssignNew(EntriesList, SListView<TSharedPtr<FDCCodeBlockEntry>>)
						.ListItemsSource(&Entries)
						.OnGenerateRow(this, &SDeltaCodeGeneratorPanel::GenerateEntryRow)
						.SelectionMode(ESelectionMode::None)
					]
				]
			]

			// ── Status + action buttons ────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &SDeltaCodeGeneratorPanel::GetStatusText)
						.AutoWrapText(true)
					]

					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SThrobber)
						.Visibility(this, &SDeltaCodeGeneratorPanel::GetSpinnerVisibility)
					]
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(this, &SDeltaCodeGeneratorPanel::GetCreateCoreAssetsButtonText)
					.ToolTipText(LOCTEXT("CreateCoreAssetsTooltip",
						"Create required Blueprint assets under /Game/DeltaCode/Core/ if they "
						"don't already exist. Safe — skips assets that are already created. "
						"(Danger Zone only)"))
					.IsEnabled(this, &SDeltaCodeGeneratorPanel::IsCreateCoreAssetsEnabled)
					.OnClicked(this, &SDeltaCodeGeneratorPanel::OnCreateCoreAssetsClicked)
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(this, &SDeltaCodeGeneratorPanel::GetBuildMissionButtonText)
					.ToolTipText(LOCTEXT("BuildMissionTooltip",
						"Clear the current level and rebuild it from the selected mission template. "
						"Runs the DeltaCode Python pipeline. Always prompts before executing. "
						"(Danger Zone only)"))
					.IsEnabled(this, &SDeltaCodeGeneratorPanel::IsBuildMissionEnabled)
					.OnClicked(this, &SDeltaCodeGeneratorPanel::OnBuildMissionClicked)
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CancelBtn", "Cancel"))
					.Visibility(this, &SDeltaCodeGeneratorPanel::GetCancelVisibility)
					.OnClicked(this, &SDeltaCodeGeneratorPanel::OnCancelClicked)
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(this, &SDeltaCodeGeneratorPanel::GetAskButtonText)
					.ToolTipText(LOCTEXT("AskTooltip",
						"Scan the project (read-only), include the scan as context, and ask "
						"DeltaCode to answer your question in plain English. (Safe Mode only)"))
					.IsEnabled(this, &SDeltaCodeGeneratorPanel::IsAskEnabled)
					.OnClicked(this, &SDeltaCodeGeneratorPanel::OnAskClicked)
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(this, &SDeltaCodeGeneratorPanel::GetGenerateButtonText)
					.IsEnabled(this, &SDeltaCodeGeneratorPanel::IsGenerateEnabled)
					.OnClicked(this, &SDeltaCodeGeneratorPanel::OnGenerateClicked)
				]
			]
		]
	];
}

SDeltaCodeGeneratorPanel::~SDeltaCodeGeneratorPanel()
{
	if (ActiveRequest.IsValid())
	{
		ActiveRequest->CancelRequest();
		ActiveRequest.Reset();
	}
}

// ─── Combo option widgets ────────────────────────────────────────────────────

TSharedRef<SWidget> SDeltaCodeGeneratorPanel::MakeModeOption(TSharedPtr<EDCGenerationMode> Option)
{
	const EDCGenerationMode Value = Option.IsValid() ? *Option : EDCGenerationMode::Safe;
	return SNew(STextBlock).Text(FDCPromptBuilder::ModeDisplayName(Value));
}

TSharedRef<SWidget> SDeltaCodeGeneratorPanel::MakeTemplateOption(TSharedPtr<EDCMissionTemplate> Option)
{
	const EDCMissionTemplate Value = Option.IsValid() ? *Option : EDCMissionTemplate::Extraction;
	return SNew(STextBlock).Text(FDCPromptBuilder::TemplateDisplayName(Value));
}

FText SDeltaCodeGeneratorPanel::GetCurrentModeText() const
{
	return FDCPromptBuilder::ModeDisplayName(
		SelectedMode.IsValid() ? *SelectedMode : EDCGenerationMode::Safe);
}

FText SDeltaCodeGeneratorPanel::GetCurrentTemplateText() const
{
	return FDCPromptBuilder::TemplateDisplayName(
		SelectedTemplate.IsValid() ? *SelectedTemplate : EDCMissionTemplate::Extraction);
}

void SDeltaCodeGeneratorPanel::OnModeChanged(TSharedPtr<EDCGenerationMode> NewMode, ESelectInfo::Type)
{
	SelectedMode = NewMode;

	const EDCGenerationMode Mode = NewMode.IsValid() ? *NewMode : EDCGenerationMode::Safe;
	StatusText = (Mode == EDCGenerationMode::Danger)
		? LOCTEXT("StatusIdleDanger", "Danger Zone: Select a template and hit Build Mission.")
		: LOCTEXT("StatusIdleSafe", "Safe Mode: Ask a question or describe what to generate.");

	if (PromptBox.IsValid())
	{
		PromptBox->SetText(FText::GetEmpty());
	}
}

void SDeltaCodeGeneratorPanel::OnTemplateChanged(TSharedPtr<EDCMissionTemplate> NewTemplate, ESelectInfo::Type)
{
	SelectedTemplate = NewTemplate;
}

TSharedRef<SWidget> SDeltaCodeGeneratorPanel::MakeInspectorTopicOption(TSharedPtr<EDCInspectorTopic> Option)
{
	const EDCInspectorTopic Value = Option.IsValid() ? *Option : EDCInspectorTopic::All;
	if (const UEnum* EnumPtr = StaticEnum<EDCInspectorTopic>())
	{
		return SNew(STextBlock).Text(EnumPtr->GetDisplayNameTextByValue((int64)Value));
	}
	return SNew(STextBlock).Text(LOCTEXT("InspectorTopicAllFallback", "All"));
}

FText SDeltaCodeGeneratorPanel::GetCurrentInspectorTopicText() const
{
	const EDCInspectorTopic Value = SelectedInspectorTopic.IsValid()
		? *SelectedInspectorTopic : EDCInspectorTopic::All;
	if (const UEnum* EnumPtr = StaticEnum<EDCInspectorTopic>())
	{
		return EnumPtr->GetDisplayNameTextByValue((int64)Value);
	}
	return LOCTEXT("InspectorTopicAllFallback", "All");
}

void SDeltaCodeGeneratorPanel::OnInspectorTopicChanged(TSharedPtr<EDCInspectorTopic> NewTopic, ESelectInfo::Type)
{
	SelectedInspectorTopic = NewTopic;
}

// ─── Visibility / enabled state ──────────────────────────────────────────────

bool SDeltaCodeGeneratorPanel::IsBusy() const
{
	return CurrentActivity != EDCPanelActivity::Idle;
}

bool SDeltaCodeGeneratorPanel::IsGenerateEnabled() const
{
	if (IsBusy()) { return false; }
	const EDCGenerationMode Mode = SelectedMode.IsValid() ? *SelectedMode : EDCGenerationMode::Safe;
	return Mode == EDCGenerationMode::Safe;
}

bool SDeltaCodeGeneratorPanel::IsBuildMissionEnabled() const
{
	if (IsBusy()) { return false; }
	const EDCGenerationMode Mode = SelectedMode.IsValid() ? *SelectedMode : EDCGenerationMode::Safe;
	return Mode == EDCGenerationMode::Danger;
}

bool SDeltaCodeGeneratorPanel::IsCreateCoreAssetsEnabled() const
{
	if (IsBusy()) { return false; }
	const EDCGenerationMode Mode = SelectedMode.IsValid() ? *SelectedMode : EDCGenerationMode::Safe;
	return Mode == EDCGenerationMode::Danger;
}

bool SDeltaCodeGeneratorPanel::IsAskEnabled() const
{
	if (IsBusy()) { return false; }
	const EDCGenerationMode Mode = SelectedMode.IsValid() ? *SelectedMode : EDCGenerationMode::Safe;
	if (Mode != EDCGenerationMode::Safe) { return false; }
	if (!PromptBox.IsValid()) { return false; }
	return !PromptBox->GetText().ToString().TrimStartAndEnd().IsEmpty();
}

EVisibility SDeltaCodeGeneratorPanel::GetCancelVisibility() const
{
	return CurrentActivity == EDCPanelActivity::Generating
		? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SDeltaCodeGeneratorPanel::GetTemplateRowVisibility() const
{
	const EDCGenerationMode Mode = SelectedMode.IsValid() ? *SelectedMode : EDCGenerationMode::Safe;
	return Mode == EDCGenerationMode::Danger ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SDeltaCodeGeneratorPanel::GetDangerWarningVisibility() const
{
	const EDCGenerationMode Mode = SelectedMode.IsValid() ? *SelectedMode : EDCGenerationMode::Safe;
	return Mode == EDCGenerationMode::Danger ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SDeltaCodeGeneratorPanel::GetInspectorRowVisibility() const
{
	const EDCGenerationMode Mode = SelectedMode.IsValid() ? *SelectedMode : EDCGenerationMode::Safe;
	return Mode == EDCGenerationMode::Safe ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SDeltaCodeGeneratorPanel::GetSpinnerVisibility() const
{
	return IsBusy() ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SDeltaCodeGeneratorPanel::GetPromptHintText() const
{
	const EDCGenerationMode Mode = SelectedMode.IsValid() ? *SelectedMode : EDCGenerationMode::Safe;
	return Mode == EDCGenerationMode::Danger
		? LOCTEXT("PromptHintDanger",
			"Prompt-driven Danger Zone generation coming soon. "
			"Use the Template dropdown and Build Mission button.")
		: LOCTEXT("PromptHintSafe",
			"Ask a question about your project, or describe what to generate.");
}

FText SDeltaCodeGeneratorPanel::GetGenerateButtonText() const
{
	return CurrentActivity == EDCPanelActivity::Generating
		? LOCTEXT("GeneratingBtn", "Generating\u2026")
		: LOCTEXT("GenerateBtn", "Generate");
}

FText SDeltaCodeGeneratorPanel::GetBuildMissionButtonText() const
{
	return CurrentActivity == EDCPanelActivity::BuildingMission
		? LOCTEXT("BuildingBtn", "Building\u2026")
		: LOCTEXT("BuildMissionBtn", "Build Mission");
}

FText SDeltaCodeGeneratorPanel::GetCreateCoreAssetsButtonText() const
{
	return CurrentActivity == EDCPanelActivity::CreatingAssets
		? LOCTEXT("CreatingAssetsBtn", "Creating Assets\u2026")
		: LOCTEXT("CreateCoreAssetsBtn", "Create Core Assets");
}

FText SDeltaCodeGeneratorPanel::GetAskButtonText() const
{
	return CurrentActivity == EDCPanelActivity::Asking
		? LOCTEXT("AskingBtn", "Asking\u2026")
		: LOCTEXT("AskBtn", "Ask DeltaCode");
}

EVisibility SDeltaCodeGeneratorPanel::GetEntriesPanelVisibility() const
{
	return Entries.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SDeltaCodeGeneratorPanel::GetApplyAllSummaryText() const
{
	if (Entries.Num() == 0)
	{
		return FText::GetEmpty();
	}
	int32 Applied = 0;
	int32 Appliable = 0;
	for (const TSharedPtr<FDCCodeBlockEntry>& Entry : Entries)
	{
		if (!Entry.IsValid()) continue;
		if (!Entry->Block.ProposedPath.IsEmpty() && !Entry->bApplied) { ++Appliable; }
		if (Entry->bApplied && Entry->LastResult == EDCApplyResult::Success) { ++Applied; }
	}
	return FText::Format(
		LOCTEXT("EntriesSummaryFmt", "{0} parsed · {1} pending · {2} applied"),
		FText::AsNumber(Entries.Num()),
		FText::AsNumber(Appliable),
		FText::AsNumber(Applied));
}

// ─── Generate / cancel ───────────────────────────────────────────────────────

FReply SDeltaCodeGeneratorPanel::OnGenerateClicked()
{
	if (IsBusy())
	{
		return FReply::Handled();
	}

	const FString PromptString = PromptBox.IsValid() ? PromptBox->GetText().ToString() : FString();
	if (PromptString.TrimStartAndEnd().IsEmpty())
	{
		StatusText = LOCTEXT("StatusEmpty", "Prompt is empty.");
		return FReply::Handled();
	}

	const EDCGenerationMode Mode =
		SelectedMode.IsValid() ? *SelectedMode : EDCGenerationMode::Safe;
	const EDCMissionTemplate Template =
		SelectedTemplate.IsValid() ? *SelectedTemplate : EDCMissionTemplate::Extraction;

	if (Mode == EDCGenerationMode::Danger)
	{
		const EAppReturnType::Type Answer = FMessageDialog::Open(
			EAppMsgType::YesNo,
			LOCTEXT("DangerConfirm",
				"Danger Zone can produce scripts that clear and rebuild level content.\n\n"
				"Proceed with a Danger Zone generation?"));
		if (Answer != EAppReturnType::Yes)
		{
			StatusText = LOCTEXT("StatusDangerDeclined", "Danger Zone request cancelled.");
			return FReply::Handled();
		}
	}

	FDCAnthropicRequest Request;
	Request.SystemPrompt = FDCPromptBuilder::BuildSystemPrompt(Mode, Template);
	Request.Messages.Add(FDCAnthropicMessage::User(PromptString));

	CurrentActivity = EDCPanelActivity::Generating;
	bWasCancelled = false;
	ResponseText = FText::GetEmpty();
	Entries.Reset();
	if (EntriesList.IsValid()) { EntriesList->RequestListRefresh(); }
	StatusText = LOCTEXT("StatusSending", "Sending request\u2026");

	TWeakPtr<SDeltaCodeGeneratorPanel> WeakSelf = SharedThis(this);
	ActiveRequest = FDCAnthropicClient::Send(
		Request,
		FDCOnAnthropicResponse::CreateLambda(
			[WeakSelf](const FDCAnthropicResponse& Response)
			{
				if (TSharedPtr<SDeltaCodeGeneratorPanel> Pinned = WeakSelf.Pin())
				{
					Pinned->OnResponseReceived(Response);
				}
			}));

	return FReply::Handled();
}

FReply SDeltaCodeGeneratorPanel::OnBuildMissionClicked()
{
	if (IsBusy()) { return FReply::Handled(); }

	const EDCMissionTemplate Template =
		SelectedTemplate.IsValid() ? *SelectedTemplate : EDCMissionTemplate::Extraction;
	const FText TemplateName = FDCPromptBuilder::TemplateDisplayName(Template);

	const FText ConfirmBody = FText::Format(
		LOCTEXT("BuildMissionConfirmFmt",
			"This will CLEAR the active level and rebuild it from the '{0}' template.\n\n"
			"Non-essential actors will be destroyed. Lighting, sky, and post-process "
			"volumes are preserved. This action can be undone from the editor.\n\n"
			"Proceed?"),
		TemplateName);

	const EAppReturnType::Type Answer = FMessageDialog::Open(
		EAppMsgType::YesNo,
		ConfirmBody,
		LOCTEXT("BuildMissionConfirmTitle", "DeltaCode \u2014 Build Mission"));

	if (Answer != EAppReturnType::Yes)
	{
		StatusText = LOCTEXT("BuildMissionDeclined", "Build Mission cancelled.");
		return FReply::Handled();
	}

	CurrentActivity = EDCPanelActivity::BuildingMission;
	StatusText = FText::Format(
		LOCTEXT("BuildMissionRunningFmt", "Running Danger Zone pipeline for '{0}'\u2026"),
		TemplateName);

	FString Message;
	const bool bBuildSucceeded = FDCLevelScriptingBridge::ExecuteDangerZoneScript(Template, Message);

	CurrentActivity = EDCPanelActivity::Idle;
	StatusText = FText::FromString(Message);

	// Toast on success — flags the UE5.5+ NavMesh-after-restart quirk where
	// pathing data only finalises after the level is saved and the editor
	// reloaded. Status bar message gets overwritten by the next interaction;
	// the toast persists in the editor notification corner long enough for
	// the user to read it.
	if (bBuildSucceeded)
	{
		FNotificationInfo Info(LOCTEXT("BuildCompleteNavMeshNotice",
			"Build complete. Save and restart editor if enemies are not moving (NavMesh initialization)."));
		Info.ExpireDuration = 8.0f;
		Info.bFireAndForget = true;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	return FReply::Handled();
}

FReply SDeltaCodeGeneratorPanel::OnCreateCoreAssetsClicked()
{
	if (IsBusy()) { return FReply::Handled(); }

	CurrentActivity = EDCPanelActivity::CreatingAssets;
	StatusText = LOCTEXT("CoreAssetsCreating", "Creating core Blueprint assets\u2026");

	FString Message;
	FDCLevelScriptingBridge::CreateCoreAssets(Message);

	CurrentActivity = EDCPanelActivity::Idle;
	StatusText = FText::FromString(Message);
	return FReply::Handled();
}

FReply SDeltaCodeGeneratorPanel::OnRunInspectorClicked()
{
	// Inspector is synchronous and quick; per design we skip activity gating
	// and don't introduce a new EDCPanelActivity value for it. The verbose
	// report still goes to the Output Log (ExecuteProjectInspector); the
	// LLM-formatted scan is also rendered into the Response box so users
	// don't have to dig through LogPython for results.
	const EDCInspectorTopic Topic = SelectedInspectorTopic.IsValid()
		? *SelectedInspectorTopic : EDCInspectorTopic::All;

	StatusText = LOCTEXT("InspectorRunning", "Running project inspector\u2026");

	FString LogMessage;
	FDCLevelScriptingBridge::ExecuteProjectInspector(Topic, LogMessage);

	FString FormattedScan;
	FString BridgeMessage;
	const bool bScanOk = FDCLevelScriptingBridge::RunInspectorForLLM(
		Topic, FormattedScan, BridgeMessage);

	if (bScanOk)
	{
		// Clear any prior parsed code blocks so the scan prose stands alone.
		ResponseText = FText::FromString(FormattedScan);
		Entries.Reset();
		if (EntriesList.IsValid()) { EntriesList->RequestListRefresh(); }
		StatusText = FText::Format(
			LOCTEXT("InspectorDoneFmt",
				"{0} See Output Log for full report."),
			FText::FromString(BridgeMessage));
	}
	else
	{
		// Bridge failed \u2014 leave Response box untouched, surface the reason.
		StatusText = FText::FromString(BridgeMessage);
	}

	return FReply::Handled();
}

FReply SDeltaCodeGeneratorPanel::OnAskClicked()
{
	if (IsBusy())
	{
		return FReply::Handled();
	}

	const FString PromptString = PromptBox.IsValid() ? PromptBox->GetText().ToString() : FString();
	if (PromptString.TrimStartAndEnd().IsEmpty())
	{
		StatusText = LOCTEXT("StatusEmpty", "Prompt is empty.");
		return FReply::Handled();
	}

	// Phase 1: scan the project (synchronous Python call). On failure we surface
	// the bridge's status and bail before firing the network request.
	CurrentActivity = EDCPanelActivity::Asking;
	StatusText = LOCTEXT("AskInspecting", "Scanning project\u2026");

	FString FormattedScan;
	FString BridgeMessage;
	const bool bScanOk = FDCLevelScriptingBridge::RunInspectorForLLM(
		EDCInspectorTopic::All, FormattedScan, BridgeMessage);
	if (!bScanOk)
	{
		CurrentActivity = EDCPanelActivity::Idle;
		StatusText = FText::FromString(BridgeMessage);
		return FReply::Handled();
	}

	// Phase 2: build the request. System prompt is the verbatim Ask preamble;
	// user message is the formatted scan followed by the user's question.
	FDCAnthropicRequest Request;
	Request.SystemPrompt = FDCPromptBuilder::BuildAskSystemPrompt();
	const FString UserMessage = FString::Printf(
		TEXT("%s\nQuestion: %s"), *FormattedScan, *PromptString);
	Request.Messages.Add(FDCAnthropicMessage::User(UserMessage));

	// Reset prior Generate output before showing the prose answer.
	bWasCancelled = false;
	ResponseText = FText::GetEmpty();
	Entries.Reset();
	if (EntriesList.IsValid()) { EntriesList->RequestListRefresh(); }
	StatusText = LOCTEXT("AskSending", "Asking DeltaCode\u2026");

	TWeakPtr<SDeltaCodeGeneratorPanel> WeakSelf = SharedThis(this);
	ActiveRequest = FDCAnthropicClient::Send(
		Request,
		FDCOnAnthropicResponse::CreateLambda(
			[WeakSelf](const FDCAnthropicResponse& Response)
			{
				if (TSharedPtr<SDeltaCodeGeneratorPanel> Pinned = WeakSelf.Pin())
				{
					Pinned->OnAskResponseReceived(Response);
				}
			}));

	return FReply::Handled();
}

void SDeltaCodeGeneratorPanel::OnAskResponseReceived(const FDCAnthropicResponse& Response)
{
	CurrentActivity = EDCPanelActivity::Idle;
	ActiveRequest.Reset();

	if (bWasCancelled)
	{
		bWasCancelled = false;
		StatusText = LOCTEXT("StatusCancelled", "Generation cancelled.");
		return;
	}

	if (Response.bSuccess)
	{
		ResponseText = FText::FromString(Response.Text);
		// Keep Entries cleared so prose isn't shown above stale code blocks.
		Entries.Reset();
		if (EntriesList.IsValid()) { EntriesList->RequestListRefresh(); }
		StatusText = FText::Format(
			LOCTEXT("AskDoneFmt",
				"Answer received. {0} in / {1} out tokens. Stop: {2}."),
			FText::AsNumber(Response.InputTokens),
			FText::AsNumber(Response.OutputTokens),
			FText::FromString(Response.StopReason.IsEmpty() ? TEXT("\u2014") : Response.StopReason));
	}
	else
	{
		StatusText = FText::Format(
			LOCTEXT("StatusFailedFmt", "Failed: {0}"),
			FText::FromString(Response.ErrorMessage));
	}
}

FReply SDeltaCodeGeneratorPanel::OnCancelClicked()
{
	if (ActiveRequest.IsValid())
	{
		bWasCancelled = true;
		ActiveRequest->CancelRequest();
	}
	return FReply::Handled();
}

void SDeltaCodeGeneratorPanel::OnResponseReceived(const FDCAnthropicResponse& Response)
{
	CurrentActivity = EDCPanelActivity::Idle;
	ActiveRequest.Reset();

	if (bWasCancelled)
	{
		bWasCancelled = false;
		StatusText = LOCTEXT("StatusCancelled", "Generation cancelled.");
		return;
	}

	if (Response.bSuccess)
	{
		ResponseText = FText::FromString(Response.Text);

		// Parse the response into per-file entries for the Apply UI.
		Entries.Reset();
		const TArray<FDCCodeBlock> Blocks = FDCResponseParser::Parse(Response.Text);
		for (const FDCCodeBlock& Block : Blocks)
		{
			TSharedPtr<FDCCodeBlockEntry> Entry = MakeShared<FDCCodeBlockEntry>();
			Entry->Block = Block;
			Entries.Add(MoveTemp(Entry));
		}
		if (EntriesList.IsValid())
		{
			EntriesList->RequestListRefresh();
		}

		StatusText = FText::Format(
			LOCTEXT("StatusDoneFmt",
				"Done. {0} in / {1} out tokens. Stop: {2}. {3} code blocks parsed."),
			FText::AsNumber(Response.InputTokens),
			FText::AsNumber(Response.OutputTokens),
			FText::FromString(Response.StopReason.IsEmpty() ? TEXT("—") : Response.StopReason),
			FText::AsNumber(Entries.Num()));
	}
	else
	{
		StatusText = FText::Format(
			LOCTEXT("StatusFailedFmt", "Failed: {0}"),
			FText::FromString(Response.ErrorMessage));
	}
}

// ─── Parsed-file list view ───────────────────────────────────────────────────

TSharedRef<ITableRow> SDeltaCodeGeneratorPanel::GenerateEntryRow(
	TSharedPtr<FDCCodeBlockEntry> Entry,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	const FString Lang = Entry.IsValid() ? Entry->Block.Language : FString();
	const FString Path = Entry.IsValid() ? Entry->Block.ProposedPath : FString();
	const int32 Lines = Entry.IsValid() ? DCPanelPrivate::CountLines(Entry->Block.Content) : 0;

	// Status text updates live off the entry pointer via a lambda.
	TWeakPtr<FDCCodeBlockEntry> WeakEntry = Entry;
	auto GetStatus = [WeakEntry]() -> FText
	{
		TSharedPtr<FDCCodeBlockEntry> Pinned = WeakEntry.Pin();
		if (!Pinned.IsValid())
		{
			return FText::GetEmpty();
		}
		if (Pinned->bApplied)
		{
			const FString Detail = (Pinned->LastResult == EDCApplyResult::Success)
				? Pinned->OutputPath
				: Pinned->ErrorDetail;
			return FDCFileWriter::ResultToText(Pinned->LastResult, Detail);
		}
		return Pinned->Block.ProposedPath.IsEmpty()
			? NSLOCTEXT("SDeltaCodeGeneratorPanel", "EntryNoPath", "No path — informational block.")
			: FText::Format(
				NSLOCTEXT("SDeltaCodeGeneratorPanel", "EntryPendingFmt", "Pending — {0}"),
				FText::FromString(Pinned->Block.ParseNote));
	};

	auto IsApplyEnabled = [WeakEntry]() -> bool
	{
		TSharedPtr<FDCCodeBlockEntry> Pinned = WeakEntry.Pin();
		return Pinned.IsValid()
			&& !Pinned->Block.ProposedPath.IsEmpty()
			&& !(Pinned->bApplied && Pinned->LastResult == EDCApplyResult::Success);
	};

	const FText HeaderText = FText::Format(
		NSLOCTEXT("SDeltaCodeGeneratorPanel", "EntryHeaderFmt", "[{0}] {1}  ({2} lines)"),
		FText::FromString(Lang.IsEmpty() ? TEXT("?") : Lang),
		FText::FromString(Path.IsEmpty() ? TEXT("<unnamed>") : Path),
		FText::AsNumber(Lines));

	return SNew(STableRow<TSharedPtr<FDCCodeBlockEntry>>, OwnerTable)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f).VAlign(VAlign_Center).Padding(4.0f, 2.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[ SNew(STextBlock).Text(HeaderText) ]

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda(GetStatus)
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 2.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("ApplyBtn", "Apply"))
			.IsEnabled_Lambda(IsApplyEnabled)
			.OnClicked(this, &SDeltaCodeGeneratorPanel::OnApplyEntryClicked, Entry)
		]
	];
}

FReply SDeltaCodeGeneratorPanel::OnApplyEntryClicked(TSharedPtr<FDCCodeBlockEntry> Entry)
{
	ApplyEntryInPlace(Entry);
	if (EntriesList.IsValid())
	{
		EntriesList->RequestListRefresh();
	}
	return FReply::Handled();
}

FReply SDeltaCodeGeneratorPanel::OnApplyAllClicked()
{
	int32 Succeeded = 0;
	int32 Skipped = 0;
	int32 Failed = 0;

	for (const TSharedPtr<FDCCodeBlockEntry>& Entry : Entries)
	{
		if (!Entry.IsValid())
		{
			continue;
		}
		if (Entry->bApplied && Entry->LastResult == EDCApplyResult::Success)
		{
			continue; // already applied — skip
		}
		if (Entry->Block.ProposedPath.IsEmpty())
		{
			++Skipped;
			continue;
		}
		if (ApplyEntryInPlace(Entry))
		{
			++Succeeded;
		}
		else
		{
			++Failed;
		}
	}

	if (EntriesList.IsValid())
	{
		EntriesList->RequestListRefresh();
	}

	StatusText = FText::Format(
		LOCTEXT("ApplyAllSummaryFmt",
			"Apply All: {0} written, {1} failed, {2} skipped (no path)."),
		FText::AsNumber(Succeeded),
		FText::AsNumber(Failed),
		FText::AsNumber(Skipped));

	return FReply::Handled();
}

bool SDeltaCodeGeneratorPanel::ApplyEntryInPlace(const TSharedPtr<FDCCodeBlockEntry>& Entry)
{
	if (!Entry.IsValid())
	{
		return false;
	}

	FString AbsolutePath;
	FString ErrorDetail;
	const EDCApplyResult Result = FDCFileWriter::Apply(Entry->Block, AbsolutePath, ErrorDetail);

	Entry->bApplied = true;
	Entry->LastResult = Result;
	Entry->OutputPath = AbsolutePath;
	Entry->ErrorDetail = ErrorDetail;

	return Result == EDCApplyResult::Success;
}

#undef LOCTEXT_NAMESPACE
