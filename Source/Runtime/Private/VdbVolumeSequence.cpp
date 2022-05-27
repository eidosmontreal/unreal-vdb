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

#include "VdbVolumeSequence.h"
#include "VdbCustomVersion.h"
#include "VdbCommon.h"
#include "VdbSequenceComponent.h"
#include "VolumeStreamingManager.h"
#include "Rendering/VdbRenderBuffer.h"
#include "Rendering/VdbRenderBufferPool.h"

#include "RenderingThread.h"
#include "EditorFramework/AssetImportData.h"
#include "Interfaces/ITargetPlatform.h"
#include "UObject/MetaData.h"

void FVdbSequenceChunk::Serialize(FArchive& Ar, UObject* Owner, int32 ChunkIndex)
{
	Ar.UsingCustomVersion(FVdbCustomVersion::GUID);

	// We force it not inline that means bulk data won't automatically be loaded when we deserialize
	// later but only when we explicitly take action to load it
	BulkData.SetBulkDataFlags(BULKDATA_Force_NOT_InlinePayload);
	BulkData.Serialize(Ar, Owner, ChunkIndex, false);
	Ar << DataSize;
	Ar << FirstFrame;
	Ar << LastFrame;
}

void FVdbSequenceChunk::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	// TODO: consider loaded bulk
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(DataSize));
}

UVdbVolumeSequence::UVdbVolumeSequence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IsVolSequence = true;
}

// Mandatory so that we can use TUniquePtr with forward declarations in the header file
UVdbVolumeSequence::UVdbVolumeSequence() = default;
UVdbVolumeSequence::UVdbVolumeSequence(FVTableHelper& Helper) {}
UVdbVolumeSequence::~UVdbVolumeSequence() = default;

float UVdbVolumeSequence::GetFrameIndexFloatFromTime(float InputAnimTime) const
{
	const float AnimationTime = FMath::Min(InputAnimTime, GetDurationInSeconds());
	const float FrameIndexFloat = AnimationTime / GetTimeBetweenFramesInSeconds();
	return FrameIndexFloat;
}

uint32 UVdbVolumeSequence::GetFrameIndexFromTime(float InputAnimTime) const
{
	const float FrameIndexFloat = GetFrameIndexFloatFromTime(InputAnimTime);
	const uint32 FrameIndexU32 = uint32(floorf(FrameIndexFloat));
	return FrameIndexU32;
}

void UVdbVolumeSequence::UVdbVolumeSequence::ValidateFrameToChunkRatio() const
{
	check(VolumeFramesInfos.Num() == VolumeRenderInfos.Num());
	check(VolumeFramesInfos.Num() == Chunks.Num());
}

uint32 UVdbVolumeSequence::GetChunkIndexFromFrameIndex(uint32 FrameIndex) const
{
	ValidateFrameToChunkRatio();
	return FrameIndex;
}

bool UVdbVolumeSequence::IsValid() const
{
	ValidateFrameToChunkRatio();
	return (VolumeFramesInfos.Num() > 0);
}

void UVdbVolumeSequence::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FVdbCustomVersion::GUID);
	
	Super::Serialize(Ar);

	// Streamed data
	int32 NumChunks = Chunks.Num();
	Ar << NumChunks;

	if (Ar.CustomVer(FVdbCustomVersion::GUID) < FVdbCustomVersion::LargestVolume)
	{
		FIntVector MaxVolumeSize = FIntVector::ZeroValue;
		for (auto& Info : VolumeFramesInfos)
		{
			MaxVolumeSize.X = FMath::Max(MaxVolumeSize.X, Info.GetSize().X);
			MaxVolumeSize.Y = FMath::Max(MaxVolumeSize.Y, Info.GetSize().Y);
			MaxVolumeSize.Z = FMath::Max(MaxVolumeSize.Z, Info.GetSize().Z);
		}
		LargestVolume = MaxVolumeSize;
	}

	if (Ar.IsLoading())
	{
		Chunks.SetNum(NumChunks);
	}

	for (int32 ChunkId = 0; ChunkId < NumChunks; ChunkId++)
	{
		Chunks[ChunkId].Serialize(Ar, this, ChunkId);
	}
}

