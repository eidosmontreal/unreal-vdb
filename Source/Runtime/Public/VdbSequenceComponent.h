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
#include "Components/PrimitiveComponent.h"
#include "VolumeStreamingManager.h"

#include "VdbSequenceComponent.generated.h"

class IInterface_StreamableVolumetricAsset;

// Handles frame by frame animation of NanoVDB assets of the linked VdbAssetComponent
UCLASS(Blueprintable, ClassGroup = Rendering, hideCategories = (Activation, Collision, Cooking, HLOD, Navigation, Mobility, Object, Physics, VirtualTexture), meta = (BlueprintSpawnableComponent))
class VOLUMERUNTIME_API UVdbSequenceComponent : public UActorComponent, public IInterface_StreamableVolumetricAssetOwner
{
	GENERATED_UCLASS_BODY()

	virtual ~UVdbSequenceComponent();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set VdbAssetComponent"), Category = "Components|VdbSequence")
	void SetVdbAssets(class UVdbAssetComponent* Component);

	//~ Begin UActorComponent Interface.
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	//~ End UActorComponent Interface.

	//~ Begin IInterface_StreamableVolumetricAssetOwner Interface.
	virtual void UpdateIndicesOfChunksToStream(TArray<uint32>& IndicesOfChunksToStream) override;
	virtual TArray<IInterface_StreamableVolumetricAsset*> GetStreamableAssets() override;
	virtual UObject* GetAssociatedUObject() override;
	//~ End IInterface_StreamableVolumetricAssetOwner Interface.

	/** Start playback of animation */
	UFUNCTION(BlueprintCallable, Category = "Components|VdbSequence")
	void PlayAnimation();

	/** Start playback of animation */
	UFUNCTION(BlueprintCallable, Category = "Components|VdbSequence")
	void PauseAnimation();

	/** Stop playback of animation */
	UFUNCTION(BlueprintCallable, Category = "Components|VdbSequence")
	void StopAnimation();

	UFUNCTION(BlueprintCallable, Category = "Components|VdbSequence")
	void TickAtThisTime(const float Time, bool bInIsRunning, bool bInBackwards, bool bInIsLooping);

	uint32 GetFrameIndexFromElapsedTime() const;
	float GetFrameIndexFloatFromElapsedTime() const;
	uint32 GetNbFrames() const;
	void SetElapsedTimeToStartTime();
	void ResetAnimationTime();
	bool GetManualTick() const { return ManualTick; }
	void SetManualTick(bool InManualTick);
	void OnChunkAvailable(uint32 ChunkId);


	const class UVdbVolumeSequence* GetPrincipalSequence() const;
	TObjectPtr<class UVdbVolumeBase> GetPrimarySequence() const;

private:

	// Play Sequence / Animation in game. If not, let Sequencer control this animation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Playback", meta = (AllowPrivateAccess = "true"))
	bool Autoplay = true;

	// Is animation looping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Playback", meta = (EditCondition = "Autoplay", AllowPrivateAccess = "true"))
	bool Looping = true;

	// Speed at which the animation is playing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Playback", meta = (UIMin = 0.0001, ClampMin = 0.0001, EditCondition = "Autoplay", AllowPrivateAccess = "true"))
	float PlaybackSpeed = 1.0f;

	// Duration of the sequence
	UPROPERTY(VisibleAnywhere, transient, Category = "Volume|Playback", meta = (AllowPrivateAccess = "true"))
	float Duration = 0.f;

	// Sequence start offset, in relative range [0, 1] where 0 represents the start and 1 the end of the sequence.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Playback", meta = (DisplayName = "Offset", UIMin = 0.0, UIMax = 1.0, ClampMin = 0.0, ClampMax = 1.0, AllowPrivateAccess = "true"))
	float OffsetRelative = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Volume|Playback", meta = (AllowPrivateAccess = "true"))
	EVolumePlayMode CurrentPlayMode = EVolumePlayMode::Stopped;

	UPROPERTY(BlueprintReadOnly, transient, Category = "Volume|Playback", meta = (AllowPrivateAccess = "true"))
	float ElapsedTime = 0.f;

private:

	uint32 LoopCount = 0;
	uint32 IndexOfLastDisplayedFrame = uint32(~0);
	bool NeedBuffering = true;
	bool ManualTick = false;

	class UVdbAssetComponent* VdbAssets;
};

