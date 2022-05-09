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

#include "VdbRendering.h"
#include "VdbShaders.h"
#include "VdbSceneProxy.h"
#include "VdbRenderBuffer.h"
#include "VdbCommon.h"
#include "VdbComposite.h"
#include "MeshCube.h"

#include "LocalVertexFactory.h"
#include "MeshPassProcessor.h"
#include "MeshPassProcessor.inl"
#include "Modules\ModuleManager.h"
#include "SceneView.h"
#include "ScenePrivate.h"

DEFINE_LOG_CATEGORY(LogSparseVolumetrics);

static TAutoConsoleVariable<int32> CVarVolumetricVdb(
	TEXT("r.Vdb"), 1,
	TEXT("VolumetricVdb components are rendered when this is not 0, otherwise ignored."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarVolumetricVdbMaxRayDepth(
	TEXT("r.Vdb.MaxRayDepth"), 0,
	TEXT("The maximum number of ray marching iterations inside the volume. Used only if > 0. Otherwise, fallback to engine value."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarVolumetricVdbSamples(
	TEXT("r.Vdb.SamplesPerPixel"), 0,
	TEXT("Number of samples per pixel, while raymarching through the volume. Used only if > 0. Otherwise, fallback to engine value."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarVolumetricVdbDenoiser(
	TEXT("r.Vdb.Denoiser"), -1,
	TEXT("Denoiser method applied on Vdb FogVolumes. Used only if >= 0. Otherwise, fallback to engine value."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

//-----------------------------------------------------------------------------
//--- FVdbMeshProcessor
//-----------------------------------------------------------------------------

class FVdbMeshProcessor : public FMeshPassProcessor
{
public:
	FVdbMeshProcessor(
		const FScene* Scene,
		const FSceneView* InView,
		FMeshPassDrawListContext* InDrawListContext,
		bool IsLevelSet,
		FVdbElementData&& ShaderElementData)
		: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InView, InDrawListContext)
		, VdbShaderElementData(ShaderElementData)
		, LevelSet(IsLevelSet)
	{
		PassDrawRenderState.SetViewUniformBuffer(InView->ViewUniformBuffer);

		if (IsLevelSet)
		{
			PassDrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
			PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
		}
		else
		{
			PassDrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI()); // alpha blending
			PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
		}
	}

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final
	{
		const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
		const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);

		if (Material && Material->GetMaterialDomain() == MD_Volume && Material->GetRenderingThreadShaderMap())
		{
			const ERasterizerFillMode MeshFillMode = FM_Solid;
			const ERasterizerCullMode MeshCullMode = CM_None;
			if (LevelSet)
			{
				Process<FVdbShaderVS, FVdbShaderPS_LevelSet>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
			}
			else
			{
				Process<FVdbShaderVS, FVdbShaderPS_FogVolume>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
			}
		}
	}

private:

	template<typename VertexShaderType, typename PixelShaderType>
	void Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		int32 StaticMeshId,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode)
	{
		VdbShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

		const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

		TMeshProcessorShaders<VertexShaderType, PixelShaderType> PassShaders;
		PassShaders.VertexShader = MaterialResource.GetShader<VertexShaderType>(VertexFactory->GetType(), 0, false);
		PassShaders.PixelShader = MaterialResource.GetShader<PixelShaderType>(VertexFactory->GetType(), 0, false);
		if (!PassShaders.VertexShader.IsValid() || !PassShaders.PixelShader.IsValid()) return;

		const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);
		BuildMeshDrawCommands(
			MeshBatch,
			BatchElementMask,
			PrimitiveSceneProxy,
			MaterialRenderProxy,
			MaterialResource,
			PassDrawRenderState,
			PassShaders,
			MeshFillMode,
			MeshCullMode,
			SortKey,
			EMeshPassFeatures::Default,
			VdbShaderElementData);
	}

	FMeshPassProcessorRenderState PassDrawRenderState;
	FVdbElementData VdbShaderElementData;
	bool LevelSet;
};

//-----------------------------------------------------------------------------
//--- FVdbRendering
//-----------------------------------------------------------------------------

FVdbRendering::FVdbRendering(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
}

bool FVdbRendering::ShouldRenderVolumetricVdb() const
{
	return CVarVolumetricVdb.GetValueOnRenderThread() > 0 && VertexFactory.IsValid();
}

void FVdbRendering::InitRendering()
{
	check(IsInRenderingThread());

	ReleaseRendering();
	{
		InitVolumeMesh();
		InitVertexFactory();
		InitDelegate();
	}
}

#define RELEASE_RESOURCE(A) if(A) { A->ReleaseResource(); A.Reset(); }
void FVdbRendering::ReleaseRendering()
{
	check(IsInRenderingThread());

	ReleaseDelegate();
	RELEASE_RESOURCE(VertexFactory);
	RELEASE_RESOURCE(VertexBuffer);
}
#undef RELEASE_RESOURCE

void FVdbRendering::Init()
{
	if (IsInRenderingThread())
	{
		InitRendering();
	}
	else
	{
		ENQUEUE_RENDER_COMMAND(InitVdbRendering)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				Init();
			});
	}
}

void FVdbRendering::Release()
{
	if (IsInRenderingThread())
	{
		ReleaseRendering();
	}
	else
	{
		ENQUEUE_RENDER_COMMAND(InitVdbRendering)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				Release();
			});
	}
}

