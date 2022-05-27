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
#include "ContentStreaming.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVolumeStreaming, Verbose, All);

struct FVolumeChunk;
struct FVolumeFrameData;
class IInterface_StreamableVolumetricAsset;
class IInterface_StreamableVolumetricAssetOwner;

/**
* Contains a request to load chunks of a volumetric animation
*/
struct FVolumeLoadChunksRequest
{
	TArray<uint32>	RequiredIndices;
	bool			bPrioritiseRequest;
};

class IInterface_StreamableVolumetricAsset
{
public:
	virtual uint32 GetNbFrames() const = 0;
	virtual uint32 GetChunkDataSize(uint32 ChunkId) const = 0;
	virtual void UpdateChunksNeeded(TArray<int32>& ChunksNeeded) = 0;
	virtual bool IsDataAlreadyLoaded(uint32 ChunkId) const = 0;
	virtual void PrefetchChunkSync(uint32 ChunkId, void* ResidentChunkMemory) = 0;
	virtual void OnChunkEvicting(uint32 ChunkId) = 0;
	virtual void OnChunkEvicted(uint32 ChunkId) = 0;
	virtual void OnChunkAvailable(uint32 ChunkId) = 0;
	virtual void CopyChunkContentToMemory(uint32 ChunkId, void* ResidentChunkMemory) = 0;
	virtual IBulkDataIORequest* CreateStreamingRequest(uint32 ChunkId, FBulkDataIORequestCallBack& AsyncFileCallBack) = 0;
};

class IInterface_StreamableVolumetricAssetOwner
{
public:
	virtual void UpdateIndicesOfChunksToStream(TArray<uint32>& IndicesOfChunksToStream) = 0;
	virtual TArray<IInterface_StreamableVolumetricAsset*> GetStreamableAssets() = 0;
	virtual UObject* GetAssociatedUObject() = 0;
};

UENUM()
enum class EVolumePlayMode : uint8
{
	Stopped,
	Playing,
	Paused
};

/**
* Note IStreamingManager is not really anything like an interface it contains code and members and whatnot.
* So we just play along here to keep the spirit of the existing audio and texture streaming managers.
*/
VOLUMESTREAMER_API struct IVolumeStreamingManager : public IStreamingManager
{
	IVolumeStreamingManager() {}

	/** Virtual destructor */
	virtual ~IVolumeStreamingManager() {}

	/** Getter of the singleton */
	VOLUMESTREAMER_API static struct IVolumeStreamingManager& Get();

	/** Adds a new volumetric animation to the streaming manager. */
	virtual void AddVolume(IInterface_StreamableVolumetricAsset* Volume) = 0;

	/** Removes a volumetric animation from the streaming manager. */
	virtual void RemoveVolume(IInterface_StreamableVolumetricAsset* Volume) = 0;

	/** Returns true if this is a volumetric animation is managed by the streaming manager. */
	virtual bool IsManagedVolume(const IInterface_StreamableVolumetricAsset* Volume) const = 0;

	/** Returns true if this data for this volumetric animation is streaming. */
	virtual bool IsStreamingInProgress(const IInterface_StreamableVolumetricAsset* Volume) = 0;

	/** Adds a new component to the streaming manager. */
	virtual void AddStreamingComponent(IInterface_StreamableVolumetricAssetOwner* AssetOwner) = 0;

	/** Removes the component from the streaming manager. */
	virtual void RemoveStreamingComponent(IInterface_StreamableVolumetricAssetOwner* AssetOwner) = 0;

	/** Prefetch data for the current state component. Data is automatically prefetched when initially registering the component
	 * this may be useful when the component has seeked etc.*/
	virtual void PrefetchData(IInterface_StreamableVolumetricAssetOwner* AssetOwner) = 0;

	/** Returns true if this is a streaming animation component that is managed by the streaming manager. */
	virtual bool IsManagedComponent(const IInterface_StreamableVolumetricAssetOwner* AssetOwner) const = 0;

	/**
	* Gets a pointer to a chunk of cached data. Can be called from any thread.
	*
	* @param Track Animation track we want a chunk from
	* @param ChunkIndex	Index of the chunk we want
	* @param OutChunkSize If non-null tores the size in bytes of the chunk
	* @return Either the desired chunk or NULL if it's not loaded
	*/
	virtual const uint8* MapChunk(const IInterface_StreamableVolumetricAsset* Volume, uint32 ChunkIndex, bool ChunkHasToBeStreamed, uint32* OutChunkSize ) = 0;

	/**
	* Releases pointer to a chunk of cached data. Can be called from any thread.
	* Should be called for every call to MapChunk.
	*
	* @param Track Animation track we want a chunk from
	* @param ChunkIndex	Index of the chunk we want
	*/
	virtual void UnmapChunk(const IInterface_StreamableVolumetricAsset* Volume, uint32 ChunkIndex) = 0;

	FThreadSafeCounter IoBandwidth;
	mutable FCriticalSection CriticalSection;
};

VOLUMESTREAMER_API void AddIndicesOfChunksToStream(TArray<uint32>& IndicesOfChunksToStream, uint32 NbFramesInAnim, uint32 IndexFirstChunk, uint32 IndexLastChunk);
