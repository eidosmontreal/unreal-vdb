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

#include "VdbSequenceComponent.h"

#include "VdbCommon.h"
#include "VdbAssetComponent.h"
#include "VdbVolumeSequence.h"
#include "Rendering/VdbMaterialSceneProxy.h"
#include "Rendering/VdbMaterialRendering.h"

#include "RendererInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "GenericPlatform/GenericPlatformMath.h"

#define LOCTEXT_NAMESPACE "VdbSequenceComponent"

static TAutoConsoleVariable<int32> CVar_VdbSeq_NbFramesBehindToCache
(
	TEXT("VdbSequence.NbFramesBehindToCache"),
	1,
	TEXT("The number of old frames to cache behind of the current frame"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVar_VdbSeq_NbFramesAheadToCache
(
	TEXT("VdbSequence.NbFramesAheadToCache"),
	3,
	TEXT("The number of old frames to cache ahead of the current frame"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVar_VdbSeq_NbFramesToCacheBeforeStartingAnimation
(
	TEXT("VdbSequence.NbFramesToCacheBeforeStartingAnimation"),
	2,
	TEXT("The number of frames to cache before an animation can be started"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

UVdbSequenceComponent::UVdbSequenceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

UVdbSequenceComponent::~UVdbSequenceComponent() {}


void UVdbSequenceComponent::SetVdbAssets(UVdbAssetComponent* Component)
{
	VdbAssets = Component;
}

TObjectPtr<UVdbVolumeBase> UVdbSequenceComponent::GetPrimarySequence() const
{
	if (VdbAssets && VdbAssets->DensityVolume && VdbAssets->DensityVolume->IsSequence())
	{
		return VdbAssets->DensityVolume;
	}
	return nullptr;
}
const UVdbVolumeSequence* UVdbSequenceComponent::GetPrincipalSequence() const
{
	if (VdbAssets && VdbAssets->DensityVolume && VdbAssets->DensityVolume->IsSequence())
	{
		return Cast<const UVdbVolumeSequence>(VdbAssets->DensityVolume);
	}
	return nullptr;
}

void UVdbSequenceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Autoplay)
	{
		PlayAnimation();
	}
}

void UVdbSequenceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAnimation();

	Super::EndPlay(EndPlayReason);
}

void UVdbSequenceComponent::SetElapsedTimeToStartTime()
{
	const UVdbVolumeSequence* VdbSequence = GetPrincipalSequence();
	if (VdbSequence)
	{
		ElapsedTime = FMath::Clamp(OffsetRelative, 0.f, 1.0f) * VdbSequence->GetDurationInSeconds();
	}
}

void UVdbSequenceComponent::OnRegister()
{
	Super::OnRegister();

	SetElapsedTimeToStartTime();

	const UVdbVolumeSequence* VdbSequence = GetPrincipalSequence();
	if (VdbSequence != nullptr)
	{
		Duration = VdbSequence->GetDurationInSeconds() / PlaybackSpeed;
	}
	else
	{
		Duration = 0.f;
	}

	if (VdbAssets)
	{
		TArray<UVdbVolumeBase*> Volumes = VdbAssets->GetVolumes();
		for (UVdbVolumeBase* Volume : Volumes)
		{
			if (Volume && Volume->IsSequence())
			{
				UVdbVolumeSequence* VolumeSeq = Cast<UVdbVolumeSequence>(Volume);
				VolumeSeq->RegisterComponent(this);
			}
		}
	}

	IndexOfLastDisplayedFrame = uint32(~0);
	IVolumeStreamingManager::Get().AddStreamingComponent(this);
}

void UVdbSequenceComponent::OnUnregister()
{
	if (VdbAssets)
	{
		TArray<UVdbVolumeBase*> Volumes = VdbAssets->GetVolumes();
		for (UVdbVolumeBase* Volume : Volumes)
		{
			if (Volume && Volume->IsSequence())
			{
				UVdbVolumeSequence* VolumeSeq = Cast<UVdbVolumeSequence>(Volume);
				VolumeSeq->UnregisterComponent(this);
			}
		}
	}

	IVolumeStreamingManager::Get().RemoveStreamingComponent(this);
	Super::OnUnregister();
}

void UVdbSequenceComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ManualTick)
	{
		return;
	}

	if ((CurrentPlayMode == EVolumePlayMode::Stopped) || (GetNbFrames() == 0))
	{
		return;
	}

	// Buffer on play	
	if (NeedBuffering && VdbAssets)
	{
		const uint32 NbFramesToCacheBeforeStartingAnimation = (uint32)CVar_VdbSeq_NbFramesToCacheBeforeStartingAnimation.GetValueOnAnyThread();
		const uint32 BeginFrameIndex = GetFrameIndexFromElapsedTime();
		const uint32 EndFrameIndex = FMath::Min(BeginFrameIndex + NbFramesToCacheBeforeStartingAnimation, GetNbFrames());

		TArray<const UVdbVolumeBase*> Volumes = VdbAssets->GetConstVolumes();
		for (const UVdbVolumeBase* Volume : Volumes)
		{
			if (Volume && Volume->IsSequence())
			{
				const UVdbVolumeSequence* VolumeSeq = Cast<const UVdbVolumeSequence>(Volume);
				//UpdateSequenceStreaming(VolumeSeq, DeltaTime);

				for (uint32 CurrentFrameIndex = BeginFrameIndex; CurrentFrameIndex < EndFrameIndex; ++CurrentFrameIndex)
				{
					const bool FrameDataMustBeUploadedToGpu = (CurrentFrameIndex == BeginFrameIndex);
					if (!VolumeSeq->IsGridDataInMemory(CurrentFrameIndex, FrameDataMustBeUploadedToGpu))
					{
						return;
					}
				}
			}
		}

		NeedBuffering = false;
	}

	const UVdbVolumeSequence* VdbSequence = GetPrincipalSequence();

	// Update ElapsedTime and LoopCount
	float PreviousElapsedTime = ElapsedTime;
	ElapsedTime += DeltaTime * PlaybackSpeed;

	if (Looping)
	{
		ElapsedTime = fmodf(ElapsedTime, VdbSequence->GetDurationInSeconds());

		if (PreviousElapsedTime > ElapsedTime)
		{
			LoopCount++;
		}
	}
	else
	{
		if (ElapsedTime > VdbSequence->GetDurationInSeconds())
		{
			ElapsedTime = VdbSequence->GetDurationInSeconds();
		}
	}
}

void UVdbSequenceComponent::SetManualTick(bool InManualTick)
{
	ManualTick = InManualTick;
}

void UVdbSequenceComponent::UpdateIndicesOfChunksToStream(TArray<uint32>& IndicesOfChunksToStream)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_UVdbSequenceComponent_UpdateIndicesOfChunksToStream);

	check(IndicesOfChunksToStream.IsEmpty());

	if (!VdbAssets) return;

	bool bUpdateAsset = false;
	const uint32 FrameIndexToStream = GetFrameIndexFromElapsedTime();

	TArray<const UVdbVolumeBase*> Volumes = VdbAssets->GetConstVolumes();
	for (const UVdbVolumeBase* Volume : Volumes)
	{
		if (Volume && Volume->IsSequence())
		{
			const UVdbVolumeSequence* VdbSequence = Cast<const UVdbVolumeSequence>(Volume);

			// Keep in memory the data for the current frame
			IndicesOfChunksToStream.Add(VdbSequence->GetChunkIndexFromFrameIndex(FrameIndexToStream));

			// If the frame isn't ready to be displayed, keep in memory the data for the currently displayed frame
			if (VdbSequence->IsGridDataInMemory(FrameIndexToStream, true))
			{
				bUpdateAsset = true;
			}
			else
			{
				if ((IndexOfLastDisplayedFrame != uint32(~0)) && (IndexOfLastDisplayedFrame != FrameIndexToStream))
				{
					IndicesOfChunksToStream.Add(VdbSequence->GetChunkIndexFromFrameIndex(IndexOfLastDisplayedFrame));
				}
			}

			// In play mode or in manual tick mode, let's stream the data for the frames around the current one
			if ((CurrentPlayMode != EVolumePlayMode::Stopped) || ManualTick)
			{
				const uint32 NbFramesBehindToCacheFromConfigVar = (uint32)CVar_VdbSeq_NbFramesBehindToCache.GetValueOnAnyThread();
				const uint32 NbFramesAheadToCacheFromConfigVar = (uint32)CVar_VdbSeq_NbFramesAheadToCache.GetValueOnAnyThread();
				const uint32 MaxNbFramesToCache = FMath::Max(NbFramesBehindToCacheFromConfigVar, NbFramesAheadToCacheFromConfigVar);

				const uint32 NbFramesBehindToCache = ManualTick ? MaxNbFramesToCache : NbFramesBehindToCacheFromConfigVar;
				const uint32 NbFramesAheadToCache = ManualTick ? MaxNbFramesToCache : NbFramesAheadToCacheFromConfigVar;

				const uint32 IndexOfLastFrame = GetNbFrames() - 1;

				uint32 IndexFirstChunk = 0;
				uint32 IndexLastChunk = 0;

				if (Looping && (LoopCount > 0))
				{
					// When looping, the start maybe at the end of the animation
					int32 StartFrameIndex = int32(FrameIndexToStream) - int32(NbFramesBehindToCache);
					if (StartFrameIndex < 0)
					{
						StartFrameIndex = FMath::Max(int32(GetNbFrames()) + StartFrameIndex, 0);
					}

					IndexFirstChunk = VdbSequence->GetChunkIndexFromFrameIndex(uint32(StartFrameIndex));

					// When looping, the end maybe at the start of the animation
					uint32 EndFrameIndex = FrameIndexToStream + NbFramesAheadToCache;
					if (EndFrameIndex > IndexOfLastFrame)
					{
						EndFrameIndex = (FrameIndexToStream + NbFramesAheadToCache) - IndexOfLastFrame - 1;
					}

					IndexLastChunk = VdbSequence->GetChunkIndexFromFrameIndex(EndFrameIndex);
				}
				else
				{
					uint32 StartFrameIndex = (uint32)FMath::Max(int32(FrameIndexToStream) - int32(NbFramesBehindToCache), int32(0));
					IndexFirstChunk = VdbSequence->GetChunkIndexFromFrameIndex(StartFrameIndex);

					uint32 EndFrameIndex = FMath::Min(FrameIndexToStream + NbFramesAheadToCache, IndexOfLastFrame);
					IndexLastChunk = VdbSequence->GetChunkIndexFromFrameIndex(EndFrameIndex);
				}

				check(IndexFirstChunk < VdbSequence->GetNbFrames());
				check(IndexLastChunk < VdbSequence->GetNbFrames());
				AddIndicesOfChunksToStream(IndicesOfChunksToStream, VdbSequence->GetNbFrames(), IndexFirstChunk, IndexLastChunk);
			}
		}
	}

	if(bUpdateAsset)
	{
		VdbAssets->BroadcastFrameChanged(FrameIndexToStream);
		IndexOfLastDisplayedFrame = FrameIndexToStream;
	}

}

