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
#include "RenderResource.h"
#include "VdbRenderBuffer.h"

typedef TRefCountPtr<FVdbRenderBuffer> FVdbRenderBufferPtr;

// Pools VdbRenderBuffers for sequence rendering
class FVdbRenderBufferPool : public FRenderResource
{
public:
	FVdbRenderBufferPool(int ByteSize, int NumAllocations = 0, int MaxAllocations = 1000);

	int GetBufferSize() const { return MemByteSize; }
	FVdbRenderBufferPtr GetBuffer();

	void TickPoolElements();
	void Release();

private:
	TArray<TPair<FVdbRenderBufferPtr, uint32>> AllocatedBuffers;
	TQueue<FVdbRenderBufferPtr> BuffersBeingReleased;

	const int MemByteSize;
	const int NumElementsMax;

	uint32 FrameCounter = 0;
};
