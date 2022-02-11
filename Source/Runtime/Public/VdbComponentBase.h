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

#include "VdbComponentBase.generated.h"

class FVdbRenderBuffer;


#if WITH_EDITOR
void MarkRenderStateDirtyForAllVdbComponents(UObject* InputObject);
#endif

UCLASS(Blueprintable, Abstract, ClassGroup = Rendering, hideCategories = (Activation, Collision, Cooking, HLOD, Navigation, /*Mobility, Object, Physics,*/ VirtualTexture), meta = (BlueprintSpawnableComponent))
class UVdbComponentBase : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	virtual ~UVdbComponentBase();

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

	//~ Begin UPrimitiveComponent Interface.
	virtual UMaterialInterface* GetMaterial(int32 Index) const override { return Material; }
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual bool SupportsStaticLighting() const override { return false; }
	//~ End UPrimitiveComponent Interface.

	//~ Begin UVdbComponentBase Interface.
	virtual FVolumeRenderInfos* GetRenderInfos() const { check(false); return nullptr; }
	virtual EVdbType GetVdbType() const { check(false); return EVdbType::Undefined; }
	//~ End UVdbComponentBase Interface.

protected:
	static FIntVector DummyIntVect;
	static FMatrix DummyMatrix;

	//~ Begin UObject Interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface.
};
