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

#include "VdbReimportFactory.h"
#include "VdbVolumeBase.h"

#include "HAL/FileManager.h"
#include "EditorFramework\AssetImportData.h"

#define LOCTEXT_NAMESPACE "VdbImporterFactory"

DEFINE_LOG_CATEGORY_STATIC(LogVdbReimport, Log, All);

UVdbReimportFactory::UVdbReimportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UVdbReimportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UVdbVolumeBase* VdbVolume = Cast<UVdbVolumeBase>(Obj);
	if (VdbVolume && VdbVolume->GetAssetImportData())
	{
		VdbVolume->GetAssetImportData()->ExtractFilenames(OutFilenames);
		return true;
	}
	return false;
}

void UVdbReimportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UVdbVolumeBase* VdbVolume = Cast<UVdbVolumeBase>(Obj);
	if (VdbVolume && VdbVolume->GetAssetImportData() && ensure(NewReimportPaths.Num() == 1))
	{
		VdbVolume->GetAssetImportData()->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UVdbReimportFactory::Reimport(UObject* Obj)
{
	UVdbVolumeBase* VdbVolume = Cast<UVdbVolumeBase>(Obj);
	if (!VdbVolume)
	{
		return EReimportResult::Failed;
	}

	// Make sure file is valid and exists
	const FString Filename = VdbVolume->GetAssetImportData()->GetFirstFilename();
	if (!Filename.Len() || IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
	{
		return EReimportResult::Failed;
	}

	// Run the import again
	EReimportResult::Type Result = EReimportResult::Failed;
	bool OutCanceled = false;

	if (ImportObject(VdbVolume->GetClass(), VdbVolume->GetOuter(), *VdbVolume->GetName(), RF_Public | RF_Standalone, Filename, nullptr, OutCanceled) != nullptr)
	{
		UE_LOG(LogVdbReimport, Log, TEXT("Imported successfully"));

		VdbVolume->GetAssetImportData()->Update(Filename);

		// Try to find the outer package so we can dirty it up
		if (VdbVolume->GetOuter())
		{
			VdbVolume->GetOuter()->MarkPackageDirty();
		}
		else
		{
			VdbVolume->MarkPackageDirty();
		}
		Result = EReimportResult::Succeeded;
	}
	else
	{
		if (OutCanceled)
		{
			UE_LOG(LogVdbReimport, Warning, TEXT("-- import canceled"));
		}
		else
		{
			UE_LOG(LogVdbReimport, Warning, TEXT("-- import failed"));
		}

		Result = EReimportResult::Failed;
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
