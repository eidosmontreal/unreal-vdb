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

#include "VdbVolumeBase.h"

#include "UnrealClient.h"
#include "UObject/MetaData.h"
#include "EditorFramework\AssetImportData.h"

FBox UVdbVolumeBase::ZeroBox = FBox(ForceInitToZero);

UVdbVolumeBase::UVdbVolumeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UVdbVolumeBase::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif
	Super::PostInitProperties();
}

#if WITH_EDITORONLY_DATA
void UVdbVolumeBase::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (AssetImportData)
	{
		OutTags.Add(FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden));
	}

	Super::GetAssetRegistryTags(OutTags);
}
#endif

void UVdbVolumeBase::UpdateFromMetadata(const nanovdb::GridMetaData* MetaData)
{
	nanovdb::Vec3R VoxSize = MetaData->voxelSize();
	VoxelSize = FVector3f(VoxSize[0], VoxSize[1], VoxSize[2]);

	if (MetaData->isLevelSet())
	{
		VdbClass = EVdbClass::SignedDistance;
	}
	else if (MetaData->isFogVolume())
	{
		VdbClass = EVdbClass::FogVolume;
	}
	else if (MetaData->isUnknown())
	{
		// I've ran across a few VDB files that don't define their type properly.
		// Even though it should be undefined, try the default value of FogVolume instead (and let's hope for the best)
		VdbClass = EVdbClass::FogVolume;
		UE_LOG(LogSparseVolumetrics, Warning, TEXT("VDB %s has an unknown type. Let's assume it is a FogVolume. If it isn't, be prepared for undefined behavior."), *GetName());
	}
	else
	{
		VdbClass = EVdbClass::Undefined;
		UE_LOG(LogSparseVolumetrics, Error, TEXT("VDB %s has an unsupported type. Be prepared for undefined behavior."), *GetName());
	}

	IsVolVector = MetaData->gridType() == nanovdb::GridType::Vec3f || MetaData->gridType() == nanovdb::GridType::Vec4f;

#if WITH_EDITORONLY_DATA
	DataType = FString(nanovdb::toStr(MetaData->gridType()));
	Class = FString(nanovdb::toStr(MetaData->gridClass()));
	GridName = MetaData->shortGridName();
	MemoryUsageStr = *GetMemoryString(MemoryUsage, false);
#endif
}
