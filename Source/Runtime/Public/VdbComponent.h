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

#include "VdbComponent.generated.h"

class FVolumeRenderInfos;

UCLASS(Blueprintable, ClassGroup = Rendering, hideCategories = (Activation, Collision, Cooking, HLOD, Navigation, /*Mobility, Object, Physics,*/ VirtualTexture), meta = (BlueprintSpawnableComponent))
class UVdbComponent : public UVdbComponentBase
{
	GENERATED_UCLASS_BODY()

	virtual ~UVdbComponent();

	// If FogVolume, represents Density values. If LevelSet, represent a narrow-band level values
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Volumetric)
	TObjectPtr<class UVdbVolume> VdbVolume;

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const  override;
	//~ End USceneComponent Interface.

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UVdbComponentBase Interface.
	virtual FVolumeRenderInfos* GetRenderInfos() const override;
	virtual EVdbType GetVdbType() const override;
	//~ End UVdbComponentBase Interface.
};


// Volumetric sparse data actor based on NanoVDB
UCLASS(ClassGroup = Rendering, Meta = (ComponentWrapperClass))
class VOLUMERUNTIME_API AVdbActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:

#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Atmosphere, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UVdbComponent> VdbComponent;

public:
	UVdbComponent* GetVdbComponent() const { return VdbComponent; }
};