void UVdbVolumeSequence::BeginDestroy()
{
	Super::BeginDestroy();

	ReleaseResources();
}

void UVdbVolumeSequence::PostLoad()
{
	Super::PostLoad();

	InitResources();

	IVolumeStreamingManager::Get().AddVolume(this);
}

void UVdbVolumeSequence::InitResources()
{
	int32 NbFramesInSequence = VolumeFramesInfos.Num();

	if (NbFramesInSequence > 0)
	{
		if (!BufferPool || BufferPool->GetBufferSize() != FrameMaxMemoryUsage)
		{
			BufferPool = MakeUnique<FVdbRenderBufferPool>(FrameMaxMemoryUsage);
		}

		if (VolumeRenderInfos.IsEmpty())
		{
			VolumeRenderInfos.AddDefaulted(NbFramesInSequence);
		}

		for (int32 FrameIndex = 0; FrameIndex < NbFramesInSequence; FrameIndex++)
		{
			FVolumeFrameInfos& VolInfos = VolumeFramesInfos[FrameIndex];
			FVolumeRenderInfos& VolRenderInfos = VolumeRenderInfos[FrameIndex];
			VolRenderInfos.Update(VolInfos.GetIndexToLocal(), VolInfos.GetIndexMin(), VolInfos.GetIndexMax(), nullptr);
		}
	}
	else
	{
		VolumeRenderInfos.Empty();
	}
}

void UVdbVolumeSequence::ReleaseResources()
{
	for (FVolumeRenderInfos& RenderInfo : VolumeRenderInfos)
	{
		RenderInfo.ReleaseResources(true);
	}

	if (BufferPool) 
	{
		BufferPool->Release();
	}

	VolumeRenderInfos.Empty();
}

void UVdbVolumeSequence::PostInitProperties()
{
	Super::PostInitProperties();

	IVolumeStreamingManager::Get().AddVolume(this);
}

FString UVdbVolumeSequence::GetDesc()
{
	ValidateFrameToChunkRatio();
	return FString::Printf(TEXT("%d Chunks - %d Frames"), Chunks.Num(), GetNbFrames());
}

void UVdbVolumeSequence::FinishDestroy()
{
	for (int32 ChunkIndex : ChunksWithPendingUpload)
	{
		IVolumeStreamingManager::Get().UnmapChunk(this, ChunkIndex);
	}

	IVolumeStreamingManager::Get().RemoveVolume(this);

	Super::FinishDestroy();
}

uint32 UVdbVolumeSequence::GetChunkDataSize(uint32 ChunkId) const
{
	return Chunks[ChunkId].DataSize;
}

bool UVdbVolumeSequence::IsDataAlreadyLoaded(uint32 ChunkId) const
{
	const FVdbSequenceChunk& Chunk = Chunks[ChunkId];
	return Chunk.BulkData.IsBulkDataLoaded();
}

void UVdbVolumeSequence::PrefetchChunkSync(uint32 ChunkId, void* ResidentChunkMemory)
{
	FVdbSequenceChunk& Chunk = Chunks[ChunkId];
	check(Chunk.BulkData.GetBulkDataSize() > 0);
	check(Chunk.BulkData.GetBulkDataSize() == Chunk.DataSize);
	Chunk.BulkData.GetCopy((void**)&ResidentChunkMemory, true); //note: This does the actual loading internally...
}

void UVdbVolumeSequence::OnChunkEvicting(uint32 ChunkId)
{
}

void UVdbVolumeSequence::OnChunkEvicted(uint32 ChunkId)
{
	const uint32 FrameIdx = (uint32)ChunkId;		// SAMI - won't work if we don't have a 1-to-1 frame to chunk ratio
	VolumeRenderInfos[FrameIdx].ReleaseResources(true);
}

