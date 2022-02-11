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
#include "HAL/CriticalSection.h"
#include "Containers/Array.h"
#include "Containers/Queue.h"

class IAsyncReadRequest;
class IBulkDataIORequest;
struct FVolumeChunk;
class IInterface_StreamableVolumetricAsset;
class IInterface_StreamableVolumetricAssetOwner;

/**
An actual chunk resident in memory.
*/
struct FResidentChunk
{
	void* Memory;
	int32 DataSize;
	int32 Refcount;
	IBulkDataIORequest* IORequest; //null when resident, nonnull when being loaded
};

/**
	The results of a completed async-io request.
*/
struct FCompletedChunk
{
	IBulkDataIORequest* ReadRequest;
	int32 LoadedChunkIndex;

	FCompletedChunk() : ReadRequest(nullptr), LoadedChunkIndex(0) {}
	FCompletedChunk(int32 SetLoadedChunkIndex, IBulkDataIORequest* SetReadRequest) : ReadRequest(SetReadRequest), LoadedChunkIndex(SetLoadedChunkIndex) {}
};

/**
For every UVolumeAsset, one of these is created for by the streaming manager.
*/
class FStreamingVolumeData
{
public:
	FStreamingVolumeData(IInterface_StreamableVolumetricAsset* InputVolumeAsset);
	~FStreamingVolumeData();

	void UpdateStreamingStatus();

	const uint8* MapChunk(uint32 ChunkIndex, bool ChunkHasToBeStreamed, uint32* OutChunkSize);
	void UnmapChunk(uint32 ChunkIndex);

	bool IsStreamingInProgress();
	bool BlockTillAllRequestsFinished(float TimeLimit = 0.0f);
	void PrefetchData(IInterface_StreamableVolumetricAssetOwner* AssetOwner);

	void ResetNeededChunks();
	void AddNeededChunk(uint32 ChunkIndex);

private:

	FResidentChunk & AddResidentChunk(int32 ChunkId, uint32 ChunkDataSize);
	void RemoveResidentChunk(FResidentChunk& LoadedChunk);
	void OnAsyncReadComplete(int32 LoadedChunkIndex, IBulkDataIORequest* ReadRequest);
	void ProcessCompletedChunks();

	// The asset we are associated with
	IInterface_StreamableVolumetricAsset* VolumeAsset;

	// Chunks that ideally would be loaded at this point in time
	// There may be more or less actual chunks loaded (more = cached chunks, less = we're still waiting for the disc)
	// This should only be used from the main thread. It can be modified without taking the lock. Changes are then
	// "latched" to other data structures/threads in the UpdateStreamingStatus function.
	TArray<int32> ChunksNeeded;

	// List of chunks currently resident in memory
	TArray<int32> ChunksAvailable;

	// This this does not necessary contains only chunks in the ChunksAvailable lists
	// for example chunks in the ChunksRequested will also be in here.
	TMap<int32, FResidentChunk> Chunks;

	// Chunks requested to be streamed in but not available yet
	TArray<int32> ChunksRequested;

	// Chunks to be evicted. Chunks may linger here for a while
	// until they are fully unpinned
	TArray<int32> ChunksEvicted;

	TArray<IAsyncReadRequest*> IoRequestsToCompleteAndDeleteOnGameThread;

	// Chunks that have finished loading but have not finished their post-load bookkeping
	// they are still not part of the ChunksAvailable list.
	TQueue<FCompletedChunk, EQueueMode::Mpsc> CompletedChunks;

	mutable FCriticalSection CriticalSection;
};

