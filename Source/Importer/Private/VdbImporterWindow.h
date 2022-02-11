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

#pragma once

#include "Framework/Application/SlateApplication.h"
#include "Widgets/SCompoundWidget.h"

struct FVdbGridInfo
{
	FName GridName;
	FString Type;
	FString Class;
	FString Dimensions;
	FString ActiveVoxels;
	//FString MemorySize; // remove until openvdb is fixed and returns correct value
	bool ShouldImport = true;
};

typedef TSharedPtr<FVdbGridInfo> FVdbGridInfoPtr;

class SVdbImporterWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVdbImporterWindow) {}

	SLATE_ARGUMENT(UObject*, ImportOptions);
	SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow);
	SLATE_ARGUMENT(FText, FileNameText);
	SLATE_ARGUMENT(FText, FilePathText);
	SLATE_ARGUMENT(FText, PackagePathText);
	SLATE_ARGUMENT(TArray<FVdbGridInfoPtr>, VdbGridsInfo);
	SLATE_END_ARGS();

public:
	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	bool ShouldImport() const;

private:
	FReply OnImport();
	FReply OnCancel();

	TSharedRef<ITableRow> OnGenerateWidgetForList(FVdbGridInfoPtr InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void OnToggleAllItems(ECheckBoxState CheckType);
	void OnItemDoubleClicked(FVdbGridInfoPtr ClickedItem);

private:
	UObject* ImportOptions;
	TWeakPtr<SWindow> Window;
	bool              bShouldImport;

	TArray<FVdbGridInfoPtr> VdbGridsInfo;
};

inline bool SVdbImporterWindow::ShouldImport() const
{
	return bShouldImport;
}
