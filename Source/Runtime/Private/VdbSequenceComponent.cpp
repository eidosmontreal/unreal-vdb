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
#include "VdbVolumeSequence.h"
#include "Rendering/VdbSceneProxy.h"
#include "Rendering/VdbRendering.h"

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

//-----------------------------------------------------------------------------
// Component
//-----------------------------------------------------------------------------

UVdbSequenceComponent::UVdbSequenceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

UVdbSequenceComponent::~UVdbSequenceComponent() {}

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
	if (VdbSequence)
	{
		ElapsedTime = FMath::Clamp(OffsetRelative, 0.f, 1.0f) * VdbSequence->GetDurationInSeconds();
	}
}

void UVdbSequenceComponent::OnRegister()
{	
	Super::OnRegister();
	
	SetElapsedTimeToStartTime();

	if (VdbSequence != nullptr)
	{
		VdbSequence->RegisterComponent(this);
		Duration = VdbSequence->GetDurationInSeconds() / PlaybackSpeed;
	}	
	else
	{
		Duration = 0.f;
	}

	IndexOfLastDisplayedFrame = uint32(~0);
	IVolumeStreamingManager::Get().AddStreamingComponent(this);
}

void UVdbSequenceComponent::OnUnregister()
{
	if (VdbSequence != nullptr)
	{
		VdbSequence->UnregisterComponent(this);
	}

	IVolumeStreamingManager::Get().RemoveStreamingComponent(this);
	Super::OnUnregister();
}

FPrimitiveSceneProxy* UVdbSequenceComponent::CreateSceneProxy()
{
	if (!VdbSequence || !VdbSequence->IsValid() || !GetMaterial(0))
		return nullptr;

	return new FVdbSceneProxy(this);
}

FBoxSphereBounds UVdbSequenceComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (VdbSequence != nullptr)
	{
		FBoxSphereBounds VdbBounds(VdbSequence->GetBounds());
		return VdbBounds.TransformBy(LocalToWorld);
	}
	else
	{
		return Super::CalcBounds(LocalToWorld);
	}
}

void UVdbSequenceComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ManualTick)
	{
		return;
	}

	if ((CurrentPlayMode == EVolumePlayMode::Stopped) || (VdbSequence == nullptr) || (GetNbFrames() == 0))
	{
		return;
	}

	// Buffer on play	
	if (NeedBuffering)
	{
		const uint32 NbFramesToCacheBeforeStartingAnimation = (uint32)CVar_VdbSeq_NbFramesToCacheBeforeStartingAnimation.GetValueOnAnyThread();		
		const uint32 BeginFrameIndex = GetFrameIndexFromElapsedTime();
		const uint32 EndFrameIndex = FMath::Min(BeginFrameIndex + NbFramesToCacheBeforeStartingAnimation, GetNbFrames());

		for (uint32 CurrentFrameIndex = BeginFrameIndex; CurrentFrameIndex < EndFrameIndex; ++CurrentFrameIndex)
		{
			const bool FrameDataMustBeUploadedToGpu = (CurrentFrameIndex == BeginFrameIndex);
			if (!VdbSequence->IsGridDataInMemory(CurrentFrameIndex, FrameDataMustBeUploadedToGpu))
			{
				return;
			}
		}

		NeedBuffering = false;
	}

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

bool UVdbSequenceComponent::UpdateSceneProxy(uint32 FrameIndex)
{
	if (!VdbSequence->IsGridDataInMemory(FrameIndex, true) || (SceneProxy == nullptr))
	{
		return false;
	}

	IndexOfLastDisplayedFrame = FrameIndex;

	FVolumeRenderInfos* RenderInfos = GetRenderInfos();
	if (RenderInfos)
	{
		ENQUEUE_RENDER_COMMAND(UploadVdbGpuData)(
			[this,
			IndexMin = RenderInfos->GetIndexMin(),
			IndexSize = RenderInfos->GetIndexSize(),
			IndexToLocal = RenderInfos->GetIndexToLocal(),
			RenderBuffer = RenderInfos->GetRenderResource()]
		(FRHICommandList& RHICmdList)
		{
			FVdbSceneProxy* VdbSceneProxy = static_cast<FVdbSceneProxy*>(SceneProxy);
			if (VdbSceneProxy != nullptr)
			{
				VdbSceneProxy->Update(IndexToLocal, IndexMin, IndexSize, RenderBuffer);
			}
		});
	}

	return true;
}

