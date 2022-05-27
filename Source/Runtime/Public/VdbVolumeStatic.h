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
#include "VdbVolumeBase.h"

#include "VdbVolumeStatic.generated.h"

class FVdbRenderBuffer;
class FVolumeRenderInfos;

// NanoVDB buffer container
UCLASS(BlueprintType, hidecategories = (Object))
class VOLUMERUNTIME_API UVdbVolumeStatic : public UVdbVolumeBase
{
	GENERATED_UCLASS_BODY()

public:

	UVdbVolumeStatic();
	virtual ~UVdbVolumeStatic();
	UVdbVolumeStatic(FVTableHelper& Helper);

#if WITH_EDITOR
	void Import(nanovdb::GridHandle<>&& GridHandle, EQuantizationType Quan);
#endif

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	
	// UVdbVolumeBase interface
	virtual bool IsValid() const;
	virtual const FBox& GetBounds(uint32 = 0) const { return VolumeFrameInfos.GetBounds(); }
	virtual const FIntVector& GetIndexMin(uint32 = 0) const { return VolumeFrameInfos.GetIndexMin(); }
	virtual const FIntVector& GetIndexMax(uint32 = 0) const { return VolumeFrameInfos.GetIndexMax(); }
	virtual const FMatrix44f& GetIndexToLocal(uint32 = 0) const { return VolumeFrameInfos.GetIndexToLocal(); }
	virtual const uint8* GetGridData(uint32 = 0) const { return VolumeRenderInfos.GetNanoGridHandle().data(); }
	virtual const nanovdb::GridMetaData* GetMetaData(uint32 = 0) { return VolumeRenderInfos.GetNanoGridHandle().gridMetaData(); }

	template<typename T>
	const nanovdb::NanoGrid<T>* GetNanoGrid() { return VolumeRenderInfos.GetNanoGridHandle().grid<T>(); }
	
	virtual const FVolumeRenderInfos* GetRenderInfos(uint32 = 0) const override { return &VolumeRenderInfos; }

private:

#if WITH_EDITOR
	void PrepareRendering();
#endif

	void InitResources();
	void ReleaseResources();

	TRefCountPtr<FVdbRenderBuffer> RenderResource;

	UPROPERTY(VisibleAnywhere, Category = "Properties")
	FVolumeFrameInfos VolumeFrameInfos;

	FVolumeRenderInfos VolumeRenderInfos;
};
