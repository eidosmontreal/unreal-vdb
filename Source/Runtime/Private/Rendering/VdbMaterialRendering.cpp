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

#include "VdbMaterialRendering.h"
#include "VdbShaders.h"
#include "VdbMaterialSceneProxy.h"
#include "VdbRenderBuffer.h"
#include "VdbCommon.h"
#include "VdbComposite.h"
#include "VolumeMesh.h"

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

static TAutoConsoleVariable<int32> CVarVolumetricVdbDenoiser(
	TEXT("r.Vdb.Denoiser"), -1,
	TEXT("Denoiser method applied on Vdb FogVolumes. Used only if >= 0. Otherwise, fallback to engine value."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarVolumetricVdbThreshold(
	TEXT("r.Vdb.Threshold"), 0.01,
	TEXT("Transmittance threshold to stop raymarching. Lower values are better but more expensive. Must be close to 0."),
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
		bool IsLevelSet, bool IsTranslucentLevelSet,
		bool ImprovedSkylight,
		bool UseTempVdb, bool UseColorVdb,
		bool UseExtraVdbs,
		FVdbElementData&& ShaderElementData)
		: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InView, InDrawListContext)
		, VdbShaderElementData(ShaderElementData)
		, bLevelSet(IsLevelSet)
		, bTranslucentLevelSet(IsTranslucentLevelSet)
		, bImprovedSkylight(ImprovedSkylight)
		, bTemperatureVdb(UseTempVdb)
		, bColorVdb(UseColorVdb)
		, bExtraVdbs(UseExtraVdbs)
	{
		PassDrawRenderState.SetViewUniformBuffer(InView->ViewUniformBuffer);

		if (bLevelSet && !bTranslucentLevelSet)
		{
			PassDrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
			PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
		}
		else
		{
			PassDrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI()); // premultiplied alpha blending
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
			if (bLevelSet)
			{
				if (bTranslucentLevelSet && bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_LevelSet_Translucent_EnvLight>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else if (bTranslucentLevelSet)
				{
					Process<FVdbShaderVS, FVdbShaderPS_LevelSet_Translucent>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else
				{
					Process<FVdbShaderVS, FVdbShaderPS_LevelSet>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
			}
			else
			{
				// combination of 4 params: 2^4 = 16 different cases
				// TODO: this is getting ridiculous, find better solution
				if (!bTemperatureVdb && !bColorVdb && !bExtraVdbs && !bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_FogVolume>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else if (!bTemperatureVdb && !bColorVdb && !bExtraVdbs && bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_FogVolume_EnvLight>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else if (!bTemperatureVdb && !bColorVdb && bExtraVdbs && !bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_FogVolume_Extra>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else if (!bTemperatureVdb && !bColorVdb && bExtraVdbs && bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_FogVolume_Extra_EnvLight>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else if (!bTemperatureVdb && bColorVdb && !bExtraVdbs && !bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_FogVolume_Color>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else if (!bTemperatureVdb && bColorVdb && !bExtraVdbs && bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_FogVolume_Color_EnvLight>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else if (!bTemperatureVdb && bColorVdb && bExtraVdbs && !bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_FogVolume_Color_Extra>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else if (!bTemperatureVdb && bColorVdb && bExtraVdbs && bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_FogVolume_Color_Extra_EnvLight>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else if (bTemperatureVdb && !bColorVdb && !bExtraVdbs && !bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_FogVolume_Blackbody>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else if (bTemperatureVdb && !bColorVdb && !bExtraVdbs && bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_FogVolume_Blackbody_EnvLight>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else if (bTemperatureVdb && !bColorVdb && bExtraVdbs && !bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_FogVolume_Blackbody_Extra>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else if (bTemperatureVdb && !bColorVdb && bExtraVdbs && bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_FogVolume_Blackbody_Extra_EnvLight>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else if (bTemperatureVdb && bColorVdb && !bExtraVdbs && !bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_FogVolume_Blackbody_Color>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else if (bTemperatureVdb && bColorVdb && !bExtraVdbs && bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_FogVolume_Blackbody_Color_EnvLight>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else if (bTemperatureVdb && bColorVdb && bExtraVdbs && !bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_FogVolume_Blackbody_Color_Extra>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
				else if (bTemperatureVdb && bColorVdb && bExtraVdbs && bImprovedSkylight)
				{
					Process<FVdbShaderVS, FVdbShaderPS_FogVolume_Blackbody_Color_Extra_EnvLight>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode);
				}
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
	bool bLevelSet;
	bool bTranslucentLevelSet;
	bool bImprovedSkylight;
	bool bTemperatureVdb;
	bool bColorVdb;
	bool bExtraVdbs;
};

//-----------------------------------------------------------------------------
//--- FVdbMaterialRendering
//-----------------------------------------------------------------------------

FVdbMaterialRendering::FVdbMaterialRendering(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
}

bool FVdbMaterialRendering::ShouldRenderVolumetricVdb() const
{
	return CVarVolumetricVdb.GetValueOnRenderThread() > 0 && VertexFactory.IsValid();
}

void FVdbMaterialRendering::InitRendering()
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
void FVdbMaterialRendering::ReleaseRendering()
{
	check(IsInRenderingThread());

	ReleaseDelegate();
	RELEASE_RESOURCE(VertexFactory);
	RELEASE_RESOURCE(VertexBuffer);
}
#undef RELEASE_RESOURCE

void FVdbMaterialRendering::Init()
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

void FVdbMaterialRendering::Release()
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

void FVdbMaterialRendering::InitVolumeMesh()
{
	VertexBuffer = MakeUnique<FVolumeMeshVertexBuffer>();
	VertexBuffer->InitResource();
}

void FVdbMaterialRendering::InitVertexFactory()
{
	VertexFactory = MakeUnique<FVolumeMeshVertexFactory>(ERHIFeatureLevel::SM5);
	VertexFactory->Init(VertexBuffer.Get());
}

void FVdbMaterialRendering::InitDelegate()
{
	if (!RenderDelegateHandle.IsValid())
	{
		const FName RendererModuleName("Renderer");
		IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
		if (RendererModule)
		{
			RenderDelegate.BindRaw(this, &FVdbMaterialRendering::Render_RenderThread);
			RenderDelegateHandle = RendererModule->RegisterPostOpaqueRenderDelegate(RenderDelegate);
		}
	}
}

void FVdbMaterialRendering::ReleaseDelegate()
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

void FVdbMaterialRendering::CreateMeshBatch(FMeshBatch& MeshBatch, const FVdbMaterialSceneProxy* PrimitiveProxy, FVdbVertexFactoryUserDataWrapper& UserData, const FMaterialRenderProxy* MaterialProxy) const
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
	BatchElement.PrimitiveUniformBuffer = PrimitiveProxy->GetUniformBuffer();
	BatchElement.IndexBuffer = &VertexBuffer->IndexBuffer;
	BatchElement.FirstIndex = 0;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = VertexBuffer->NumVertices - 1;
	BatchElement.NumPrimitives = VertexBuffer->NumPrimitives;
	BatchElement.VertexFactoryUserData = VertexFactory->GetUniformBuffer();
	BatchElement.UserData = &UserData;
}

void FVdbMaterialRendering::Render_RenderThread(FPostOpaqueRenderParameters& Parameters)
{
	if (!ShouldRenderVolumetricVdb())
		return;

	SCOPE_CYCLE_COUNTER(STAT_VdbRendering_RT);

	const FSceneView* View = static_cast<FSceneView*>(Parameters.Uid);

	TArray<FVdbMaterialSceneProxy*> OpaqueProxies = VdbProxies.FilterByPredicate([View](const FVdbMaterialSceneProxy* Proxy) { return !Proxy->IsTranslucent() && Proxy->IsVisible(View); });
	TArray<FVdbMaterialSceneProxy*> TranslucentProxies = VdbProxies.FilterByPredicate([View](const FVdbMaterialSceneProxy* Proxy) { return Proxy->IsTranslucent() && Proxy->IsVisible(View); });

	const FMatrix& ViewMat = View->ViewMatrices.GetViewMatrix();
	OpaqueProxies.Sort([ViewMat](const FVdbMaterialSceneProxy& Lhs, const FVdbMaterialSceneProxy& Rhs) -> bool 
		{ 
			const FVector& LeftProxyCenter = Lhs.GetBounds().GetSphere().Center;
			const FVector& RightProxyCenter = Rhs.GetBounds().GetSphere().Center;
			return ViewMat.TransformPosition(LeftProxyCenter).Z < ViewMat.TransformPosition(RightProxyCenter).Z; // front to back
		});
	TranslucentProxies.Sort([ViewMat](const FVdbMaterialSceneProxy& Lhs, const FVdbMaterialSceneProxy& Rhs) -> bool
		{
			const FVector& LeftProxyCenter = Lhs.GetBounds().GetSphere().Center;
			const FVector& RightProxyCenter = Rhs.GetBounds().GetSphere().Center;
			return ViewMat.TransformPosition(LeftProxyCenter).Z > ViewMat.TransformPosition(RightProxyCenter).Z; // back to front
		});

	auto DrawVdbProxies = [&](const TArray<FVdbMaterialSceneProxy*>& Proxies, bool Translucent, TRDGUniformBufferRef<FVdbShaderParams> VdbUniformBuffer, FRDGTexture* RenderTexture)
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
			Translucent ? RDG_EVENT_NAME("Vdb Translucent Rendering") : RDG_EVENT_NAME("Vdb Opaque Rendering"),
			PassParameters,
			ERDGPassFlags::Raster,
			[this, &InView = *View, ViewportRect = Parameters.ViewportRect, Proxies](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.SetViewport(ViewportRect.Min.X, ViewportRect.Min.Y, 0.0f, ViewportRect.Max.X, ViewportRect.Max.Y, 1.0f);
			RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

			for (const FVdbMaterialSceneProxy* Proxy : Proxies)
			{
				if (Proxy && Proxy->GetMaterial() && Proxy->IsVisible(&InView) && Proxy->GetDensityRenderResource())
				{
					DrawDynamicMeshPass(InView, RHICmdList,
						[&](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
						{
							FVdbElementData ShaderElementData;
							ShaderElementData.CustomIntData0 = Proxy->GetCustomIntData0();
							ShaderElementData.CustomFloatData0 = Proxy->GetCustomFloatData0();
							ShaderElementData.CustomFloatData1 = Proxy->GetCustomFloatData1();
							ShaderElementData.CustomFloatData2 = Proxy->GetCustomFloatData2();
							ShaderElementData.DensityBufferSRV = Proxy->GetDensityRenderResource()->GetBufferSRV();
							ShaderElementData.TemperatureBufferSRV = Proxy->GetTemperatureRenderResource() ? Proxy->GetTemperatureRenderResource()->GetBufferSRV() : nullptr;
							ShaderElementData.ColorBufferSRV = Proxy->GetColorRenderResource() ? Proxy->GetColorRenderResource()->GetBufferSRV() : nullptr;
							if (!ShaderElementData.DensityBufferSRV)
								return;

							const TStaticArray<FVdbRenderBuffer*, NUM_EXTRA_VDBS>& ExtraBuffers = Proxy->GetExtraRenderResources();
							for (uint32 idx = 0; idx < NUM_EXTRA_VDBS; ++idx)
							{
								ShaderElementData.ExtraBuffersSRV[idx] = ExtraBuffers[idx] ? ExtraBuffers[idx]->GetBufferSRV() : ShaderElementData.DensityBufferSRV;
							}

							FVdbMeshProcessor PassMeshProcessor(
								InView.Family->Scene->GetRenderScene(),
								&InView,
								DynamicMeshPassContext,
								Proxy->IsLevelSet(), Proxy->IsTranslucentLevelSet(),
								Proxy->UseImprovedSkylight(),
								ShaderElementData.TemperatureBufferSRV != nullptr,
								ShaderElementData.ColorBufferSRV != nullptr,
								Proxy->UseExtraRenderResources(),
								MoveTemp(ShaderElementData));

							FVdbVertexFactoryUserDataWrapper UserData;
							UserData.Data.IndexMin = Proxy->GetIndexMin() - ShaderElementData.CustomFloatData2.Y;
							UserData.Data.IndexSize = Proxy->GetIndexSize() + 2.0*ShaderElementData.CustomFloatData2.Y;
							UserData.Data.IndexToLocal = Proxy->GetIndexToLocal();

							FMeshBatch VolumeMesh;
							CreateMeshBatch(VolumeMesh, Proxy, UserData, Proxy->GetMaterial()->GetRenderProxy());

							const uint64 DefaultBatchElementMask = ~0ull; // or 1 << 0; // LOD 0 only
							PassMeshProcessor.AddMeshBatch(VolumeMesh, DefaultBatchElementMask, Proxy);
						}
					);
				}
			}
		});
	};

	FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;

	FVdbShaderParams* UniformParameters = GraphBuilder.AllocParameters<FVdbShaderParams>();
	UniformParameters->SceneDepthTexture = Parameters.DepthTexture;
	UniformParameters->Threshold = FMath::Max(0.0, CVarVolumetricVdbThreshold.GetValueOnAnyThread());
	TRDGUniformBufferRef<FVdbShaderParams> VdbUniformBuffer = GraphBuilder.CreateUniformBuffer(UniformParameters);

	if (!OpaqueProxies.IsEmpty())
	{
		SCOPE_CYCLE_COUNTER(STAT_VdbOpaque_RT);
		DrawVdbProxies(OpaqueProxies, false, VdbUniformBuffer, nullptr);
	}

	if (!TranslucentProxies.IsEmpty())
	{
		SCOPE_CYCLE_COUNTER(STAT_VdbTranslucent_RT);

		FRDGTextureDesc TexDesc = Parameters.ColorTexture->Desc;
		TexDesc.Format = PF_FloatRGBA; // force RGBA. Depending on quality settings, ColorTexture might not have alpha
		TexDesc.ClearValue = FClearValueBinding(FLinearColor::Transparent);
		FRDGTexture* VdbCurrRenderTexture = GraphBuilder.CreateTexture(TexDesc, TEXT("VdbRenderTexture"));

		DrawVdbProxies(TranslucentProxies, true, VdbUniformBuffer, VdbCurrRenderTexture);

		// Add optional post-processing (blurring, denoising etc.)
		EVdbDenoiserMethod Method = CVarVolumetricVdbDenoiser.GetValueOnAnyThread() >= 0 ?
			EVdbDenoiserMethod(CVarVolumetricVdbDenoiser.GetValueOnAnyThread()) : DenoiserMethod;
		FRDGTexture* DenoisedTex = VdbDenoiser::ApplyDenoising(
			GraphBuilder, VdbCurrRenderTexture, View, Parameters.ViewportRect, Method);

		// Composite VDB offscreen rendering onto back buffer
		VdbComposite::CompositeFullscreen(GraphBuilder, DenoisedTex, Parameters.ColorTexture, View);
	}
}

void FVdbMaterialRendering::AddVdbProxy(FVdbMaterialSceneProxy* Proxy)
{
	ENQUEUE_RENDER_COMMAND(FAddVdbProxyCommand)(
		[this, Proxy](FRHICommandListImmediate& RHICmdList)
		{
			check(VdbProxies.Find(Proxy) == INDEX_NONE);
			VdbProxies.Emplace(Proxy);
		});
}

void FVdbMaterialRendering::RemoveVdbProxy(FVdbMaterialSceneProxy* Proxy)
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

void FVdbMaterialRendering::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	// Reset visibility on all registered FVdbProxies, before SceneVisibility is computed 
	for (FVdbMaterialSceneProxy* Proxy : VdbProxies)
	{
		Proxy->ResetVisibility();
	}
}
