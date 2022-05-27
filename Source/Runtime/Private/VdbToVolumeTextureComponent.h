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
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

#include "VdbToVolumeTextureComponent.generated.h"

class UVdbSequenceComponent;
class FTextureResource;

// TODO: We need to find a robust way to only update once if the source volume isn't changing.
// For now, update every frame. 
#define TICK_EVERY_FRAME

UENUM(BlueprintType)
enum class EVdbToVolumeMethod : uint8
{
	Disabled				UMETA(DisplayName = "No Conversion"),
	PrimaryR8				UMETA(DisplayName = "Primary R8"),
	PrimaryR16F				UMETA(DisplayName = "Primary R16F"),
	PrimaryRGBA8			UMETA(DisplayName = "Primary RGBA8"),
	PrimaryRGBA16F			UMETA(DisplayName = "Primary RGBA16F"),
	PrimarySecondaryRG8		UMETA(DisplayName = "Primary R8 + Secondary B8, RG8"),
	PrimarySecondaryRG16F	UMETA(DisplayName = "Primary R16F + Secondary B16F, RG16F"),
	PrimarySecondaryRGBA8	UMETA(DisplayName = "Primary A8 + Secondary RGB8, RGBA8"),
	PrimarySecondaryRGBA16F	UMETA(DisplayName = "Primary A16F + Secondary RGB16F, RGBA16F"),

	Count					UMETA(Hidden)
};

// Dynamically convert VDBs to Volume Textures, using a Volume Render Target
UCLASS(Blueprintable, ClassGroup = Rendering, hideCategories = (Activation, Collision, Cooking, HLOD, Navigation, VirtualTexture), meta = (BlueprintSpawnableComponent))
class UVdbToVolumeTextureComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	virtual ~UVdbToVolumeTextureComponent();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set VdbAssetComponent"), Category = Volume)
	void SetVdbAssets(class UVdbAssetComponent* Comp);

	// Target Volume Texture, to be used anywhere else
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Volume)
	TObjectPtr<class UTextureRenderTargetVolume> VolumeRenderTarget;

	// Packing method
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Volume)
	EVdbToVolumeMethod Method = EVdbToVolumeMethod::PrimaryR8;

	//~ Begin UObject Interface.
	virtual void PostLoad() override;
	//~ End UObject Interface.

private:

#ifdef TICK_EVERY_FRAME
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
#endif

	void UpdateRenderTargetIfNeeded(bool Force = false);

	void CopyVdbToVolume_GameThread(uint32 FrameIndex);

	void CopyVdbToVolume_RenderThread(
		class FRHICommandListImmediate& RHICmdList, 
		class FTextureRenderTargetResource* RenderTarget, 
		class FVdbRenderBuffer* DensityRenderBuffer, 
		class FVdbRenderBuffer* TemperatureRenderBuffer,
		const FVector3f& IndexMin,
		const FVector3f& IndexSize,
		const FIntVector& TextureSize);

	class UVdbAssetComponent* VdbAssets;
};
