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

#include "StreamingVolumeData.h"

#include "Misc/ScopeLock.h"

#include "VolumeStreamingManager.h"

FStreamingVolumeData::FStreamingVolumeData(IInterface_StreamableVolumetricAsset* InputVolumeAsset)
	: VolumeAsset(InputVolumeAsset)
{
}

FStreamingVolumeData::~FStreamingVolumeData()
{
	check(IsInGameThread());

	// Flush the render thread so any decoding still happening is finished and thus no maps held by the render thread.
	FlushRenderingCommands();

	// Wait for all outstanding requests to finish
	BlockTillAllRequestsFinished();
	check(ChunksRequested.Num() == 0);

	// Free data associated with all chunks
	for (auto Iter = Chunks.CreateIterator(); Iter; ++Iter)
	{
		RemoveResidentChunk(Iter.Value());
	}
	Chunks.Empty();
}

void FStreamingVolumeData::ResetNeededChunks()
{
	ChunksNeeded.Empty();

	if (VolumeAsset)
	{
		VolumeAsset->UpdateChunksNeeded(ChunksNeeded);
	}
}

void FStreamingVolumeData::AddNeededChunk(uint32 ChunkIndex)
{
	ChunksNeeded.AddUnique(ChunkIndex);
}

FResidentChunk& FStreamingVolumeData::AddResidentChunk(int32 ChunkId, uint32 ChunkDataSize)
{
	FResidentChunk& result = Chunks.Add(ChunkId);
	result.Refcount = 0;
	result.Memory = nullptr;
	result.DataSize = ChunkDataSize;
	result.IORequest = nullptr;
	return result;
}

void FStreamingVolumeData::RemoveResidentChunk(FResidentChunk& LoadedChunk)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_FStreamingVolumeData_RemoveResidentChunk);

	checkf(LoadedChunk.Refcount == 0, TEXT("Tried to remove a chunk wich was still mapped. Make sure there is an unmap for every map."));
	checkf(LoadedChunk.IORequest == nullptr, TEXT("RemoveResidentChunk was called on a chunk which hasn't been processed by ProcessCompletedChunks yet."));

	// Already loaded, so free it
	if (LoadedChunk.Memory != NULL)
	{
		FMemory::Free(LoadedChunk.Memory);
	}

	LoadedChunk.Memory = nullptr;
	LoadedChunk.IORequest = nullptr;
	LoadedChunk.DataSize = 0;
	LoadedChunk.Refcount = 0;
}

/**
This is called from some random thread when reading is complete.
*/
void FStreamingVolumeData::OnAsyncReadComplete(int32 LoadedChunkIndex, IBulkDataIORequest* ReadRequest)
{
	// We should do the least amount of work possible here as to not stall the async io threads.
	// We also cannot take the critical section here as this would lead to a deadlock between the
	// our critical section and the async-io internal critical section. 
	// So we just put this on queue here and then process the results later when we are on a different
	// thread that already holds our lock.
	// Game Thread:												... meanwhile on the Async loading thread:
	// - Get CriticalSection									- Get CachedFilesScopeLock as part of async code
	// - Call some async function								- Hey a request is complete start OnAsyncReadComplete
	// - Try get CachedFilesScopeLock as part of this function	- TRY get CriticalSection section waits for Game Thread
	// Both are waiting for each other's locks...
	// Note we cant clean the IO request up here. Trying to delete the request would deadlock
	// as delete waits until the request is complete bus it is only complete after the
	// callback returns ans since we're in the callback...

	CompletedChunks.Enqueue(FCompletedChunk(LoadedChunkIndex, ReadRequest));
}

/**
This does a blocking load for the first few seconds based on the component's current settings.
This ensures we got something to display initially.
*/
void FStreamingVolumeData::PrefetchData(IInterface_StreamableVolumetricAssetOwner* AssetOwner)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_FStreamingVolumeData_PrefetchData);

	FScopeLock Lock(&CriticalSection);
	check(IsInGameThread());

	TArray<uint32> NewChunksNeeded;
	AssetOwner->UpdateIndicesOfChunksToStream(NewChunksNeeded);

	for (int32 ChunkId : NewChunksNeeded)
	{
		// We just check here in case anything got loaded asynchronously last minute
		// to avoid unnecessary loading it synchronously again
		ProcessCompletedChunks();

		// Already got it
		if (ChunksAvailable.Contains(ChunkId))
		{
			continue;
		}

		// Still waiting for eviction, revive it
		if (ChunksEvicted.Contains(ChunkId))
		{
			ChunksEvicted.Remove(ChunkId);
			ChunksAvailable.Add(ChunkId);
			continue;
		}

		// Already requested an async load but not complete yet ... nothing much to do about this
		// it will just be loaded twice
		if (ChunksRequested.Contains(ChunkId))
		{
			ChunksRequested.Remove(ChunkId);
			// DEC_DWORD_STAT(STAT_VA_OutstandingRequests); This is not needed here it will be DEC'ed when the async completes
		}

		// Load chunk from bulk data if available.
		const uint32 ChunkDataSize = VolumeAsset->GetChunkDataSize(ChunkId);

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_FStreamingVolumeData_PrefetchData_Sync);

			FResidentChunk& ResidentChunk = AddResidentChunk(ChunkId, ChunkDataSize);
			ResidentChunk.Memory = static_cast<uint8*>(FMemory::Malloc(ChunkDataSize));
			VolumeAsset->PrefetchChunkSync(ChunkId, ResidentChunk.Memory);
		}

		ChunksAvailable.Add(ChunkId);
		VolumeAsset->OnChunkAvailable(ChunkId);
	}
}