void FVdbRendering::InitVolumeMesh()
{
	VertexBuffer = MakeUnique<FCubeMeshVertexBuffer>();
	VertexBuffer->InitResource();
}

void FVdbRendering::InitVertexFactory()
{
	VertexFactory = MakeUnique<FCubeMeshVertexFactory>(ERHIFeatureLevel::SM5);
	VertexFactory->Init(VertexBuffer.Get());
}

void FVdbRendering::InitDelegate()
{
	if (!RenderDelegateHandle.IsValid())
	{
		const FName RendererModuleName("Renderer");
		IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
		if (RendererModule)
		{
			RenderDelegate.BindRaw(this, &FVdbRendering::Render_RenderThread);
			RenderDelegateHandle = RendererModule->RegisterPostOpaqueRenderDelegate(RenderDelegate);
		}
	}
}

void FVdbRendering::ReleaseDelegate()
{
	if (RenderDelegateHandle.IsValid())
	{
		const FName RendererModuleName("Renderer");
		IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
		if (RendererModule)
		{
			RendererModule->RemovePostOpaqueRenderDelegate(RenderDelegateHandle);
		}

		RenderDelegateHandle.Reset();
	}
}

void FVdbRendering::CreateMeshBatch(FMeshBatch& MeshBatch, const FVdbSceneProxy* PrimitiveProxy, FVdbVertexFactoryUserDataWrapper& UserData, const FMaterialRenderProxy* MaterialProxy) const
{
	MeshBatch.bUseWireframeSelectionColoring = PrimitiveProxy->IsSelected();
	MeshBatch.VertexFactory = VertexFactory.Get();
	MeshBatch.MaterialRenderProxy = MaterialProxy;
	MeshBatch.ReverseCulling = PrimitiveProxy->IsLocalToWorldDeterminantNegative();
	MeshBatch.Type = PT_TriangleList;
	MeshBatch.DepthPriorityGroup = SDPG_World;
	MeshBatch.bCanApplyViewModeOverrides = true;
	MeshBatch.bUseForMaterial = true;
	MeshBatch.CastShadow = false;
	MeshBatch.bUseForDepthPass = false;

	FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
	BatchElement.PrimitiveUniformBuffer = nullptr;
	BatchElement.IndexBuffer = &VertexBuffer->IndexBuffer;
	BatchElement.FirstIndex = 0;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = VertexBuffer->NumVertices - 1;
	BatchElement.NumPrimitives = VertexBuffer->NumPrimitives;
	BatchElement.VertexFactoryUserData = VertexFactory->GetUniformBuffer();
	BatchElement.UserData = &UserData;
}

