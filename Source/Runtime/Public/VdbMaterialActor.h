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
#include "VolumeStreamingManager.h"

#include "VdbMaterialActor.generated.h"

class FVolumeRenderInfos;
class UVdbSequenceComponent;
class UVdbVolumeBase;

// Sparse Volumetric VDB actor, using Unreal's material system
UCLASS(ClassGroup = Rendering, HideCategories = (Activation, Input, Physics, Materials, Collision, Lighting, LOD, HLOD, Mobile, Navigation, VirtualTexture), Meta = (ComponentWrapperClass))
class VOLUMERUNTIME_API AVdbMaterialActor : public AActor
{
	GENERATED_UCLASS_BODY()

public:

	class UVdbAssetComponent* GetVdbAssetComponent() const { return AssetComponent; }

private:

#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif

	UPROPERTY(BlueprintReadOnly, Category = Volumetric, VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UVdbAssetComponent> AssetComponent;

	UPROPERTY(BlueprintReadOnly, Category = Volumetric, VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UVdbMaterialComponent> MaterialComponent;

	UPROPERTY(BlueprintReadOnly, Category = Volumetric, VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UVdbSequenceComponent> SeqComponent;
};