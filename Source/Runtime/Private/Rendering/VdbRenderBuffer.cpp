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

#include "VdbRenderBuffer.h"
#include "VdbCommon.h"
#include "RenderingThread.h"

DEFINE_LOG_CATEGORY_STATIC(LogVdbRenderBuffer, Log, All);

void FVdbRenderBuffer::SetData(uint64 InVolumeMemorySize, const uint8* InVolumeGridData)
{
	ByteSize = InVolumeMemorySize;
	DataPtr = (void*)InVolumeGridData;
}

void FVdbRenderBuffer::InitRHI()
{
	check(IsInRenderingThread());

	const uint32 Stride = sizeof(uint32);

	FRHIResourceCreateInfo CreateInfo(TEXT("FVdbRenderBuffer"));
	Buffer = RHICreateStructuredBuffer(Stride, ByteSize, BUF_Static | BUF_ShaderResource, CreateInfo);
	
	BufferSRV = RHICreateShaderResourceView(Buffer, Stride, PF_R32_UINT);

	if (DataPtr)
	{
		uint32* BufferMemory = (uint32*)RHILockBuffer(Buffer, 0, ByteSize, RLM_WriteOnly);
		FMemory::Memcpy(BufferMemory, DataPtr, ByteSize);
		RHIUnlockBuffer(Buffer);
	}

	INC_MEMORY_STAT_BY(STAT_VdbGPUDataInterfaceMemory, ByteSize);
}

void FVdbRenderBuffer::ReleaseRHI()
{
	check(IsInRenderingThread());

	if (Buffer)
	{
		Buffer.SafeRelease();
	}

	BufferSRV.SafeRelease();

	DEC_MEMORY_STAT_BY(STAT_VdbGPUDataInterfaceMemory, ByteSize);
}

void FVdbRenderBuffer::UploadData(uint64 MemByteSize, const uint8* MemPtr)
{
	UploadFinished = false;

	ENQUEUE_RENDER_COMMAND(UplodaVdbGpuData)(
		[this, MemByteSize, MemPtr](FRHICommandList& RHICmdList)
		{
			uint32* BufferMemory = (uint32*)RHILockBuffer(Buffer, 0, MemByteSize, RLM_WriteOnly);
			FMemory::Memcpy(BufferMemory, MemPtr, MemByteSize);
			RHIUnlockBuffer(Buffer);

			UploadFinished = true;
		});
}