void FVdbRendering::Render_RenderThread(FPostOpaqueRenderParameters& Parameters)
{
	if (!ShouldRenderVolumetricVdb())
		return;

	SCOPE_CYCLE_COUNTER(STAT_VdbRendering_RT);

	const FSceneView* View = static_cast<FSceneView*>(Parameters.Uid);

	TArray<FVdbSceneProxy*> LeveSetProxies = VdbProxies.FilterByPredicate([View](const FVdbSceneProxy* Proxy) { return Proxy->IsLevelSet() && Proxy->IsVisible(View); });
	TArray<FVdbSceneProxy*> FogVolumeProxies = VdbProxies.FilterByPredicate([View](const FVdbSceneProxy* Proxy) { return !Proxy->IsLevelSet() && Proxy->IsVisible(View); });

	const FMatrix& ViewMat = View->ViewMatrices.GetViewMatrix();
	LeveSetProxies.Sort([ViewMat](const FVdbSceneProxy& Lhs, const FVdbSceneProxy& Rhs) -> bool 
		{ 
			const FVector& LeftProxyCenter = Lhs.GetBounds().GetSphere().Center;
			const FVector& RightProxyCenter = Rhs.GetBounds().GetSphere().Center;
			return ViewMat.TransformPosition(LeftProxyCenter).Z < ViewMat.TransformPosition(RightProxyCenter).Z; // front to back
		});
	FogVolumeProxies.Sort([ViewMat](const FVdbSceneProxy& Lhs, const FVdbSceneProxy& Rhs) -> bool
		{
			const FVector& LeftProxyCenter = Lhs.GetBounds().GetSphere().Center;
			const FVector& RightProxyCenter = Rhs.GetBounds().GetSphere().Center;
			return ViewMat.TransformPosition(LeftProxyCenter).Z > ViewMat.TransformPosition(RightProxyCenter).Z; // back to front
		});

	auto DrawVdbProxies = [&](const TArray<FVdbSceneProxy*>& Proxies, bool IsLevelSet, TRDGUniformBufferRef<FVdbShaderParams> VdbUniformBuffer, FRDGTexture* RenderTexture)
	{
		FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;

		auto* PassParameters = GraphBuilder.AllocParameters<FVdbShaderParametersPS>();
		PassParameters->VdbUniformBuffer = VdbUniformBuffer;
		PassParameters->InstanceCulling = FInstanceCullingContext::CreateDummyInstanceCullingUniformBuffer(GraphBuilder);
		if (RenderTexture)
		{
			PassParameters->RenderTargets[0] = FRenderTargetBinding(RenderTexture, ERenderTargetLoadAction::EClear);
			// Don't bind depth buffer, we will read it in Pixel Shader instead
		}
		else
		{
			PassParameters->RenderTargets[0] = FRenderTargetBinding(Parameters.ColorTexture, ERenderTargetLoadAction::ELoad);
			PassParameters->RenderTargets.DepthStencil =
				FDepthStencilBinding(Parameters.DepthTexture, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilNop);
		}

		GraphBuilder.AddPass(
			IsLevelSet ? RDG_EVENT_NAME("Vdb LevelSet Rendering") : RDG_EVENT_NAME("Vdb FogVolume Rendering"),
			PassParameters,
			ERDGPassFlags::Raster,
			[this, &InView = *View, ViewportRect = Parameters.ViewportRect, Proxies](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.SetViewport(ViewportRect.Min.X, ViewportRect.Min.Y, 0.0f, ViewportRect.Max.X, ViewportRect.Max.Y, 1.0f);
			RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

			for (const FVdbSceneProxy* Proxy : Proxies)
			{
				if (Proxy && Proxy->GetMaterial() && Proxy->IsVisible(&InView) && Proxy->GetRenderResource())
				{
					DrawDynamicMeshPass(InView, RHICmdList,
						[&](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
						{
							FVdbElementData ShaderElementData;
							ShaderElementData.DensityMultiplier = Proxy->GetDensityMultiplier();
							ShaderElementData.StepMultiplier = Proxy->GetStepMultiplier();
							ShaderElementData.BufferSRV = Proxy->GetRenderResource()->GetBufferSRV();
							if (!ShaderElementData.BufferSRV)
								return;

							FVdbMeshProcessor PassMeshProcessor(
								InView.Family->Scene->GetRenderScene(),
								&InView,
								DynamicMeshPassContext,
								Proxy->IsLevelSet(),
								MoveTemp(ShaderElementData));

							FVdbVertexFactoryUserDataWrapper UserData;
							UserData.Data.IndexMin = Proxy->GetIndexMin();
							UserData.Data.IndexSize = Proxy->GetIndexSize();
							UserData.Data.IndexToLocal = Proxy->GetIndexToLocal();

							FMeshBatch CubeMesh;
							CreateMeshBatch(CubeMesh, Proxy, UserData, Proxy->GetMaterial()->GetRenderProxy());

							const uint64 DefaultBatchElementMask = ~0ull; // or 1 << 0; // LOD 0 only
							PassMeshProcessor.AddMeshBatch(CubeMesh, DefaultBatchElementMask, Proxy);
						}
					);
				}
			}
		});
	};

	FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;

	uint32 Samples = CVarVolumetricVdbSamples.GetValueOnAnyThread() > 0 ? (uint32)CVarVolumetricVdbSamples.GetValueOnAnyThread() : NbSamples;
	uint32 RayDepth = CVarVolumetricVdbMaxRayDepth.GetValueOnAnyThread() > 0 ? (uint32)CVarVolumetricVdbMaxRayDepth.GetValueOnAnyThread() : MaxRayDepth;

	FVdbShaderParams* UniformParameters = GraphBuilder.AllocParameters<FVdbShaderParams>();
	UniformParameters->SamplesPerPixel = Samples;
	UniformParameters->MaxRayDepth = RayDepth;
	UniformParameters->SceneDepthTexture = Parameters.DepthTexture;
	TRDGUniformBufferRef<FVdbShaderParams> VdbUniformBuffer = GraphBuilder.CreateUniformBuffer(UniformParameters);

	if (!LeveSetProxies.IsEmpty())
	{
		SCOPE_CYCLE_COUNTER(STAT_VdbLevelSets_RT);
		DrawVdbProxies(LeveSetProxies, true, VdbUniformBuffer, nullptr);
	}

	if (!FogVolumeProxies.IsEmpty())
	{
		SCOPE_CYCLE_COUNTER(STAT_VdbFogVolumes_RT);

		FRDGTextureDesc TexDesc = Parameters.ColorTexture->Desc;
		TexDesc.ClearValue = FClearValueBinding(FLinearColor::Transparent);
		FRDGTexture* VdbCurrRenderTexture = GraphBuilder.CreateTexture(TexDesc, TEXT("VdbRenderTexture"));

		DrawVdbProxies(FogVolumeProxies, false, VdbUniformBuffer, VdbCurrRenderTexture);

		// Add optional post-processing (blurring, denoising etc.)
		EVdbDenoiserMethod Method = CVarVolumetricVdbDenoiser.GetValueOnAnyThread() >= 0 ?
			EVdbDenoiserMethod(CVarVolumetricVdbDenoiser.GetValueOnAnyThread()) : DenoiserMethod;
		FRDGTexture* DenoisedTex = VdbDenoiser::ApplyDenoising(
			GraphBuilder, VdbCurrRenderTexture, View, Parameters.ViewportRect, Method);

		// Composite VDB offscreen rendering onto back buffer
		VdbComposite::CompositeFullscreen(GraphBuilder, DenoisedTex, Parameters.ColorTexture, View);
	}
}

void FVdbRendering::AddVdbProxy(FVdbSceneProxy* Proxy)
{
	ENQUEUE_RENDER_COMMAND(FAddVdbProxyCommand)(
		[this, Proxy](FRHICommandListImmediate& RHICmdList)
		{
			check(VdbProxies.Find(Proxy) == INDEX_NONE);
			VdbProxies.Emplace(Proxy);
		});
}

void FVdbRendering::RemoveVdbProxy(FVdbSceneProxy* Proxy)
{
	ENQUEUE_RENDER_COMMAND(FRemoveVdbProxyCommand)(
		[this, Proxy](FRHICommandListImmediate& RHICmdList)
		{
			auto Idx = VdbProxies.Find(Proxy);
			if (Idx != INDEX_NONE)
			{
				VdbProxies.Remove(Proxy);
			}
		});
}

void FVdbRendering::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	// Reset visibility on all registered FVdbProxies, before SceneVisibility is computed 
	for (FVdbSceneProxy* Proxy : VdbProxies)
	{
		Proxy->ResetVisibility();
	}
}