void FStreamingVolumeData::UpdateStreamingStatus()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_FStreamingVolumeData_UpdateStreamingStatus);

	FScopeLock Lock(&CriticalSection);

	// Find any chunks that aren't available yet
	for (int32 NeededIndex : ChunksNeeded)
	{
		if (!ChunksAvailable.Contains(NeededIndex))
		{
			// Revive it if it was still pinned for some other thread.
			if (ChunksEvicted.Contains(NeededIndex))
			{
				ChunksEvicted.Remove(NeededIndex);
				ChunksAvailable.Add(NeededIndex);
				continue;
			}

			// If not requested yet, then request a load
			if (!ChunksRequested.Contains(NeededIndex))
			{
				const uint32 ChunkDataSize = VolumeAsset->GetChunkDataSize(NeededIndex);

				// This can happen in the editor if the asset hasn't been saved yet.
				if (VolumeAsset->IsDataAlreadyLoaded(NeededIndex))
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_FStreamingVolumeData_UpdateStreamingStatus_Copy);

					FResidentChunk& ResidentChunk = AddResidentChunk(NeededIndex, ChunkDataSize);
					ResidentChunk.Memory = FMemory::Malloc(ChunkDataSize);
					VolumeAsset->CopyChunkContentToMemory(NeededIndex, ResidentChunk.Memory);

					ChunksAvailable.Add(NeededIndex);
					VolumeAsset->OnChunkAvailable(NeededIndex);

					continue;
				}

				FResidentChunk& ResidentChunk = AddResidentChunk(NeededIndex, ChunkDataSize);

				FBulkDataIORequestCallBack AsyncFileCallBack = [this, NeededIndex](bool bWasCancelled, IBulkDataIORequest* Req)
				{
					this->OnAsyncReadComplete(NeededIndex, Req);
				};
				
				TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_FStreamingVolumeData_UpdateStreamingStatus_CreateRequest);

				ResidentChunk.IORequest = VolumeAsset->CreateStreamingRequest(NeededIndex, AsyncFileCallBack);
				if (!ResidentChunk.IORequest)
				{
					UE_LOG(LogVolumeStreaming, Error, TEXT("Volumetric animation streaming read request failed."));
					return;
				}

				// Add it to the list
				ChunksRequested.Add(NeededIndex);
			}

			// Nothing to do the chunk was requested and will be streamed in soon (hopefully)
		}
	}

	// Update bookkeeping with any recently completed chunks
	ProcessCompletedChunks();

	// Find indices that aren't needed anymore and add them to the list of chunks to evict
	for (int32 IndexInChunksAvailableArray = ChunksAvailable.Num() - 1; IndexInChunksAvailableArray >= 0; IndexInChunksAvailableArray--)
	{
		int32 RealChunkIndex = ChunksAvailable[IndexInChunksAvailableArray];
		if (!ChunksNeeded.Contains(RealChunkIndex))
		{
			ChunksEvicted.AddUnique(RealChunkIndex);
			VolumeAsset->OnChunkEvicting(RealChunkIndex);

			ChunksAvailable.RemoveAt(IndexInChunksAvailableArray);
		}
	}

	// Try to evict a bunch of chunks. Chunks which are still mapped (by other threads) can't be evicted
	// but others are free to go.
	for (int32 IndexInChunksEvictedArray = ChunksEvicted.Num() - 1; IndexInChunksEvictedArray >= 0; IndexInChunksEvictedArray--)
	{
		int32 RealChunkIndex = ChunksEvicted[IndexInChunksEvictedArray];

		FResidentChunk& ResidentChunk = Chunks[RealChunkIndex];
		if (ResidentChunk.Refcount == 0)
		{
			RemoveResidentChunk(ResidentChunk);

			ChunksEvicted.RemoveAt(IndexInChunksEvictedArray);
			VolumeAsset->OnChunkEvicted(RealChunkIndex);
		}

	}
}

bool FStreamingVolumeData::BlockTillAllRequestsFinished(float TimeLimit)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_FStreamingVolumeData_BlockTillAllRequestsFinished);

	FScopeLock Lock(&CriticalSection);

	if (TimeLimit == 0.0f)
	{
		for (auto Iter = Chunks.CreateIterator(); Iter; ++Iter)
		{
			if (Iter.Value().IORequest)
			{
				Iter.Value().IORequest->WaitCompletion();
				ProcessCompletedChunks();
			}
		}
	}
	else
	{
		double EndTime = FPlatformTime::Seconds() + TimeLimit;
		for (auto Iter = Chunks.CreateIterator(); Iter; ++Iter)
		{
			if (Iter.Value().IORequest)
			{
				float ThisTimeLimit = EndTime - FPlatformTime::Seconds();
				if (ThisTimeLimit < .001f || // one ms is the granularity of the platform event system
					!Iter.Value().IORequest->WaitCompletion(ThisTimeLimit))
				{
					return false;
				}
				ProcessCompletedChunks();
			}
		}
	}

	return true;
}

