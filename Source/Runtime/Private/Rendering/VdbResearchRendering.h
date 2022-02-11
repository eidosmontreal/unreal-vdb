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
#include "VdbDenoiser.h"
#include "MeshCube.h"

class FVdbResearchSceneProxy;

// Very basic manager to handle VdbResearch rendering
class FVdbResearchRendering
{
public:

	FVdbResearchRendering() = default;
	~FVdbResearchRendering() = default;
	FVdbResearchRendering(const FVdbResearchRendering&) = delete;
	FVdbResearchRendering(FVdbResearchRendering&&) = delete;

	void Init();
	void Release();

	void AddVdbProxy(FVdbResearchSceneProxy* Proxy);
	void RemoveVdbProxy(FVdbResearchSceneProxy* Proxy);

private:

	void InitRendering();
	void InitBuffers();
	void InitDelegate();
	void ReleaseRendering();
	void ReleaseDelegate();

	void Render_RenderThread(FPostOpaqueRenderParameters& Parameters);

	TArray<FVdbResearchSceneProxy*> VdbProxies;
	FPostOpaqueRenderDelegate RenderDelegate;
	FDelegateHandle RenderDelegateHandle;

	FBufferRHIRef IndexBufferRHI;
	FBufferRHIRef VertexBufferRHI;

	EVdbDenoiserMethod DenoiserMethod = EVdbDenoiserMethod::None;
};