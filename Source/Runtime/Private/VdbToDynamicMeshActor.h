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
#include "DynamicMeshActor.h"

#include "VdbToDynamicMeshActor.generated.h"

// Actor that combines VdbToVolumeTexture component with a dynamic mesh.
// This class needs to be blueprinted, and the blueprint needs to implement UpdateDynamicMesh
// See BP_VdbToDynamicMesh for an example.
UCLASS(ClassGroup = Rendering, Meta = (ComponentWrapperClass))
class AVdbToDynamicMeshActor : public ADynamicMeshActor
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintImplementableEvent)
	void UpdateDynamicMesh();

private:

#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Volumetric, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UVdbAssetComponent> AssetComponent;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Volumetric, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UVdbToVolumeTextureComponent> VdbToTexComponent;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Volumetric, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UVdbSequenceComponent> SequenceComponent;

	void UpdateDynamicMeshInternal(uint32 FrameIndex);
};