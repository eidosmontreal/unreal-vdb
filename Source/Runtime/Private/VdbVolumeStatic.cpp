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

#include "VdbVolumeStatic.h"
#include "VdbCustomVersion.h"
#include "VdbCommon.h"
#include "Rendering/VdbRenderBuffer.h"

#include "RenderingThread.h"
#include "EditorFramework\AssetImportData.h"
#include "UnrealClient.h"

UVdbVolumeStatic::UVdbVolumeStatic(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// Mandatory so that we can use TUniquePtr with forward declarations in the header file
UVdbVolumeStatic::UVdbVolumeStatic() = default;
UVdbVolumeStatic::~UVdbVolumeStatic() = default;
UVdbVolumeStatic::UVdbVolumeStatic(FVTableHelper & Helper) {}

#if WITH_EDITOR
void UVdbVolumeStatic::Import(nanovdb::GridHandle<>&& GridHandle, EQuantizationType Quan)
{
	VolumeFrameInfos.UpdateFrame(GridHandle);

	Bounds = VolumeFrameInfos.GetBounds();
	LargestVolume = VolumeFrameInfos.GetSize();
	MemoryUsage = VolumeFrameInfos.GetMemoryUsage();
	Quantization = Quan;

	VolumeRenderInfos.GetNanoGridHandle() = MoveTemp(GridHandle);

	const nanovdb::GridMetaData* MetaData = VolumeRenderInfos.GetNanoGridHandle().gridMetaData();
	UpdateFromMetadata(MetaData);
	
	PrepareRendering();
}

void UVdbVolumeStatic::PrepareRendering()
{
	// Create & init renderer resource
	InitResources();
}

#endif

bool UVdbVolumeStatic::IsValid() const
{
	return VolumeRenderInfos.HasNanoGridData();
}

void UVdbVolumeStatic::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FVdbCustomVersion::GUID);

	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		if (Ar.CustomVer(FVdbCustomVersion::GUID) < FVdbCustomVersion::LargestVolume)
		{
			LargestVolume = VolumeFrameInfos.GetSize();
		}
	}

	nanovdb::GridHandle<nanovdb::HostBuffer>& NanoGridHandle = VolumeRenderInfos.GetNanoGridHandle();
	Ar << NanoGridHandle;
}

void UVdbVolumeStatic::PostLoad()
{
	Super::PostLoad();

	const int32 Version = GetLinkerCustomVersion(FVdbCustomVersion::GUID);

	InitResources();
}

void UVdbVolumeStatic::BeginDestroy()
{
	Super::BeginDestroy();

	ReleaseResources();
}

void UVdbVolumeStatic::InitResources()
{
	if (RenderResource == nullptr)
	{
		RenderResource = new FVdbRenderBuffer();
	}

	if (VolumeRenderInfos.HasNanoGridData())
	{
		RenderResource->SetData(MemoryUsage, VolumeRenderInfos.GetNanoGridHandle().data());
		BeginInitResource(RenderResource); // initialize
	}
	else
	{
		RenderResource->SetData(0, nullptr);
	}

	VolumeRenderInfos.Update(GetIndexToLocal(), GetIndexMin(), GetIndexMax(), RenderResource);
}

void UVdbVolumeStatic::ReleaseResources()
{
	if (RenderResource && RenderResource->IsInitialized())
	{
		if (GIsEditor && !GIsPlayInEditorWorld)
		{
			// Flush the rendering command to be sure there is no command left that can create/modify a rendering resource
			FlushRenderingCommands();
		}

		BeginReleaseResource(RenderResource);

		if (GIsEditor && !GIsPlayInEditorWorld)
		{
			// In case of reimport, this UObject gets deleted before resource is released. Force an extra flush.
			FlushRenderingCommands();
		}
	}
}
