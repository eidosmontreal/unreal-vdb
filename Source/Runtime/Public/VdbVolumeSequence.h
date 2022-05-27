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
#include "UObject/NoExportTypes.h"
#include "Misc/Paths.h"
#include "Serialization/BulkData.h"
#include "VdbVolumeBase.h"
#include "VolumeStreamingManager.h"

THIRD_PARTY_INCLUDES_START
#include <nanovdb/NanoVDB.h>
#include <nanovdb/util/GridHandle.h>
THIRD_PARTY_INCLUDES_END

#include "VdbVolumeSequence.generated.h"

class FVdbRenderBuffer;
class UVdbSequenceComponent;

USTRUCT()
struct VOLUMERUNTIME_API FVdbSequenceChunk
{
	GENERATED_USTRUCT_BODY()

	/** Size of the chunk of data in bytes */
	int32 DataSize = 0;

	/** Frame index of the earliest frame stored in this block */
	int32 FirstFrame = 0;

	/** End frame index of the interval this chunk contains data for.
	This closed so the last frame is included in the interval
	*/
	int32 LastFrame = 0;

	/** Bulk data if stored in the package. */
	FByteBulkData BulkData;

	/** Serialization. */
	void Serialize(FArchive& Ar, UObject* Owner, int32 ChunkIndex);

	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize);
};

// NanoVDB file container
UCLASS(BlueprintType, hidecategories = (Object))
class VOLUMERUNTIME_API UVdbVolumeSequence : public UVdbVolumeBase, public IInterface_StreamableVolumetricAsset
{
	GENERATED_UCLASS_BODY()

public:
	UVdbVolumeSequence();
	UVdbVolumeSequence(FVTableHelper& Helper);
	virtual ~UVdbVolumeSequence();

	//~ Begin UObject Interface.
	virtual void BeginDestroy() override;
	virtual void FinishDestroy() override;
	virtual FString GetDesc() override;	
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface.
	
	//~ Begin UVdbVolumeBase Interface.
	virtual bool IsValid() const;
	virtual const FBox& GetBounds(uint32 FrameIndex) const;
	virtual const FIntVector& GetIndexMin(uint32 FrameIndex) const;
	virtual const FIntVector& GetIndexMax(uint32 FrameIndex) const;
	virtual const FMatrix44f& GetIndexToLocal(uint32 FrameIndex) const;
	virtual const uint8* GetGridData(uint32 FrameIndex) const { return VolumeRenderInfos[FrameIndex].GetNanoGridHandle().data(); }
	virtual const nanovdb::GridMetaData* GetMetaData(uint32 FrameIndex) { return VolumeRenderInfos[FrameIndex].GetNanoGridHandle().gridMetaData(); }
	//~ End UVdbVolumeBase Interface.

	template<typename T>
	const nanovdb::NanoGrid<T>* GetNanoGrid(uint32 FrameIndex) { return VolumeRenderInfos[FrameIndex].GetNanoGridHandle().grid<T>(); }
	virtual const FVolumeRenderInfos* GetRenderInfos(uint32 FrameIndex) const override { return &VolumeRenderInfos[FrameIndex]; }
	bool IsGridDataInMemory(uint32 FrameIndex, bool CheckIsAlsoUploadedToGPU) const;

	float GetTimeBetweenFramesInSeconds() const { return 0.0333333333333333333f; }
	float GetDurationInSeconds() const { return GetTimeBetweenFramesInSeconds() * float(FMath::Max(uint32(1), GetNbFrames()) - 1); }
	uint32 GetFrameIndexFromTime(float InputAnimTime) const;
	float GetFrameIndexFloatFromTime(float InputAnimTime) const;

	TArray<FVdbSequenceChunk>& GetChunks() { return Chunks; }
	uint32 GetChunkIndexFromFrameIndex(uint32 FrameIndex) const;
	void RegisterComponent(UVdbSequenceComponent* VdbSeqComponent);
	void UnregisterComponent(UVdbSequenceComponent* VdbSeqComponent);

#if WITH_EDITOR
	void PrepareRendering();
	void AddFrame(nanovdb::GridHandle<>& NanoGridHandle, EQuantizationType InQuantization);
	void FinalizeImport(const FString& Filename);
#endif

	// IInterface_StreamableVolumetricAsset
	virtual uint32 GetNbFrames() const override { return (uint32)VolumeFramesInfos.Num(); }
	virtual uint32 GetChunkDataSize(uint32 ChunkId) const override;
	virtual void UpdateChunksNeeded(TArray<int32>& ChunksNeeded) override;
	virtual bool IsDataAlreadyLoaded(uint32 ChunkId) const override;
	virtual void PrefetchChunkSync(uint32 ChunkId, void* ResidentChunkMemory) override;
	virtual void OnChunkEvicting(uint32 ChunkId) override;
	virtual void OnChunkEvicted(uint32 ChunkId) override;
	virtual void OnChunkAvailable(uint32 ChunkId) override;
	virtual void CopyChunkContentToMemory(uint32 ChunkId, void* ResidentChunkMemory) override;
	virtual IBulkDataIORequest* CreateStreamingRequest(uint32 ChunkId, FBulkDataIORequestCallBack& AsyncFileCallBack) override;


private:
	void ValidateFrameToChunkRatio() const;
	void InitResources();
	void ReleaseResources();

	UPROPERTY()
	uint64 FrameMaxMemoryUsage = 0;

	UPROPERTY(VisibleAnywhere, Category = "Properties")
	TArray<FVolumeFrameInfos> VolumeFramesInfos;

	TArray<FVolumeRenderInfos> VolumeRenderInfos;

	TArray<FVdbSequenceChunk> Chunks;
	TArray<int32> ChunksWithPendingUpload;

	TUniquePtr<class FVdbRenderBufferPool> BufferPool;

	FCriticalSection ComponentsLock;
	TArray<UVdbSequenceComponent*> VdbSequenceComponents;
};
