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

#include "VdbRenderBufferPool.h"
#include "VdbCommon.h"

FVdbRenderBufferPool::FVdbRenderBufferPool(int ByteSize, int NumAllocation, int MaxAllocations)
	:	MemByteSize(ByteSize)
	,	NumElementsMax(MaxAllocations)
{
	AllocatedBuffers.SetNum(NumAllocation);
}

FVdbRenderBufferPtr FVdbRenderBufferPool::GetBuffer()
{
	// First find any available buffer.
	for (auto& PooledBufferPair : AllocatedBuffers)
	{
		// Still being used outside the pool.
		if (PooledBufferPair.Key.GetRefCount() > 1)
		{
			continue;
		}

		PooledBufferPair.Value = FrameCounter;
		return PooledBufferPair.Key;
	}

	// Safety check. There should not be a very large number of allocations, buffers are probably not released when they should have been
	check(AllocatedBuffers.Num() < NumElementsMax);

	// Allocate new one
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FVdbRenderBufferPool::CreateBuffer);

		TRefCountPtr<FVdbRenderBuffer> NewBuffer = new FVdbRenderBuffer();
		NewBuffer->SetData(MemByteSize, nullptr);
		BeginInitResource(NewBuffer);

		AllocatedBuffers.Add(TPair<FVdbRenderBufferPtr, uint32>(NewBuffer, FrameCounter));

		return NewBuffer;
	}
}

void FVdbRenderBufferPool::Release()
{
	for (auto& Pair : AllocatedBuffers)
	{
		BeginReleaseResource(Pair.Key);
	}
}

// Heavily inspired by FRenderGraphResourcePool::TickPoolElements
void FVdbRenderBufferPool::TickPoolElements()
{
	SCOPED_NAMED_EVENT(VolAnim_FVdbRenderBufferPool_TickPoolElements, FColor::Cyan);

	const uint32 kFramesUntilRelease = 30;

	int32 BufferIndex = 0;

	while (BufferIndex < AllocatedBuffers.Num())
	{
		TPair<FVdbRenderBufferPtr, uint32>& Pair = AllocatedBuffers[BufferIndex];
		FVdbRenderBufferPtr& Buffer = Pair.Key;
		uint32 LastUsedFrame = Pair.Value;

		const bool bIsUnused = Buffer.GetRefCount() == 1;

		// This will probably problematic when FrameCounter overflows
		const bool bNotRequestedRecently = (FrameCounter - LastUsedFrame) > kFramesUntilRelease;

		if (bIsUnused && bNotRequestedRecently)
		{
			SCOPED_NAMED_EVENT(VolAnim_FVdbRenderBufferPool_TickPoolElements_BeginRelease, FColor::Yellow);

			BeginReleaseResource(Buffer); // Release resource
			BuffersBeingReleased.Enqueue(Buffer);

			Swap(Pair, AllocatedBuffers.Last());
			AllocatedBuffers.Pop();
		}
		else
		{
			++BufferIndex;
		}
	}

	++FrameCounter;

	// clean up released resources
	while (!BuffersBeingReleased.IsEmpty() && !BuffersBeingReleased.Peek()->GetReference()->IsInitialized())
	{
		SCOPED_NAMED_EVENT(VolAnim_FVdbRenderBufferPool_TickPoolElements_Pop, FColor::Yellow);
		BuffersBeingReleased.Pop();
	}
}