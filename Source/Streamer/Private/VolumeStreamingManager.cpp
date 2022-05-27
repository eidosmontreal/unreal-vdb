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

#include "VolumeStreamingManager.h"

#include "Async/Async.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/ScopeLock.h"
#include "Stats/Stats.h"

#include "StreamingVolumeData.h"

DEFINE_LOG_CATEGORY(LogVolumeStreaming);


struct FVolumeStreamingManager : public IVolumeStreamingManager
{
	/** Constructor, initializing all members */
	FVolumeStreamingManager();

	virtual ~FVolumeStreamingManager();

	// IStreamingManager interface
	virtual void UpdateResourceStreaming(float DeltaTime, bool bProcessEverything = false) override;
	virtual int32 BlockTillAllRequestsFinished(float TimeLimit = 0.0f, bool bLogResults = false) override;
	virtual void CancelForcedResources() override;
	virtual void NotifyLevelChange() override;
	virtual void SetDisregardWorldResourcesForFrames(int32 NumFrames) override;
	virtual void AddLevel(class ULevel* Level) override;
	virtual void RemoveLevel(class ULevel* Level) override;
	virtual void NotifyLevelOffset(class ULevel* Level, const FVector& Offset) override;

	// End IStreamingManager interface

	// IVolumeStreamingManager interface
	void AddVolume(IInterface_StreamableVolumetricAsset* Volume);
	void RemoveVolume(IInterface_StreamableVolumetricAsset* Volume);
	bool IsManagedVolume(const IInterface_StreamableVolumetricAsset* Volume) const;
	bool IsStreamingInProgress(const IInterface_StreamableVolumetricAsset* Volume);
	void AddStreamingComponent(IInterface_StreamableVolumetricAssetOwner* AssetOwner);
	void RemoveStreamingComponent(IInterface_StreamableVolumetricAssetOwner* AssetOwner);
	void PrefetchData(IInterface_StreamableVolumetricAssetOwner* AssetOwner);
	bool IsManagedComponent(const IInterface_StreamableVolumetricAssetOwner* AssetOwner) const;
	const uint8* MapChunk(const IInterface_StreamableVolumetricAsset* Volume, uint32 ChunkIndex, bool AllowChunkToBeUnloaded, uint32* OutChunkSize = nullptr);
	void UnmapChunk(const IInterface_StreamableVolumetricAsset* Volume, uint32 ChunkIndex);
	// End IVolumeStreamingManager interface

private:

	void PrefetchDataInternal(IInterface_StreamableVolumetricAssetOwner* AssetOwner);

	/** Animations being managed. */
	TMap<IInterface_StreamableVolumetricAsset*, FStreamingVolumeData*> StreamingVolumes;

	/** Scene components currently running streaming. */
	TArray<IInterface_StreamableVolumetricAssetOwner*>	StreamingComponents;

	mutable FCriticalSection CriticalSection;

	double LastTickTime;	
};


static FVolumeStreamingManager* VolumeStreamingManager = nullptr;

IVolumeStreamingManager &IVolumeStreamingManager::Get()
{
	if (VolumeStreamingManager == nullptr)
	{
		VolumeStreamingManager = new FVolumeStreamingManager();
	}
	return *VolumeStreamingManager;
}

FVolumeStreamingManager::FVolumeStreamingManager()
{
	IStreamingManager::Get().AddStreamingManager(this);
}

FVolumeStreamingManager::~FVolumeStreamingManager()
{
	IStreamingManager::Get().RemoveStreamingManager(this);
}

