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

// Experimentation / research rendering. Performance is not our priority.
// Use this to experiment old or new rendering and denoising techniques.
// This render path is not Unreal-compliant, it doesn't use Unreal materials, 
// it doesn't display most of Unreal debug and help features, it's only here 
// for fun and experimentation. 
// Have you always wanted to try using NanoVDB in a/your prototype renderer ? 
// Now you can using Unreal, and you get access to a rasterizer and a path-tracer for free.

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "SceneViewExtension.h"
#include "VdbDenoiser.h"
#include "VolumeMesh.h"

class FVdbPrincipledSceneProxy;

// Very basic manager to handle VdbPrincipled rendering
class FVdbPrincipledRendering : public FSceneViewExtensionBase
{
public:

	FVdbPrincipledRendering(const FAutoRegister& AutoRegister);

	void Init(UTextureRenderTarget2D* DefaultRenderTarget);
	void Release();

	void AddVdbProxy(FVdbPrincipledSceneProxy* Proxy);
	void RemoveVdbProxy(FVdbPrincipledSceneProxy* Proxy);

	//~ Begin ISceneViewExtension Interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override {}
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual int32 GetPriority() const override { return -1; }
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const { return true; }
	//~ End ISceneViewExtension Interface

private:

	void InitRendering();
	void InitBuffers();
	void InitDelegate();
	void ReleaseRendering();
	void ReleaseDelegate();

	void Render_RenderThread(FPostOpaqueRenderParameters& Parameters);

	TArray<FVdbPrincipledSceneProxy*> VdbProxies;
	FPostOpaqueRenderDelegate RenderDelegate;
	FDelegateHandle RenderDelegateHandle;

	UTextureRenderTarget2D* DefaultVdbRenderTarget;
	FTexture* DefaultVdbRenderTargetTex;

	FBufferRHIRef IndexBufferRHI;
	FBufferRHIRef VertexBufferRHI;

	EVdbDenoiserMethod DenoiserMethod = EVdbDenoiserMethod::None;
};