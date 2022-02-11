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

#include "VdbVolume.h"
#include "VdbCustomVersion.h"
#include "VdbCommon.h"
#include "VdbComponentBase.h"
#include "Rendering/VdbRenderBuffer.h"

#include "RenderingThread.h"
#include "EditorFramework\AssetImportData.h"
#include "UnrealClient.h"

UVdbVolume::UVdbVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// Mandatory so that we can use TUniquePtr with forward declarations in the header file
UVdbVolume::UVdbVolume() = default;
UVdbVolume::~UVdbVolume() = default;
UVdbVolume::UVdbVolume(FVTableHelper & Helper) {}

#if WITH_EDITOR
void UVdbVolume::Import(nanovdb::GridHandle<>&& GridHandle, EQuantizationType Quan)
{
	VolumeFrameInfos.UpdateFrame(GridHandle);

	Bounds = VolumeFrameInfos.GetBounds();
	MemoryUsage = VolumeFrameInfos.GetMemoryUsage();
	Quantization = Quan;

	VolumeRenderInfos.GetNanoGridHandle() = MoveTemp(GridHandle);

	const nanovdb::GridMetaData* MetaData = VolumeRenderInfos.GetNanoGridHandle().gridMetaData();
	UpdateFromMetadata(MetaData);
	
	PrepareRendering();
}

void UVdbVolume::PrepareRendering()
{
	// Create & init renderer resource
	InitResources();

	// Mark referencers as dirty so that the display actually refreshes with this new data
	MarkRenderStateDirtyForAllVdbComponents(this);
}

#endif

bool UVdbVolume::IsValid() const
{
	return VolumeRenderInfos.HasNanoGridData();
}

void UVdbVolume::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FVdbCustomVersion::GUID);

	Super::Serialize(Ar);

	nanovdb::GridHandle<nanovdb::HostBuffer>& NanoGridHandle = VolumeRenderInfos.GetNanoGridHandle();
	Ar << NanoGridHandle;
}

void UVdbVolume::PostLoad()
{
	Super::PostLoad();

	const int32 Version = GetLinkerCustomVersion(FVdbCustomVersion::GUID);

	InitResources();
}

void UVdbVolume::BeginDestroy()
{
	Super::BeginDestroy();

	ReleaseResources();
}

void UVdbVolume::InitResources()
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

void UVdbVolume::ReleaseResources()
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
