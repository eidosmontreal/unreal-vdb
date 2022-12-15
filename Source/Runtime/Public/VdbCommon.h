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
#include "Stats/Stats.h"
#include "HAL/ConsoleManager.h"

THIRD_PARTY_INCLUDES_START
#include <nanovdb/NanoVDB.h>
#include <nanovdb/util/GridHandle.h>
THIRD_PARTY_INCLUDES_END

#include "VdbCommon.generated.h"

#define NUM_EXTRA_VDBS 8

DECLARE_LOG_CATEGORY_EXTERN(LogSparseVolumetrics, Log, All);

DECLARE_STATS_GROUP(TEXT("VolumetricVdb"), STATGROUP_Vdb, STATCAT_Advanced);
DECLARE_MEMORY_STAT(TEXT("Vdb GPU data interface memory"), STAT_VdbGPUDataInterfaceMemory, STATGROUP_Vdb);

DECLARE_STATS_GROUP(TEXT("VdbOverview"), STATGROUP_VdbOverview, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("RT Total"), STAT_VdbRendering_RT, STATGROUP_VdbOverview);
DECLARE_CYCLE_STAT(TEXT("RT Opaque"), STAT_VdbOpaque_RT, STATGROUP_VdbOverview);
DECLARE_CYCLE_STAT(TEXT("RT Translucent"), STAT_VdbTranslucent_RT, STATGROUP_VdbOverview);
DECLARE_CYCLE_STAT(TEXT("RT Principled"), STAT_VdbPrincipled_RT, STATGROUP_VdbOverview);
DECLARE_CYCLE_STAT(TEXT("RT GetDynMeshElements"), STAT_VdbSceneProxy_GetDynamicMeshElements, STATGROUP_VdbOverview);

struct FVdbCVars
{
	static TAutoConsoleVariable<bool> CVarVolumetricVdb;
	static TAutoConsoleVariable<bool> CVarVolumetricVdbTrilinear;
	static TAutoConsoleVariable<int32> CVarVolumetricVdbCinematicQuality;
	static TAutoConsoleVariable<int32> CVarVolumetricVdbDenoiser;
	static TAutoConsoleVariable<float> CVarVolumetricVdbThreshold;
	static TAutoConsoleVariable<bool> CVarVolumetricVdbAfterTransparents;
};

class FVdbRenderBuffer;

// Based on nanovdb::GridType enum
UENUM(BlueprintType)
enum class EQuantizationType : uint8
{
	// keep original data type
	None,
	// 4bit quantization of float point value
	Fp4,
	// 8bit quantization of float point value
	Fp8,
	// 16bit quantization of float point value
	Fp16,
	// variable bit quantization of floating point value
	FpN,
};

UENUM()
enum class EVdbClass : uint8
{
	FogVolume,
	SignedDistance,
	Undefined
};

// Store per-frame Volume information
USTRUCT()
struct VOLUMERUNTIME_API FVolumeFrameInfos
{
	GENERATED_USTRUCT_BODY()

public:
	FVolumeFrameInfos();

#if WITH_EDITOR
	void UpdateFrame(nanovdb::GridHandle<>& NanoGridHandle);
#endif

	const FIntVector& GetIndexMin() const { return IndexMin; }
	const FIntVector& GetIndexMax() const { return IndexMax; }
	const FMatrix44f& GetIndexToLocal() const { return IndexToLocal; }
	const FIntVector& GetSize() const { return Size; }
	const FBox& GetBounds() const{ return Bounds; }
	uint64 GetMemoryUsage() const { return MemoryUsage; }

private:

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Properties")
	uint32 NumberActiveVoxels = 0;
#endif

	UPROPERTY(VisibleAnywhere, Category = "Properties")
	FMatrix44f IndexToLocal = FMatrix44f::Identity;

	UPROPERTY(VisibleAnywhere, Category = "Properties")
	FBox Bounds;

	UPROPERTY(VisibleAnywhere, Category = "Properties")
	FIntVector Size = FIntVector::ZeroValue;

	UPROPERTY(VisibleAnywhere, Category = "Properties")
	FIntVector IndexMin = FIntVector::ZeroValue;

	UPROPERTY(VisibleAnywhere, Category = "Properties")
	FIntVector IndexMax = FIntVector::ZeroValue;

	UPROPERTY(VisibleAnywhere, Category = "Properties")
	uint64 MemoryUsage = 0;

	VOLUMERUNTIME_API friend FArchive& operator<<(FArchive& Ar, FVolumeFrameInfos& VdbVolumeInfos);
};

class FVolumeRenderInfos
{
public:
	
	FVdbRenderBuffer* GetRenderResource() const { return RenderResource; }
	void ReleaseResources(bool ClearGrid);

	const nanovdb::GridHandle<nanovdb::HostBuffer>& GetNanoGridHandle() const { return NanoGridHandle; }
	nanovdb::GridHandle<nanovdb::HostBuffer>& GetNanoGridHandle() { return NanoGridHandle; }
	const FVector3f& GetIndexMin() const { return IndexMin; }
	const FVector3f& GetIndexSize() const { return IndexSize; }
	const FMatrix44f& GetIndexToLocal() const { return IndexToLocal; }
	bool IsVectorGrid() const;

	bool HasNanoGridData() const;	

	void Update(const FMatrix44f& InIndexToLocal, const FIntVector& InIndexMin, const FIntVector& InIndexMax, const TRefCountPtr<FVdbRenderBuffer>& RenderResource);

protected:	
	FMatrix44f IndexToLocal;
	FVector3f IndexMin;
	FVector3f IndexSize;
	TRefCountPtr<FVdbRenderBuffer> RenderResource;

	// this keeps buffers in memory
	nanovdb::GridHandle<nanovdb::HostBuffer> NanoGridHandle;
};

VOLUMERUNTIME_API FArchive& operator<<(FArchive& Ar, nanovdb::GridHandle<nanovdb::HostBuffer>& NanoGridHandle);
