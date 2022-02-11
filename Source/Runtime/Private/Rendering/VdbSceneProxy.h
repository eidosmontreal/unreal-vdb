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
#include "PrimitiveSceneProxy.h"

class UVdbComponentBase;

// Render Thread equivalent of VdbComponent
class FVdbSceneProxy : public FPrimitiveSceneProxy
{
public:
	FVdbSceneProxy(const UVdbComponentBase* InComponent);
	virtual ~FVdbSceneProxy() = default;

	FVector GetIndexMin() const { return IndexMin; }
	FVector GetIndexSize() const { return IndexSize; }
	float GetStepMultiplier() const { return StepMultiplier; }
	float GetDensityMultiplier() const { return DensityMultiplier; }
	const FMatrix& GetIndexToLocal() const { return IndexToLocal; }
	class UMaterialInterface* GetMaterial() const { return Material; }
	const class FVdbRenderBuffer* GetRenderResource() const { return RenderBuffer; }
	
	bool IsLevelSet() const { return LevelSet; }
	void ResetVisibility() { VisibleViews.Empty(4); }
	bool IsVisible(const FSceneView* View) const { return VisibleViews.Find(View) != INDEX_NONE; }
	void Update(const FMatrix& IndexToLocal, const FVector& IndexMin, const FVector& IndexSize, FVdbRenderBuffer* RenderBuffer);

protected:
	//~ Begin FPrimitiveSceneProxy Interface
	virtual SIZE_T GetTypeHash() const override;
	virtual void CreateRenderThreadResources() override;
	virtual void DestroyRenderThreadResources() override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual uint32 GetMemoryFootprint() const override { return(sizeof(*this) + GetAllocatedSize()); }
	//~ End FPrimitiveSceneProxy Interface

private:
	
	TSharedPtr<class FVdbRendering, ESPMode::ThreadSafe> VdbRenderExtension;

	// Fixed attributes
	const UVdbComponentBase* VdbComponent = nullptr;
	class UMaterialInterface* Material = nullptr;
	bool LevelSet;

	float DensityMultiplier;
	float StepMultiplier;

	FVdbRenderBuffer* RenderBuffer;
	FVector IndexMin;
	FVector IndexSize;
	FMatrix IndexToLocal;

	mutable TArray<const FSceneView*> VisibleViews;
};