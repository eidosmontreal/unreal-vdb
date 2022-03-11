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
#include "VdbComponentBase.h"
#include "VolumeStreamingManager.h"

#include "VdbComponent.generated.h"

class FVolumeRenderInfos;
class UVdbSequenceComponent;

UCLASS(Blueprintable, ClassGroup = Rendering, hideCategories = (Activation, Collision, Cooking, HLOD, Navigation, Lighting, Physics, VirtualTexture, Mobile), meta = (BlueprintSpawnableComponent))
class UVdbComponent : public UVdbComponentBase
{
	GENERATED_UCLASS_BODY()

	virtual ~UVdbComponent();

	// If FogVolume, represents Density values. If LevelSet, represent a narrow-band level values
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Volumetric)
	TObjectPtr<class UVdbVolumeBase> VdbVolume;

	// Must be a Volume domain material.
	UPROPERTY(EditAnywhere, Category = Volumetric)
	TObjectPtr<class UMaterialInterface> Material;

	// Global Density multiplier
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FogVolume, meta = (UIMin = "0.0001", ClampMin = "0.0001"))
	float DensityMultiplier = 1.f;

	// Raymarching step distance multiplier. The smaller the more accurate, but also the more expensive. Only use small values 
	// to capture small missing features. It is recommended to keep this multiplier as high as possible for better performance.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FogVolume, meta = (UIMin = "0.1", ClampMin = "0.1"))
	float StepMultiplier = 4.f;

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const  override;
	//~ End USceneComponent Interface.

	//~ Begin UPrimitiveComponent Interface.
	virtual UMaterialInterface* GetMaterial(int32 Index) const override { return Material; }
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UVdbComponentBase Interface.
	virtual bool UpdateSceneProxy(uint32 FrameIndex, class UVdbVolumeSequence* Seq) override;
	//~ End UVdbComponentBase Interface.

	EVdbType GetVdbType() const;
	void SetSeqComponent(UVdbSequenceComponent* Comp) { SequenceComponent = Comp; }
	const UVdbSequenceComponent* GetSeqComponent() const {  return SequenceComponent; }

protected:
	//~ Begin UObject Interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface.

	UVdbSequenceComponent* SequenceComponent;
};

// Volumetric sparse data actor based on NanoVDB
UCLASS(ClassGroup = Rendering, Meta = (ComponentWrapperClass))
class VOLUMERUNTIME_API AVdbActor : public AActor
{
	GENERATED_UCLASS_BODY()

	friend UVdbComponent;

public:

	UVdbComponent* GetVdbComponent() const { return VdbComponent; }
	UVdbSequenceComponent* GetSeqComponent() const { return SeqComponent; }

private:

#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif

	UPROPERTY(BlueprintReadOnly, Category = Volumetric, VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVdbComponent> VdbComponent;

	UPROPERTY(BlueprintReadOnly, Category = Volumetric, VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVdbSequenceComponent> SeqComponent;
};