void FVolumeStreamingManager::UpdateResourceStreaming(float DeltaTime, bool bProcessEverything)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_FVolumeStreamingManager_UpdateResourceStreaming);

	check(IsInGameThread());

	//Phase zero: Clear ChunksNeeded
	for (auto Iter = StreamingVolumes.CreateIterator(); Iter; ++Iter)
	{
		FStreamingVolumeData*& StreamingAnimData = Iter.Value();
		StreamingAnimData->ResetNeededChunks();
	}

	// First phase: gather all the chunks that need to be streamed from all playing instances
	TArray<uint32> ChunksNeeded;

	for (IInterface_StreamableVolumetricAssetOwner* Component : StreamingComponents)
	{
		for (IInterface_StreamableVolumetricAsset* Volume : Component->GetStreamableAssets())
		{
			if (Volume)
			{
				FStreamingVolumeData** DataPtr = StreamingVolumes.Find(Volume);
				if (DataPtr)
				{
					ChunksNeeded.Reset();

					// Get needed frame indices from component. Convert needed frame indices into needed chunk indices
					//		FramesNeededByComponent.Reset();
					//		Component->GetNeededFrames(FramesNeededByComponent);
					//		Volume->GetNeededChunks(FramesNeededByComponent, ChunksNeeded);
					Component->UpdateIndicesOfChunksToStream(ChunksNeeded);

					for (uint32& ChunkIndex : ChunksNeeded)
					{
						(*DataPtr)->AddNeededChunk(ChunkIndex);
					}
				}
			}
		}
	}

	double ThisTickTime = FPlatformTime::Seconds();
	double Delta = ThisTickTime - LastTickTime;
	unsigned BandWidthSinceLastTick = IoBandwidth.Reset();
	LastTickTime = ThisTickTime;

	// Second phase: Schedule any new request we discovered, evict old, ...
	for (auto Iter = StreamingVolumes.CreateIterator(); Iter; ++Iter)
	{
		FStreamingVolumeData* StreamingAnimData = Iter.Value();
		StreamingAnimData->UpdateStreamingStatus();
	}
}

int32 FVolumeStreamingManager::BlockTillAllRequestsFinished(float TimeLimit, bool bLogResults)
{
	int32 Result = 0;

	if (TimeLimit == 0.0f)
	{
		for (auto Iter = StreamingVolumes.CreateIterator(); Iter; ++Iter)
		{
			Iter.Value()->BlockTillAllRequestsFinished();
		}
	}
	else
	{
		double EndTime = FPlatformTime::Seconds() + TimeLimit;
		for (auto Iter = StreamingVolumes.CreateIterator(); Iter; ++Iter)
		{
			float ThisTimeLimit = EndTime - FPlatformTime::Seconds();
			if (ThisTimeLimit < .001f || // one ms is the granularity of the platform event system
				!Iter.Value()->BlockTillAllRequestsFinished(ThisTimeLimit))
			{
				Result = 1; // we don't report the actual number, just 1 for any number of outstanding requests
				break;
			}
		}
	}

	return Result;
}

void FVolumeStreamingManager::CancelForcedResources()
{
}

void FVolumeStreamingManager::NotifyLevelChange()
{
}

void FVolumeStreamingManager::SetDisregardWorldResourcesForFrames(int32 NumFrames)
{
}

void FVolumeStreamingManager::AddLevel(class ULevel* Level)
{
	check(IsInGameThread());
}

void FVolumeStreamingManager::RemoveLevel(class ULevel* Level)
{
	check(IsInGameThread());
}

void FVolumeStreamingManager::NotifyLevelOffset(class ULevel* Level, const FVector& Offset)
{
	check(IsInGameThread());
}

void FVolumeStreamingManager::AddVolume(IInterface_StreamableVolumetricAsset* Volume)
{
	check(IsInGameThread() || IsInAsyncLoadingThread());
	FStreamingVolumeData* & StreamingAnimData = StreamingVolumes.FindOrAdd(Volume);
	if (StreamingAnimData == nullptr)
	{
		StreamingAnimData = new FStreamingVolumeData(Volume);
	}
}

void FVolumeStreamingManager::RemoveVolume(IInterface_StreamableVolumetricAsset* Volume)
{
	check(IsInGameThread());
	FStreamingVolumeData** StreamingAnimData = StreamingVolumes.Find(Volume);
	if (StreamingAnimData != nullptr)
	{
		check(*StreamingAnimData != nullptr);
		delete (*StreamingAnimData);
		StreamingVolumes.Remove(Volume);
	}
}

bool FVolumeStreamingManager::IsManagedVolume(const IInterface_StreamableVolumetricAsset* Volume) const
{
	check(IsInGameThread());
	return StreamingVolumes.Contains(Volume);
}

