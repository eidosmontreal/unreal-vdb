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
#include "VdbVolumeStatic.h"

class FVdbRenderBuffer : public FRenderResource, public FRefCountedObject
{
public:
	void SetData(uint64 InVolumeMemorySize, const uint8* InVolumeGridData);	
	void UploadData(uint64 MemByteSize, const uint8* MemPtr);
	bool IsUploadFinished() const { return UploadFinished; }

	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;

	FORCEINLINE uint64 GetCapacity() const { return ByteSize; }
	FShaderResourceViewRHIRef GetBufferSRV() const { return BufferSRV; }

private:

	FBufferRHIRef Buffer = nullptr;
	FShaderResourceViewRHIRef BufferSRV;

	void* DataPtr = nullptr;
	uint64 ByteSize = 0;

	bool UploadFinished = true;
};