void UVdbSequenceComponent::SetManualTick(bool InManualTick) 
{ 
	ManualTick = InManualTick; 
}

void UVdbSequenceComponent::UpdateIndicesOfChunksToStream(TArray<uint32>& IndicesOfChunksToStream)
{
	check(IndicesOfChunksToStream.IsEmpty());

	if (VdbSequence == nullptr)
	{
		return;
	}

	// Keep in memory the data for the current frame
	const uint32 FrameIndexToStream = GetFrameIndexFromElapsedTime();
	IndicesOfChunksToStream.Add(VdbSequence->GetChunkIndexFromFrameIndex(FrameIndexToStream));

	// If the frame isn't ready to be displayed, keep in memory the data for the currently displayed frame
	if (!UpdateSceneProxy(FrameIndexToStream))
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

IInterface_StreamableVolumetricAsset* UVdbSequenceComponent::GetStreamableAsset()
{
	return VdbSequence;
}

UObject* UVdbSequenceComponent::GetAssociatedUObject()
{
	return this;
}

float UVdbSequenceComponent::GetFrameIndexFloatFromElapsedTime() const
{
	return (VdbSequence != nullptr) ? VdbSequence->GetFrameIndexFloatFromTime(ElapsedTime) : 0;
}

uint32 UVdbSequenceComponent::GetFrameIndexFromElapsedTime() const
{
	return (VdbSequence != nullptr) ? VdbSequence->GetFrameIndexFromTime(ElapsedTime) : 0;
}

uint32 UVdbSequenceComponent::GetNbFrames() const
{
	if (VdbSequence != nullptr)
	{
		return VdbSequence->GetNbFrames();
	}

	return 0;
}

bool UVdbSequenceComponent::SetVdbSequence(UVdbVolumeSequence* Sequence)
{
	// Do nothing if we are already using the supplied sequence
	if (VdbSequence == Sequence)
	{
		return false;
	}

	StopAnimation();
	VdbSequence = Sequence;	
	return true;
}

void UVdbSequenceComponent::PlayAnimation()
{
	if (VdbSequence == nullptr)
	{
		return;
	}

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

FVolumeRenderInfos* UVdbSequenceComponent::GetRenderInfos() const
{
	if (VdbSequence != nullptr)
	{
		const uint32 FrameIndex = GetFrameIndexFromElapsedTime();
		return &VdbSequence->GetRenderInfos(FrameIndex);
	}
	else
	{
		return nullptr;
	}
}

EVdbType UVdbSequenceComponent::GetVdbType() const
{
	if (VdbSequence != nullptr)
	{
		return VdbSequence->GetType();
	}
	else
	{
		return EVdbType::Undefined;
	}
}

void UVdbSequenceComponent::ResetAnimationTime()
{
	SetElapsedTimeToStartTime();
}

void UVdbSequenceComponent::TickAtThisTime(const float Time, bool bInIsRunning, bool bInBackwards, bool bInIsLooping)
{
	if (ManualTick && (VdbSequence != nullptr))
	{
		ElapsedTime = Time;
	}
}

void UVdbSequenceComponent::OnChunkAvailable(uint32 ChunkId)
{
	if (ManualTick)
	{
		const uint32 FrameIndex = GetFrameIndexFromElapsedTime();		
		if (VdbSequence->GetChunkIndexFromFrameIndex(FrameIndex) == ChunkId)
		{
			UpdateSceneProxy(FrameIndex);
		}
	}
}

#undef LOCTEXT_NAMESPACE