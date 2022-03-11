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
class UVdbVolumeBase;
class UVdbSequenceComponent;

#if WITH_EDITOR
void MarkRenderStateDirtyForAllVdbComponents(UObject* InputObject);
#endif

UCLASS(Blueprintable, Abstract, ClassGroup = Rendering, hideCategories = (Activation, Collision, Cooking, HLOD, Navigation, VirtualTexture), meta = (BlueprintSpawnableComponent))
class UVdbComponentBase : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	virtual ~UVdbComponentBase();

	//~ Begin UPrimitiveComponent Interface.
	virtual bool SupportsStaticLighting() const override { return false; }
	//~ End UPrimitiveComponent Interface.

	//~ Begin UVdbComponentBase Interface.
	virtual bool UpdateSceneProxy(uint32 FrameIndex, class UVdbVolumeSequence* Seq) { check(false); return false; }
#if WITH_EDITOR
	virtual void UpdateSeqProperties(const UVdbSequenceComponent* SeqComponent) {}
#endif
	//~ End UVdbComponentBase Interface.

	void SetVdbSequence(UVdbVolumeBase* SeqVolume, UVdbSequenceComponent* SeqComponent);
	const FVolumeRenderInfos* GetRenderInfos(const UVdbVolumeBase* VdbVolume, const UVdbSequenceComponent* SeqComponent) const;

};
