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
#include "VdbCommon.h"

#include "VdbAssetComponent.generated.h"

class FVolumeRenderInfos;
class UVdbSequenceComponent;
class UVdbVolumeBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVdbChanged, int32, FrameIndex);

// Can contain several grids of the same OpenVDB/NanoVDB file
UCLASS(Blueprintable, ClassGroup = Rendering, hideCategories = (Activation, Collision, Cooking, HLOD, Navigation, VirtualTexture), meta = (BlueprintSpawnableComponent))
class UVdbAssetComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	virtual ~UVdbAssetComponent();

	//----------------------------------------------------------------------------

	// Principal mandatory volume (VDB float grid). If FogVolume, Density values. If LevelSet, narrow-band level set values.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Volume, meta = (AllowPrivateAccess = "true", DisplayName = "Density (float)"))
	TObjectPtr<UVdbVolumeBase> DensityVolume;

	// Optional second volume (VDB float grid). If FogVolume, Temperature values. If LevelSet, unused.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Volume, meta = (AllowPrivateAccess = "true", DisplayName = "Temperature (float)"))
	TObjectPtr<UVdbVolumeBase> TemperatureVolume;

	// Optional third volume (VDB vector grid). If FogVolume, Color values. If LevelSet, unused.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Volume, meta = (AllowPrivateAccess = "true", DisplayName = "Color (vector)"))
	TObjectPtr<UVdbVolumeBase> ColorVolume;

	//----------------------------------------------------------------------------

	void BroadcastFrameChanged(uint32 Frame);
	void GetReferencedContentObjects(TArray<UObject*>& Objects) const;

	EVdbClass GetVdbClass() const;
	const FVolumeRenderInfos* GetRenderInfos(const UVdbVolumeBase* VdbVolumeStatic) const;

	TArray<const class UVdbVolumeBase*> GetConstVolumes() const;
	TArray<class UVdbVolumeBase*> GetVolumes();

	UFUNCTION(BlueprintCallable, Category = Volume)
	FVector3f GetVolumeSize() const;

	UFUNCTION(BlueprintCallable, Category = Volume)
	FVector3f GetVolumeOffset() const;

	UFUNCTION(BlueprintCallable, Category = Volume)
	FVector3f GetVolumeUvScale() const;

	UPROPERTY(BlueprintAssignable, Category = Volume)
	FOnVdbChanged OnVdbChanged;
	
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFrameChanged, uint32);
	FOnFrameChanged OnFrameChanged;

	void SetTargetFrameIndex(uint32 Frame) { TargetFrameIndex = Frame; }
	uint32 GetCurrFrameIndex() const { return CurrFrameIndex; }

private:

	uint32 CurrFrameIndex = 0;
	uint32 TargetFrameIndex = 0;

public:

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};