void UVdbVolumeSequence::UpdateChunksNeeded(TArray<int32>& ChunksNeeded)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_UVdbVolumeSequence_UpdateChunksNeeded);

	ChunksWithPendingUpload.RemoveAll([this, &ChunksNeeded](const int32& ChunkId)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_UVdbVolumeSequence_UpdateChunksNeeded_Remove);

			const uint32 FrameIdx = (uint32)ChunkId;		// SAMI - won't work if we don't have a 1-to-1 frame to chunk ratio
			const FVolumeRenderInfos& VolRenderInfos = this->VolumeRenderInfos[FrameIdx];

			// Not ready
			if (!VolRenderInfos.GetRenderResource()->IsUploadFinished())
			{
				ChunksNeeded.AddUnique(ChunkId);
				return false;
			}

			{
				FScopeLock ScopeLock(&ComponentsLock);
				for (UVdbSequenceComponent* SeqComponent : VdbSequenceComponents)
				{
					SeqComponent->OnChunkAvailable(ChunkId);
				}
			}

			// Ready
			IVolumeStreamingManager::Get().UnmapChunk(this, ChunkId);
			return true;
		});

	if (BufferPool)
	{
		BufferPool->TickPoolElements();
	}
}

void UVdbVolumeSequence::OnChunkAvailable(uint32 ChunkId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_UVdbVolumeSequence_OnChunkAvailable);

	const uint32 FrameIdx = (uint32)ChunkId;		// SAMI - won't work if we don't have a 1-to-1 frame to chunk ratio
	FVdbSequenceChunk& Chunk = Chunks[ChunkId];

	uint32 ChunkSize = 0;
	const uint8* ChunkMemory = IVolumeStreamingManager::Get().MapChunk(this, ChunkId, false, &ChunkSize);
	if (ChunkMemory == nullptr)
	{
		// Data not in memory
		return;
	}

	FBufferReader Ar((void*)ChunkMemory, ChunkSize, /*bInFreeOnClose=*/ false, /*bIsPersistent=*/ true);	
	FVolumeRenderInfos& VolRenderInfos = VolumeRenderInfos[FrameIdx];
	nanovdb::GridHandle<nanovdb::HostBuffer>& GridHandle = VolRenderInfos.GetNanoGridHandle();
	Ar << GridHandle;

	ChunksWithPendingUpload.AddUnique(ChunkId);
	FVolumeFrameInfos& VolInfos = VolumeFramesInfos[FrameIdx];

	TRefCountPtr<FVdbRenderBuffer> PooledBuffer = BufferPool->GetBuffer();
	PooledBuffer->UploadData(VolInfos.GetMemoryUsage(), VolRenderInfos.GetNanoGridHandle().data());
	VolRenderInfos.Update(VolInfos.GetIndexToLocal(), VolInfos.GetIndexMin(), VolInfos.GetIndexMax(), PooledBuffer);
}

void UVdbVolumeSequence::CopyChunkContentToMemory(uint32 ChunkId, void* ResidentChunkMemory)
{
	FVdbSequenceChunk& Chunk = Chunks[ChunkId];
	check(Chunk.BulkData.GetBulkDataSize() == Chunk.DataSize);

	const void* BulkDataPtr = Chunk.BulkData.LockReadOnly();
	FMemory::Memcpy(ResidentChunkMemory, BulkDataPtr, Chunk.DataSize);
	Chunk.BulkData.Unlock();
}

IBulkDataIORequest* UVdbVolumeSequence::CreateStreamingRequest(uint32 ChunkId, FBulkDataIORequestCallBack& AsyncFileCallBack)
{
	FVdbSequenceChunk& Chunk = Chunks[ChunkId];

	checkf(Chunk.BulkData.CanLoadFromDisk(), TEXT("Bulk data is not loaded and cannot be loaded from disk!"));
	check(!Chunk.BulkData.IsStoredCompressedOnDisk()); // We do not support compressed Bulkdata for this system
	check(Chunk.BulkData.GetBulkDataSize() == Chunk.DataSize);

	return Chunk.BulkData.CreateStreamingRequest(AIOP_BelowNormal, &AsyncFileCallBack, nullptr);
}