TArray<IInterface_StreamableVolumetricAsset*> UVdbSequenceComponent::GetStreamableAssets()
{
	TArray<IInterface_StreamableVolumetricAsset*> StreamableAssets;
	if (VdbAssets)
	{
		TArray<UVdbVolumeBase*> Volumes = VdbAssets->GetVolumes();
		for (UVdbVolumeBase* Volume : Volumes)
		{
			if (Volume && Volume->IsSequence())
			{
				UVdbVolumeSequence* VolumeSeq = Cast<UVdbVolumeSequence>(Volume);
				StreamableAssets.Add(VolumeSeq);
			}
		}
	}

	return StreamableAssets;
}

UObject* UVdbSequenceComponent::GetAssociatedUObject()
{
	return this;
}

float UVdbSequenceComponent::GetFrameIndexFloatFromElapsedTime() const
{
	const UVdbVolumeSequence* VdbSequence = GetPrincipalSequence();
	return (VdbSequence != nullptr) ? VdbSequence->GetFrameIndexFloatFromTime(ElapsedTime) : 0;
}

uint32 UVdbSequenceComponent::GetFrameIndexFromElapsedTime() const
{
	const UVdbVolumeSequence* VdbSequence = GetPrincipalSequence();
	return (VdbSequence != nullptr) ? VdbSequence->GetFrameIndexFromTime(ElapsedTime) : 0;
}

