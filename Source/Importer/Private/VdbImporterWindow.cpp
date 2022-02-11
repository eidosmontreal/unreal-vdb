// Copyright 2022 Eidos-Montreal / Eidos-Sherbrooke

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http ://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "VdbImporterWindow.h"
#include "VdbImporterOptions.h"

#include "SlateOptMacros.h"
#include "EditorStyleSet.h"
#include "HAL/PlatformProcess.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "VdbImporterWindow"

class SVdbTableRow : public SMultiColumnTableRow<FVdbGridInfoPtr>
{
public:

	SLATE_BEGIN_ARGS(SVdbTableRow) { }
	SLATE_ARGUMENT(FVdbGridInfoPtr, GridInfo)
	SLATE_END_ARGS()

public:

	/**
	* Constructs the widget.
	*
	* @param InArgs The construction arguments.
	* @param InOwnerTableView The table view that owns this row.
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		check(InArgs._GridInfo.IsValid());

		VdbGridInfo = InArgs._GridInfo;

		SMultiColumnTableRow<FVdbGridInfoPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

public:

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == "ShouldImport")
		{
			return SNew(SBox).Padding(FMargin(4.0f, 0.0f)).VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked(this, &SVdbTableRow::ShouldImportEnabled)
					.OnCheckStateChanged(this, &SVdbTableRow::OnChangeShouldImport)
				];
		}
		else if (ColumnName == "GridName")
		{
			return SNew(SBox).Padding(FMargin(4.0f, 0.0f)).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(VdbGridInfo->GridName.ToString()))
				];
		}
		else if (ColumnName == "Type")
		{
			return SNew(SBox).Padding(FMargin(4.0f, 0.0f)).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(VdbGridInfo->Type))
				];
		}
		else if (ColumnName == "Class")
		{
			return SNew(SBox).Padding(FMargin(4.0f, 0.0f)).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(VdbGridInfo->Class))
				];
		}
		else if (ColumnName == "Dimensions")
		{
			return SNew(SBox).Padding(FMargin(4.0f, 0.0f)).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(VdbGridInfo->Dimensions))
				];
		}
		else if (ColumnName == "ActiveVoxels")
		{
			return SNew(SBox).Padding(FMargin(4.0f, 0.0f)).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(VdbGridInfo->ActiveVoxels))
				];
		}
		// openvdb returns wrong memory size (unless VDB is previously parsed). Remove until it's fixed
		//else if (ColumnName == "MemorySize")
		//{
		//	return SNew(SBox).Padding(FMargin(4.0f, 0.0f)).VAlign(VAlign_Center)
		//		[
		//			SNew(STextBlock).Text(FText::FromString(VdbGridInfo->MemorySize))
		//		];
		//}

		return SNullWidget::NullWidget;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

private:
	ECheckBoxState ShouldImportEnabled() const
	{
		return VdbGridInfo->ShouldImport ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	void OnChangeShouldImport(ECheckBoxState NewState)
	{
		VdbGridInfo->ShouldImport = (NewState == ECheckBoxState::Checked);
	}

private:
	FVdbGridInfoPtr VdbGridInfo;
	FText InformationText;
};

void SVdbImporterWindow::Construct(const FArguments& InArgs)
{
	ImportOptions = InArgs._ImportOptions;
	Window = InArgs._WidgetWindow;
	bShouldImport = false;

	TSharedPtr<SBox> DetailsViewBox;
	const FText VersionText(FText::Format(LOCTEXT("VdbImporterWindow_Version", " Version   {0}"), FText::FromString("1.0")));

	VdbGridsInfo = InArgs._VdbGridsInfo;

	ChildSlot
		[
			SNew(SVerticalBox)

	+ SVerticalBox::Slot()
		.Padding(0.0f, 10.0f)
		.AutoHeight()
		[
			SNew(SInlineEditableTextBlock)
			.IsReadOnly(true)
			.Text(InArgs._FileNameText)
			.ToolTipText(InArgs._FilePathText)
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SInlineEditableTextBlock)
			.IsReadOnly(true)
			.Text(InArgs._PackagePathText)
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SBox)
				.MinDesiredWidth(512.0f)
				[
					SNew(SListView<FVdbGridInfoPtr>)
					.ItemHeight(24)
					.ScrollbarVisibility(EVisibility::Visible)
					.ListItemsSource(&VdbGridsInfo)
					.OnMouseButtonDoubleClick(this, &SVdbImporterWindow::OnItemDoubleClicked)
					.OnGenerateRow(this, &SVdbImporterWindow::OnGenerateWidgetForList)
					.HeaderRow
					(
						SNew(SHeaderRow)

						+ SHeaderRow::Column("ShouldImport").FillWidth(0.1f).DefaultLabel(FText::FromString(TEXT("Include")))
						[
							SNew(SCheckBox)
							.HAlign(HAlign_Center)
							.OnCheckStateChanged(this, &SVdbImporterWindow::OnToggleAllItems)
						]
						+ SHeaderRow::Column("GridName").DefaultLabel(LOCTEXT("GridName", "Grid Name")).FillWidth(0.25f)
						+ SHeaderRow::Column("Type").DefaultLabel(LOCTEXT("GridType", "Type")).FillWidth(0.1f)
						+ SHeaderRow::Column("Class").DefaultLabel(LOCTEXT("GridClass", "Class")).FillWidth(0.15f)
						+ SHeaderRow::Column("Dimensions").DefaultLabel(LOCTEXT("GridDimensions", "Dimensions")).FillWidth(0.15f)
						+ SHeaderRow::Column("ActiveVoxels").DefaultLabel(LOCTEXT("GridActiveVoxels", "Active Voxels")).FillWidth(0.15f)
						//+ SHeaderRow::Column("MemorySize").DefaultLabel(LOCTEXT("GridMemorySize", "Memory Size")).FillWidth(0.15f)
					)
				]
			]
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SAssignNew(DetailsViewBox, SBox)
			.MinDesiredHeight(320.0f)
		.MinDesiredWidth(450.0f)
		]

	+SVerticalBox::Slot()
		.MaxHeight(50)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(5)

			+ SUniformGridPanel::Slot(0, 0)
			.HAlign(HAlign_Left)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(5)
				+ SUniformGridPanel::Slot(0, 0)
				.HAlign(HAlign_Left)
				[
					SNew(SInlineEditableTextBlock)
					.IsReadOnly(true)
				.Text(VersionText)
				]
			]

			+ SUniformGridPanel::Slot(1, 0)
			.HAlign(HAlign_Right)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(5)
				+ SUniformGridPanel::Slot(0, 0)
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("VDBOptionWindow_Import", "Import"))
				.ToolTipText(LOCTEXT("VDBOptionWindow_Import_ToolTip", "Import file"))
				.OnClicked(this, &SVdbImporterWindow::OnImport)
				]
			+ SUniformGridPanel::Slot(1, 0)
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Text(LOCTEXT("VDBOptionWindow_Cancel", "Cancel"))
					.ToolTipText(LOCTEXT("VDBOptionWindow_Cancel_ToolTip", "Cancel import"))
					.OnClicked(this, &SVdbImporterWindow::OnCancel)
				]
			]
		]
	];

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bAllowMultipleTopLevelObjects = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	TSharedPtr<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsViewBox->SetContent(DetailsView.ToSharedRef());
	DetailsView->SetObject(ImportOptions);
}

FReply SVdbImporterWindow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) /*override*/
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		return OnCancel();
	}

	return FReply::Unhandled();
}

FReply SVdbImporterWindow::OnImport()
{
	bShouldImport = true;
	if (Window.IsValid())
	{
		Window.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FReply SVdbImporterWindow::OnCancel()
{
	bShouldImport = false;
	if (Window.IsValid())
	{
		Window.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

TSharedRef<ITableRow> SVdbImporterWindow::OnGenerateWidgetForList(FVdbGridInfoPtr InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SVdbTableRow, OwnerTable)
		.GridInfo(InItem);
}

void SVdbImporterWindow::OnToggleAllItems(ECheckBoxState CheckType)
{
	/** Set all items to top level checkbox state */
	for (FVdbGridInfoPtr& Item : VdbGridsInfo)
	{
		Item->ShouldImport = CheckType == ECheckBoxState::Checked;
	}
}

void SVdbImporterWindow::OnItemDoubleClicked(FVdbGridInfoPtr ClickedItem)
{
	/** Toggle state on / off for the selected list entry */
	for (FVdbGridInfoPtr& Item : VdbGridsInfo)
	{
		if (Item == ClickedItem)
		{
			Item->ShouldImport = !Item->ShouldImport;
		}
	}
}

#undef LOCTEXT_NAMESPACE