bool UVdbVolumeSequence::IsGridDataInMemory(uint32 FrameIndex, bool CheckIsAlsoUploadedToGPU) const
{
	const FVolumeRenderInfos& RenderInfos = VolumeRenderInfos[FrameIndex];
	const nanovdb::GridHandle<nanovdb::HostBuffer>& GridHandle = RenderInfos.GetNanoGridHandle();
	if (GridHandle.buffer().empty())
	{
		return false;
	}

	return !CheckIsAlsoUploadedToGPU || ((RenderInfos.GetRenderResource() != nullptr) && RenderInfos.GetRenderResource()->IsUploadFinished());
}

const FBox& UVdbVolumeSequence::GetBounds(uint32 FrameIndex) const
{
	return VolumeFramesInfos[FrameIndex].GetBounds();
}

const FIntVector& UVdbVolumeSequence::GetIndexMin(uint32 FrameIndex) const
{
	return VolumeFramesInfos[FrameIndex].GetIndexMin();
}

const FIntVector& UVdbVolumeSequence::GetIndexMax(uint32 FrameIndex) const
{ 
	return VolumeFramesInfos[FrameIndex].GetIndexMax();

}

const FMatrix44f& UVdbVolumeSequence::GetIndexToLocal(uint32 FrameIndex) const
{ 
	return VolumeFramesInfos[FrameIndex].GetIndexToLocal();
}

void UVdbVolumeSequence::RegisterComponent(UVdbSequenceComponent* VdbSeqComponent)
{
	FScopeLock ScopeLock(&ComponentsLock);
	VdbSequenceComponents.Add(VdbSeqComponent);
}

void UVdbVolumeSequence::UnregisterComponent(UVdbSequenceComponent* VdbSeqComponent)
{
	FScopeLock ScopeLock(&ComponentsLock);
	VdbSequenceComponents.Remove(VdbSeqComponent);
}

#if WITH_EDITOR
void UVdbVolumeSequence::PrepareRendering()
{
	// Create & init renderer resource
	InitResources();
}

void UVdbVolumeSequence::AddFrame(nanovdb::GridHandle<>& NanoGridHandle, EQuantizationType InQuantization)
{
	FVolumeFrameInfos* VolumeInfosEntry = new(VolumeFramesInfos) FVolumeFrameInfos();
	VolumeInfosEntry->UpdateFrame(NanoGridHandle);

	const nanovdb::GridMetaData* MetaData = NanoGridHandle.gridMetaData();
	if (VolumeFramesInfos.Num() == 1)
	{
		// First entry, first frame
		UpdateFromMetadata(MetaData);

		Bounds = VolumeInfosEntry->GetBounds();
		LargestVolume = VolumeInfosEntry->GetIndexMax() - VolumeInfosEntry->GetIndexMin();
		MemoryUsage = VolumeInfosEntry->GetMemoryUsage();
		Quantization = InQuantization;
		FrameMaxMemoryUsage = MemoryUsage;
	}
	else
	{
		check(DataType == FString(nanovdb::toStr(MetaData->gridType())));
		check(Class == FString(nanovdb::toStr(MetaData->gridClass())));
		check(GridName == MetaData->shortGridName());

		const FIntVector& IndexVolume = VolumeInfosEntry->GetIndexMax() - VolumeInfosEntry->GetIndexMin();
		LargestVolume.X = FMath::Max(LargestVolume.X, IndexVolume.X);
		LargestVolume.Y = FMath::Max(LargestVolume.Y, IndexVolume.Y);
		LargestVolume.Z = FMath::Max(LargestVolume.Z, IndexVolume.Z);

		Bounds += VolumeInfosEntry->GetBounds();
		uint64 MemUsage = VolumeInfosEntry->GetMemoryUsage();
		MemoryUsage += MemUsage;
		FrameMaxMemoryUsage = FMath::Max(MemUsage, FrameMaxMemoryUsage);
	}
}

void UVdbVolumeSequence::FinalizeImport(const FString& Filename)
{
	AssetImportData->Update(Filename);
	MemoryUsageStr = *GetMemoryString(MemoryUsage, false);

	PrepareRendering();
}
#endif
