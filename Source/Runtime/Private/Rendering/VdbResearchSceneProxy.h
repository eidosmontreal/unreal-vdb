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

class FVdbRenderBuffer;
class FVdbResearchRendering;

struct FVdbResearchParams
{
	FVdbRenderBuffer* VdbDensity;
	FVdbRenderBuffer* VdbTemperature;
	FVector IndexMin;
	FVector IndexSize;
	FMatrix IndexToLocal;
	uint32 MaxRayDepth;
	uint32 SamplesPerPixel;
	FLinearColor Color;
	float DensityMult;
	float Albedo;
	float Anisotropy;
	float EmissionStrength;
	FLinearColor EmissionColor;
	float BlackbodyIntensity;
	FLinearColor BlackbodyTint;
	float Temperature;
};

// Render Thread equivalent of VdbResearchComponent
class FVdbResearchSceneProxy : public FPrimitiveSceneProxy
{
public:
	FVdbResearchSceneProxy(const class UVdbResearchComponent* InComponent);
	virtual ~FVdbResearchSceneProxy() = default;

	void UpdateProperties_RenderThread();

	const FVdbResearchParams& GetParams() const { return Params; }
	bool GetDisplayBounds() const { return DisplayBounds; }
	bool GetDisplayUnfinishedPaths() const { return DisplayUnfinishedPaths; }

	FRDGTextureRef GetOrCreateRenderTarget(FRDGBuilder& GraphBuilder, const FIntPoint& RtSize, bool EvenFrame);

	void ResetVisibility() { VisibleViews.Empty(4); }
	bool IsVisible(const FSceneView* View) const { return VisibleViews.Find(View) != INDEX_NONE; }
	void Update(const FMatrix& InIndexToLocal, const FVector& InIndexMin, const FVector& InIndexSize, FVdbRenderBuffer* RenderBuffer, bool IsDensity);

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

	TSharedPtr<FVdbResearchRendering, ESPMode::ThreadSafe> VdbRenderMgr;
	FVdbResearchParams Params;
	bool DisplayBounds;
	bool DisplayUnfinishedPaths;

	// RTs per proxy, for easier translucency support
	TRefCountPtr<IPooledRenderTarget> OffscreenRenderTarget[2];

	mutable TArray<const FSceneView*> VisibleViews;
};