void FStreamingVolumeData::ProcessCompletedChunks()
{
	//Note: This function should only be called from code which owns the CriticalSection
	if (!IsInGameThread() && !IsInRenderingThread())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_FStreamingVolumeData_ProcessCompletedChunks);

	FCompletedChunk CompletedChunk;
	while (CompletedChunks.Dequeue(CompletedChunk))
	{
		FResidentChunk* Chunk = Chunks.Find(CompletedChunk.LoadedChunkIndex);
		if (!Chunk)
		{
			UE_LOG(LogVolumeStreaming, Error, TEXT("Got a stray async read request"));
			return;
		}

		// Chunks can be queued up multiple times when scrubbing but we can trust the LoadedChunkIndex of the completed chunk
		// so all we need to check is if the FResidentChunk  has a valid IORequest pointer or not.
		// If it is nullptr then we already processed a request for the chunk and it can be ignored.
		if (Chunk->IORequest != nullptr)
		{
			// Check to see if we successfully managed to load anything
			uint8* Mem = CompletedChunk.ReadRequest->GetReadResults();
			if (Mem)
			{
				Chunk->Memory = Mem;

				ChunksAvailable.Add(CompletedChunk.LoadedChunkIndex);
				VolumeAsset->OnChunkAvailable(CompletedChunk.LoadedChunkIndex);

				ChunksRequested.Remove(CompletedChunk.LoadedChunkIndex);

				IVolumeStreamingManager::Get().IoBandwidth.Add(Chunk->DataSize);
			}
			else
			{
				UE_LOG(LogVolumeStreaming, Error, TEXT("Async loading request failed!"));
				ChunksRequested.Remove(CompletedChunk.LoadedChunkIndex);
				// Fix me do we want to recover from this? Granite simply reschedules requests
				// as it may have failed for transient reasons (buffer contention, ...)
			}

			Chunk->IORequest = nullptr;
		}

		// Clean up the now fully processed IO request
		check(CompletedChunk.ReadRequest->PollCompletion());
		delete CompletedChunk.ReadRequest;
	}
}

const uint8* FStreamingVolumeData::MapChunk(uint32 ChunkIndex, bool ChunkHasToBeStreamed, uint32* OutChunkSize)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_FStreamingVolumeData_MapChunk);

	FScopeLock Lock(&CriticalSection);

	// Quickly check before mapping if maybe something new arrived we haven't done bookkeeping for yet
	ProcessCompletedChunks();

	if (!ChunksAvailable.Contains(ChunkIndex))
	{
		if (ChunkHasToBeStreamed)
		{
			if (!ChunksRequested.Contains(ChunkIndex))
			{
				if (ChunksEvicted.Contains(ChunkIndex))
				{
					UE_LOG(LogVolumeStreaming, Log, TEXT("Tried to map an evicted chunk: %i."), ChunkIndex);
				}
				else
				{
					UE_LOG(LogVolumeStreaming, Log, TEXT("Tried to map an unavailabe non-requested chunk: %i."), ChunkIndex);
				}
			}
			else
			{
				UE_LOG(LogVolumeStreaming, Log, TEXT("Tried to map a chunk (%i) that is still being streamed in."), ChunkIndex);
			}
		}

		return nullptr;
	}
	else
	{
		FResidentChunk *ResidentChunk = Chunks.Find(ChunkIndex);
		check(ResidentChunk);
		if (OutChunkSize)
		{
			*OutChunkSize = ResidentChunk->DataSize;
		}
		ResidentChunk->Refcount++;
		return (uint8*)ResidentChunk->Memory;
	}
}

void FStreamingVolumeData::UnmapChunk(uint32 ChunkIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_FStreamingVolumeData_UnmapChunk);

	FScopeLock Lock(&CriticalSection);

	FResidentChunk* ResidentChunk = Chunks.Find(ChunkIndex);

	if (ResidentChunk != nullptr )
	{
		checkf(ResidentChunk->Refcount > 0, TEXT("Map/Unmap out of balance. Make sure you unmap once fore every map."));
		checkf(ChunksAvailable.Contains(ChunkIndex) || ChunksEvicted.Contains(ChunkIndex), TEXT("Tried to unmap a chunk in an invalid state."));
		ResidentChunk->Refcount--;
	}
	else
	{
		UE_LOG(LogVolumeStreaming, Log, TEXT("Tried to unmap an unknown chunk."));
	}
}

bool FStreamingVolumeData::IsStreamingInProgress()
{
	FScopeLock Lock(&CriticalSection);
	return ChunksRequested.Num() > 0;
}
