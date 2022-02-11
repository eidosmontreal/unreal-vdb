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

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Misc/Paths.h"
#include "VdbCommon.h"

#include "VdbVolumeBase.generated.h"

class FVdbRenderBuffer;
class FVolumeRenderInfos;

// Base interface for NanoVDB file containers
UCLASS(BlueprintType, hidecategories = (Object))
class VOLUMERUNTIME_API UVdbVolumeBase : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	UVdbVolumeBase() = default;
	virtual ~UVdbVolumeBase() = default;

	EVdbType GetType() { return VdbType; };
	const FBox& GetBounds() const { return Bounds; }
	int GetMemorySize() const { return MemoryUsage; }
#if WITH_EDITORONLY_DATA
	class UAssetImportData* GetAssetImportData() { return AssetImportData; }
#endif

	void UpdateFromMetadata(const nanovdb::GridMetaData* MetaData);

	virtual bool IsValid() const PURE_VIRTUAL(UVdbVolumeBase::IsValid, return false;);
	virtual const FIntVector& GetIndexMin(uint32 FrameIndex) const PURE_VIRTUAL(UVdbVolumeBase::GetIndexMin, return FIntVector::ZeroValue;);
	virtual const FIntVector& GetIndexMax(uint32 FrameIndex) const PURE_VIRTUAL(UVdbVolumeBase::GetIndexMax, return FIntVector::ZeroValue;);
	virtual const FMatrix& GetIndexToLocal(uint32 FrameIndex) const PURE_VIRTUAL(UVdbVolumeBase::GetIndexToLocal, return FMatrix::Identity;);

	virtual const uint8* GetGridData(uint32 FrameIndex) const PURE_VIRTUAL(UVdbVolumeBase::GetGridData, return nullptr;);
	virtual const nanovdb::GridMetaData* GetMetaData(uint32 FrameIndex) PURE_VIRTUAL(UVdbVolumeBase::GetGridData, return nullptr;);

	// UObject Interface.
	virtual void PostInitProperties() override;
#if WITH_EDITORONLY_DATA
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
#endif

protected:

	UPROPERTY(VisibleAnywhere, Category = "Properties")
	EVdbType VdbType;

#if WITH_EDITORONLY_DATA
	UPROPERTY(BlueprintReadOnly, Instanced, Category = ImportSettings)
	TObjectPtr<class UAssetImportData> AssetImportData;

	UPROPERTY(VisibleAnywhere, Category = "Properties")
	FString GridName;

	UPROPERTY(VisibleAnywhere, Category = "Properties")
	FString Class;

	UPROPERTY(VisibleAnywhere, Category = "Properties")
	FString DataType;

	UPROPERTY(VisibleAnywhere, Category = "Properties", Meta = (DisplayName = "Total Memory"))
	FString MemoryUsageStr;
#endif

	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Properties")
	FBox Bounds;

	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Properties")
	FVector VoxelSize;

	UPROPERTY()
	uint64 MemoryUsage = 0;

	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Properties")
	EQuantizationType Quantization = EQuantizationType::None;
};



