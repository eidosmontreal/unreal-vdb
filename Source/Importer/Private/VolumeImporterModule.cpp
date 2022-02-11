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

#include "VolumeImporterModule.h"
#include "VdbAssetTypeActions.h"
#include "VdbSequenceAssetTypeActions.h"

#include "Interfaces/IPluginManager.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "FVdbImporterModule"

class FVdbImporterModule : public IVolumeImporterModule
{
public:
	
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	void InitInterfaceCustomization();
	void ReleaseInterfaceCustomization();

	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);

	/** All created asset type actions.  Cached here so that we can unregister them during shutdown. */
	TArray< TSharedPtr<IAssetTypeActions> > CreatedAssetTypeActions;
	
	void* LibraryHandleBlosc = nullptr;
};

// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
void FVdbImporterModule::StartupModule()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("SparseVolumetrics");
	FString BaseDir = Plugin->GetBaseDir();

	FString BinaryDir = FPaths::Combine(BaseDir, TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory());
	FString BloscDll = FPaths::Combine(BinaryDir, TEXT("blosc.dll"));
	LibraryHandleBlosc = FPlatformProcess::GetDllHandle(*BloscDll);

	// Add custom Editor interface callback
	InitInterfaceCustomization();
}

void FVdbImporterModule::ShutdownModule()
{
	// Free the dll handle
	FPlatformProcess::FreeDllHandle(LibraryHandleBlosc);
	LibraryHandleBlosc = nullptr;

	ReleaseInterfaceCustomization();
}

void FVdbImporterModule::InitInterfaceCustomization()
{
	// TODO if we want:
	// Register interface extensions (FExtensibilityManager)
	// Register slate style overrides
	// Register commands
	// Register brokers (IComponentAssetBroker)
	// Register the details customizations
	// Register to be notified when properties are edited
	// Register to be notified when an asset is reimported
	// Register the thumbnail renderers
	// Register the editor modes
	// Register with the mesh paint module
	// Register tutorial category
	// Register Settings

	// Register asset types
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	EAssetTypeCategories::Type VdbAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("VdbVolume")), LOCTEXT("VdbAssetCategory", "VdbVolume"));

	RegisterAssetTypeAction(AssetTools, MakeShareable(new FVdbAssetTypeActions(VdbAssetCategoryBit)));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FVdbSequenceAssetTypeActions(VdbAssetCategoryBit)));
}

void FVdbImporterModule::ReleaseInterfaceCustomization()
{
	// Unregister all the asset types that we registered
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (int32 Index = 0; Index < CreatedAssetTypeActions.Num(); ++Index)
		{
			AssetTools.UnregisterAssetTypeActions(CreatedAssetTypeActions[Index].ToSharedRef());
		}
	}
	CreatedAssetTypeActions.Empty();
}

void FVdbImporterModule::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FVdbImporterModule, VolumeImporter)
