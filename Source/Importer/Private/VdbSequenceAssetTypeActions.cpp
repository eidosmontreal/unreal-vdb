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

#include "VdbSequenceAssetTypeActions.h"

#include "VdbVolumeSequence.h"

#include "EditorFramework/AssetImportData.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FVdbSequenceAssetTypeActions::FVdbSequenceAssetTypeActions(EAssetTypeCategories::Type InAssetCategory)
	: MyAssetCategory(InAssetCategory)
{
}

FText FVdbSequenceAssetTypeActions::GetName() const
{
	return LOCTEXT("FVdbSequenceAssetTypeActionsName", "NanoVdbSequence");
}

FColor FVdbSequenceAssetTypeActions::GetTypeColor() const
{
	return FColor::Silver;
}

UClass* FVdbSequenceAssetTypeActions::GetSupportedClass() const
{
	return UVdbVolumeSequence::StaticClass();
}

uint32 FVdbSequenceAssetTypeActions::GetCategories()
{
	return MyAssetCategory;
}

void FVdbSequenceAssetTypeActions::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		UVdbVolumeSequence* VdbVolumeSeq = CastChecked<UVdbVolumeSequence>(Asset);
		VdbVolumeSeq->GetAssetImportData()->ExtractFilenames(OutSourceFilePaths);
	}
}

bool FVdbSequenceAssetTypeActions::HasActions(const TArray<UObject*>& InObjects) const
{ 
	return false; 
}

bool FVdbSequenceAssetTypeActions::IsImportedAsset() const
{ 
	return true; 
}

#undef LOCTEXT_NAMESPACE
