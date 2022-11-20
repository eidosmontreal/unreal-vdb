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

#include "VdbAssetTypeActions.h"
#include "VdbVolumeStatic.h"
#include "VdbToVolumeTextureFactory.h"

#include "EditorFramework/AssetImportData.h"
#include "Engine/VolumeTexture.h"
#include "ToolMenus.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FVdbAssetTypeActions::FVdbAssetTypeActions(EAssetTypeCategories::Type InAssetCategory)
	: MyAssetCategory(InAssetCategory)
{
}

FText FVdbAssetTypeActions::GetName() const
{
	return LOCTEXT("FVdbAssetTypeActionsName", "NanoVdb");
}

FColor FVdbAssetTypeActions::GetTypeColor() const
{
	return FColor::Silver;
}

UClass* FVdbAssetTypeActions::GetSupportedClass() const
{
	return UVdbVolumeStatic::StaticClass();
}

uint32 FVdbAssetTypeActions::GetCategories()
{
	return MyAssetCategory;
}

void FVdbAssetTypeActions::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	auto VdbVolumes = GetTypedWeakObjectPtrs<UVdbVolumeStatic>(InObjects);

	Section.AddMenuEntry(
		"VdbVolume_CreateVolumeTexture",
		LOCTEXT("VdbVolume_CreateVolumeTexture", "Create Volume Texture"),
		LOCTEXT("VdbVolume_CreateVolumeTextureTooltip", "Creates a Volume texture and copies content from Vdb Volume."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FVdbAssetTypeActions::ExecuteConvertToVolume, VdbVolumes),
			FCanExecuteAction()
		)
	);
}

void FVdbAssetTypeActions::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		UVdbVolumeStatic* VdbVolumeStatic = CastChecked<UVdbVolumeStatic>(Asset);
		VdbVolumeStatic->GetAssetImportData()->ExtractFilenames(OutSourceFilePaths);
	}
}

//////////////////////////////////////////////////////////////////////////


void FVdbAssetTypeActions::ExecuteConvertToVolume(TArray<TWeakObjectPtr<UVdbVolumeStatic>> Objects)
{
	const FString DefaultSuffix = TEXT("_Volume");

	for (auto Object : Objects)
	{
		if (Object.Get())
		{
			// Determine the asset name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			UVdbToVolumeTextureFactory* Factory = NewObject<UVdbToVolumeTextureFactory>();
			Factory->InitialVdbVolume = Object.Get();
			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), UVolumeTexture::StaticClass(), Factory);
		}
	}
}


#undef LOCTEXT_NAMESPACE