uint32 UVdbSequenceComponent::GetNbFrames() const
{
	const UVdbVolumeSequence* VdbSequence = GetPrincipalSequence();
	if (VdbSequence != nullptr)
	{
		return VdbSequence->GetNbFrames();
	}

	return 0;
}

void UVdbSequenceComponent::PlayAnimation()
{
	CurrentPlayMode = EVolumePlayMode::Playing;
}

void UVdbSequenceComponent::PauseAnimation()
{
	CurrentPlayMode = EVolumePlayMode::Paused;
}

void UVdbSequenceComponent::StopAnimation()
{
	if (CurrentPlayMode == EVolumePlayMode::Stopped)
	{
		return;
	}

	CurrentPlayMode = EVolumePlayMode::Stopped;
	SetElapsedTimeToStartTime();
	LoopCount = 0;
	NeedBuffering = true;
}

void UVdbSequenceComponent::ResetAnimationTime()
{
	SetElapsedTimeToStartTime();
}

void UVdbSequenceComponent::TickAtThisTime(const float Time, bool bInIsRunning, bool bInBackwards, bool bInIsLooping)
{
	if (ManualTick)
	{
		ElapsedTime = Time;
		if (VdbAssets && !bInIsRunning) VdbAssets->SetTargetFrameIndex(GetFrameIndexFromElapsedTime());
	}
}

void UVdbSequenceComponent::OnChunkAvailable(uint32 ChunkId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VolAnim_UVdbSequenceComponent_OnChunkAvailable);

	const UVdbVolumeSequence* VdbSequence = GetPrincipalSequence();
	if (ManualTick && VdbAssets)
	{
		const uint32 FrameIndex = GetFrameIndexFromElapsedTime();
		if (VdbSequence->GetChunkIndexFromFrameIndex(FrameIndex) == ChunkId && 
			VdbSequence->IsGridDataInMemory(FrameIndex, true))
		{
			VdbAssets->BroadcastFrameChanged(FrameIndex);
			IndexOfLastDisplayedFrame = FrameIndex;
		}
	}
}

#undef LOCTEXT_NAMESPACE