bool FVolumeStreamingManager::IsStreamingInProgress(const IInterface_StreamableVolumetricAsset* Volume)
{
	check(IsInGameThread());
	FStreamingVolumeData** StreamingAnimData = StreamingVolumes.Find(Volume);
	if (StreamingAnimData != nullptr)
	{
		check(*StreamingAnimData != nullptr);
		return (*StreamingAnimData)->IsStreamingInProgress();
	}
	return false;
}

void FVolumeStreamingManager::AddStreamingComponent(IInterface_StreamableVolumetricAssetOwner* AssetOwner)
{
	check(IsInGameThread());
	StreamingComponents.AddUnique(AssetOwner);

	PrefetchData(AssetOwner);
}

void FVolumeStreamingManager::RemoveStreamingComponent(IInterface_StreamableVolumetricAssetOwner* AssetOwner)
{
	check(IsInGameThread());
	StreamingComponents.Remove(AssetOwner);
}

void FVolumeStreamingManager::PrefetchDataInternal(IInterface_StreamableVolumetricAssetOwner* AssetOwner)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_FVolumeStreamingManager_PrefetchDataInternal);

	check(IsInGameThread());
	check(IsManagedComponent(AssetOwner));
	for (auto Volume : AssetOwner->GetStreamableAssets())
	{
		if (Volume)
		{
			FStreamingVolumeData** data = StreamingVolumes.Find(Volume);
			checkf(data != nullptr, TEXT("No data could be prefetched for an animation because it was not registered with the manager."));
			(*data)->PrefetchData(AssetOwner);
		}
	}
}

void FVolumeStreamingManager::PrefetchData(IInterface_StreamableVolumetricAssetOwner* AssetOwner)
{
	if (IsInGameThread())
	{
		PrefetchDataInternal(AssetOwner);
	}
	else
	{
		// The prefetch doesn't need to be executed right now, so schedule it for the game thread
		FWeakObjectPtr WeakObjPtr(AssetOwner->GetAssociatedUObject());
		AsyncTask(ENamedThreads::GameThread, [this, WeakObjPtr, AssetOwner]()
		{
			if (WeakObjPtr.Get())
			{
				PrefetchDataInternal(AssetOwner);
			}
		});
	}
}

bool FVolumeStreamingManager::IsManagedComponent(const IInterface_StreamableVolumetricAssetOwner* AssetOwner) const
{
	check(IsInGameThread());
	return StreamingComponents.Contains(AssetOwner);
}

const uint8* FVolumeStreamingManager::MapChunk(const IInterface_StreamableVolumetricAsset* Volume, uint32 ChunkIndex, bool ChunkHasToBeStreamed, uint32* OutChunkSize)
{
	FStreamingVolumeData** data = StreamingVolumes.Find(Volume);
	if (data)
	{
		return (*data)->MapChunk(ChunkIndex, ChunkHasToBeStreamed, OutChunkSize);
	}

	UE_LOG(LogVolumeStreaming, Error, TEXT("Tried to map a chunk in an unregistered volumetric animation"));
	return nullptr;
}

void FVolumeStreamingManager::UnmapChunk(const IInterface_StreamableVolumetricAsset* Volume, uint32 ChunkIndex)
{
	FStreamingVolumeData** data = StreamingVolumes.Find(Volume);
	if (data)
	{
		(*data)->UnmapChunk(ChunkIndex);
	}
}

void AddIndicesOfChunksToStream(TArray<uint32>& IndicesOfChunksToStream, uint32 NbFramesInAnim, uint32 IndexFirstChunk, uint32 IndexLastChunk)
{
	if (IndexLastChunk < IndexFirstChunk)
	{
		// Add chunks in range [IndexFirstChunk, VolumeAsset->GetNbFrames() - 1]
		for (uint32 ChunkId = IndexFirstChunk; ChunkId < NbFramesInAnim; ++ChunkId)
		{
			IndicesOfChunksToStream.AddUnique(ChunkId);
		}

		// Add chunks in range [0, IndexLastChunk]
		for (uint32 ChunkId = 0; ChunkId <= IndexLastChunk; ++ChunkId)
		{
			IndicesOfChunksToStream.AddUnique(ChunkId);
		}
	}
	else
	{
		for (uint32 ChunkId = IndexFirstChunk; ChunkId <= IndexLastChunk; ++ChunkId)
		{
			IndicesOfChunksToStream.AddUnique(ChunkId);
		}
	}
}