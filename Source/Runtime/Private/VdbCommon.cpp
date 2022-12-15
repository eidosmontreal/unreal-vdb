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

#include "VdbCommon.h"

#include "RenderResource.h"
#include "Rendering/VdbRenderBuffer.h"
#include "VdbCustomVersion.h"

TAutoConsoleVariable<bool> FVdbCVars::CVarVolumetricVdb(
	TEXT("r.Vdb"), true,
	TEXT("VolumetricVdb components are rendered when true, otherwise ignored."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<bool> FVdbCVars::CVarVolumetricVdbTrilinear(
	TEXT("r.Vdb.Trilinear"), false,
	TEXT("Force Trilinear sampling on all Vdb volumes."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> FVdbCVars::CVarVolumetricVdbCinematicQuality(
	TEXT("r.Vdb.CinematicQuality"), 0,
	TEXT("Force better cinematic quality on all Vdb volumes. Recommended during Movie render Queue, this allows great renders while keeping faster realtime viewport displays with lower quality. Please be aware that this may crash your GPU and Unreal with high settings.\n If 1, Step sizes are divided by 4x, samples per pixels mult x2.\n If 2, step sizes are divided by 10x, samples per pixels mult x4 AND trilinear sampling is forced to true."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

TAutoConsoleVariable<int32> FVdbCVars::CVarVolumetricVdbDenoiser(
	TEXT("r.Vdb.Denoiser"), -1,
	TEXT("Denoiser method applied on Vdb FogVolumes. Used only if >= 0. Otherwise, fallback to engine value."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

TAutoConsoleVariable<float> FVdbCVars::CVarVolumetricVdbThreshold(
	TEXT("r.Vdb.Threshold"), 0.01,
	TEXT("Transmittance threshold to stop raymarching. Lower values are better but more expensive. Must be close to 0."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

TAutoConsoleVariable<bool> FVdbCVars::CVarVolumetricVdbAfterTransparents(
	TEXT("r.Vdb.AfterTransparents"), false,
	TEXT("VDBs are rendered before transparent objects by default (false). If true, VDBs will be rendered after transparent objects."),
	ECVF_RenderThreadSafe);

FArchive& operator<<(FArchive& Ar, nanovdb::GridHandle<nanovdb::HostBuffer>& NanoGridHandle)
{
	Ar.UsingCustomVersion(FVdbCustomVersion::GUID);

	// Custom NanoVDB Buffer serialization
	uint64 BufferByteSize = NanoGridHandle.size();
	Ar << BufferByteSize;

	if (Ar.IsLoading())
	{
		NanoGridHandle.buffer().init(BufferByteSize);
	}

	Ar.Serialize(NanoGridHandle.data(), NanoGridHandle.size());

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FVolumeFrameInfos& VdbVolumeInfos)
{
#if WITH_EDITORONLY_DATA
	Ar << VdbVolumeInfos.NumberActiveVoxels;
#endif
	Ar << VdbVolumeInfos.IndexToLocal;
	Ar << VdbVolumeInfos.Bounds;
	Ar << VdbVolumeInfos.Size;
	Ar << VdbVolumeInfos.IndexMin;
	Ar << VdbVolumeInfos.IndexMax;
	Ar << VdbVolumeInfos.MemoryUsage;

	return Ar;
}

FVolumeFrameInfos::FVolumeFrameInfos()
	: Bounds(ForceInit)
{}

#if WITH_EDITOR
void FVolumeFrameInfos::UpdateFrame(nanovdb::GridHandle<>& NanoGridHandle)
{
	const nanovdb::GridMetaData* MetaData = NanoGridHandle.gridMetaData();
	
	const nanovdb::Map& vdbMap = MetaData->map();
	IndexToLocal = FMatrix44f(
		FVector3f(vdbMap.mMatF[0], vdbMap.mMatF[3], vdbMap.mMatF[6]),
		FVector3f(vdbMap.mMatF[1], vdbMap.mMatF[4], vdbMap.mMatF[7]),
		FVector3f(vdbMap.mMatF[2], vdbMap.mMatF[5], vdbMap.mMatF[8]),
		FVector3f(vdbMap.mVecF[0], vdbMap.mVecF[1], vdbMap.mVecF[2]));

	const nanovdb::BBox<nanovdb::Vec3R>& WorldBBox = MetaData->worldBBox();
	FVector3f Min(WorldBBox.min()[0], WorldBBox.min()[1], WorldBBox.min()[2]);
	FVector3f Max(WorldBBox.max()[0], WorldBBox.max()[1], WorldBBox.max()[2]);
	Bounds = FBox(Min, Max);

	const nanovdb::BBox<nanovdb::Vec3R>& IndexBBox = MetaData->indexBBox();
	IndexMin = FIntVector(IndexBBox.min()[0], IndexBBox.min()[1], IndexBBox.min()[2]);
	IndexMax = FIntVector(IndexBBox.max()[0], IndexBBox.max()[1], IndexBBox.max()[2]);
	Size = IndexMax - IndexMin;

	MemoryUsage = NanoGridHandle.size();
	NumberActiveVoxels = MetaData->activeVoxelCount();

	if (NumberActiveVoxels == 0)
	{
		// Special to handle empty volumes. Create arbitrary smallest volume.
		Bounds = FBox(FVector(0.0, 0.0, 0.0), FVector(1.0, 1.0, 1.0));
		IndexMin = FIntVector(0, 0, 0);
		IndexMax = FIntVector(1, 1, 1);
		Size = IndexMax - IndexMin;
	}
}
#endif

bool FVolumeRenderInfos::HasNanoGridData() const
{
	return NanoGridHandle.gridMetaData() && NanoGridHandle.gridMetaData()->isValid();
}

void FVolumeRenderInfos::Update(const FMatrix44f& InIndexToLocal, const FIntVector& InIndexMin, const FIntVector& InIndexMax, const TRefCountPtr<FVdbRenderBuffer>& InRenderResource)
{
	IndexToLocal = InIndexToLocal;
	IndexMin = FVector3f(InIndexMin);
	IndexSize = FVector3f(InIndexMax - InIndexMin);
	RenderResource = InRenderResource;
}

void FVolumeRenderInfos::ReleaseResources(bool ClearGrid)
{ 
	RenderResource.SafeRelease(); 
	
	if (ClearGrid)
	{
		NanoGridHandle.buffer().clear();
	}
}

bool FVolumeRenderInfos::IsVectorGrid() const
{
	return NanoGridHandle.gridType() == nanovdb::GridType::Vec3f ||
		NanoGridHandle.gridType() == nanovdb::GridType::Vec4f;
}
