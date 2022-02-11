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
#include "RendererInterface.h"
#include "SceneViewExtension.h"
#include "VdbDenoiser.h"

class FVdbSceneProxy;

class FVdbRendering : public FSceneViewExtensionBase
{
public:

	FVdbRendering(const FAutoRegister& AutoRegister);

	bool ShouldRenderVolumetricVdb() const;

	void Init();
	void Release();

	void AddVdbProxy(FVdbSceneProxy* Proxy);
	void RemoveVdbProxy(FVdbSceneProxy* Proxy);

	void CreateMeshBatch(struct FMeshBatch&, const FVdbSceneProxy*, struct FVdbVertexFactoryUserDataWrapper&, const class FMaterialRenderProxy*) const;

	//~ Setters
	void SetNbSamples(int Samples) { NbSamples = (uint32)FMath::Max(1, Samples); }
	void SetMaxRayDepth(int Depth) { MaxRayDepth = (uint32)FMath::Max(1, Depth); }
	void SetDenoiserMethod(EVdbDenoiserMethod Method) { DenoiserMethod = Method; }
	//~ End Setters

	//~ Begin ISceneViewExtension Interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override {}
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	virtual int32 GetPriority() const override { return -1; }
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const { return true; }
	//~ End ISceneViewExtension Interface

private:

	void InitRendering();
	void ReleaseRendering();

	void InitVolumeMesh();
	void InitVertexFactory();
	void InitDelegate();
	void ReleaseDelegate();

	void Render_RenderThread(FPostOpaqueRenderParameters& Parameters);

	TArray<FVdbSceneProxy*> VdbProxies;
	TUniquePtr<class FCubeMeshVertexBuffer> VertexBuffer;
	TUniquePtr<class FCubeMeshVertexFactory> VertexFactory;
	FPostOpaqueRenderDelegate RenderDelegate;
	FDelegateHandle RenderDelegateHandle;

	uint32 NbSamples = 1;
	uint32 MaxRayDepth = 5;
	EVdbDenoiserMethod DenoiserMethod = EVdbDenoiserMethod::GaussianBlur;
};