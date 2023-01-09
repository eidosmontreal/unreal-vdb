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
#include "SceneTexturesConfig.h"

#include "LocalVertexFactory.h"
#include "MeshPassProcessor.h"
#include "MeshPassProcessor.inl"
#include "Modules\ModuleManager.h"
#include "SceneView.h"
#include "ScenePrivate.h"

DEFINE_LOG_CATEGORY(LogSparseVolumetrics);

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
		bool TrilinearSampling,
		bool UseTempVdb, bool UseColorVdb,
		FVdbElementData&& ShaderElementData)
		: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InView, InDrawListContext)
		, VdbShaderElementData(ShaderElementData)
		, bLevelSet(IsLevelSet)
		, bTranslucentLevelSet(IsTranslucentLevelSet)
		, bImprovedSkylight(ImprovedSkylight)
		, bTrilinearSampling(TrilinearSampling)
		, bTemperatureVdb(UseTempVdb)
		, bColorVdb(UseColorVdb)
	{
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

		int32 CinematicMode = FVdbCVars::CVarVolumetricVdbCinematicQuality.GetValueOnAnyThread();
		if (CinematicMode == 1)
		{
			VdbShaderElementData.CustomFloatData0[0] /= 4.f; // local step size
			VdbShaderElementData.CustomFloatData0[1] = FMath::Max(1.f, VdbShaderElementData.CustomFloatData0[1] / 4.f); // local shadow step size
			VdbShaderElementData.CustomIntData0[0] *= 2; // Max number of steps
			VdbShaderElementData.CustomIntData0[1] *= 2; // Samples per pixels
		}
		else if (CinematicMode == 2)
		{
			VdbShaderElementData.CustomFloatData0[0] /= 10.f; // local step size
			VdbShaderElementData.CustomFloatData0[1] = FMath::Max(1.f, VdbShaderElementData.CustomFloatData0[1] / 10.f); // local shadow step size
			VdbShaderElementData.CustomIntData0[0] *= 4; // Max number of steps
			VdbShaderElementData.CustomIntData0[1] *= 4; // Samples per pixels
			bTrilinearSampling = true;
		}
	}

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final
	{
		const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
		const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);

		if (Material && Material->GetMaterialDomain() == MD_Volume && Material->GetRenderingThreadShaderMap())
		{
			const ERasterizerFillMode MeshFillMode = FM_Solid;
			const ERasterizerCullMode MeshCullMode = CM_CCW;

			#define PROCESS_SHADER(shader) { Process<FVdbShaderVS, ##shader>(MeshBatch, BatchElementMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, StaticMeshId, MeshFillMode, MeshCullMode); }

			if (bLevelSet)
			{
				if (bTranslucentLevelSet && bImprovedSkylight)
					PROCESS_SHADER(FVdbShaderPS_LevelSet_Translucent_EnvLight)
				else if (bTranslucentLevelSet)
					PROCESS_SHADER(FVdbShaderPS_LevelSet_Translucent)
				else
					PROCESS_SHADER(FVdbShaderPS_LevelSet)
			}
			else
			{
				// combination of 4 params: 2^4 = 16 different cases
				// TODO: this is getting ridiculous, find better solution
				if (!bTemperatureVdb && !bColorVdb && !bImprovedSkylight && !bTrilinearSampling)
					PROCESS_SHADER(FVdbShaderPS_FogVolume)
				else if (!bTemperatureVdb && !bColorVdb && !bImprovedSkylight && bTrilinearSampling)
					PROCESS_SHADER(FVdbShaderPS_FogVolume_Trilinear)
				else if (!bTemperatureVdb && !bColorVdb && bImprovedSkylight && !bTrilinearSampling)
					PROCESS_SHADER(FVdbShaderPS_FogVolume_EnvLight)
				else if (!bTemperatureVdb && !bColorVdb && bImprovedSkylight && bTrilinearSampling)
					PROCESS_SHADER(FVdbShaderPS_FogVolume_EnvLight_Trilinear)
				else if (!bTemperatureVdb && bColorVdb && !bImprovedSkylight && !bTrilinearSampling)
					PROCESS_SHADER(FVdbShaderPS_FogVolume_Color)
				else if (!bTemperatureVdb && bColorVdb && !bImprovedSkylight && bTrilinearSampling)
					PROCESS_SHADER(FVdbShaderPS_FogVolume_Color_Trilinear)
				else if (!bTemperatureVdb && bColorVdb && bImprovedSkylight && !bTrilinearSampling)
					PROCESS_SHADER(FVdbShaderPS_FogVolume_Color_EnvLight)
				else if (!bTemperatureVdb && bColorVdb && bImprovedSkylight && bTrilinearSampling)
					PROCESS_SHADER(FVdbShaderPS_FogVolume_Color_EnvLight_Trilinear)
				else if (bTemperatureVdb && !bColorVdb && !bImprovedSkylight && !bTrilinearSampling)
					PROCESS_SHADER(FVdbShaderPS_FogVolume_Blackbody)
				else if (bTemperatureVdb && !bColorVdb && !bImprovedSkylight && bTrilinearSampling)
					PROCESS_SHADER(FVdbShaderPS_FogVolume_Blackbody_Trilinear)
				else if (bTemperatureVdb && !bColorVdb && bImprovedSkylight && !bTrilinearSampling)
					PROCESS_SHADER(FVdbShaderPS_FogVolume_Blackbody_EnvLight)
				else if (bTemperatureVdb && !bColorVdb && bImprovedSkylight && bTrilinearSampling)
					PROCESS_SHADER(FVdbShaderPS_FogVolume_Blackbody_EnvLight_Trilinear)
				else if (bTemperatureVdb && bColorVdb && !bImprovedSkylight && !bTrilinearSampling)
					PROCESS_SHADER(FVdbShaderPS_FogVolume_Blackbody_Color)
				else if (bTemperatureVdb && bColorVdb && !bImprovedSkylight && bTrilinearSampling)
					PROCESS_SHADER(FVdbShaderPS_FogVolume_Blackbody_Color_Trilinear)
				else if (bTemperatureVdb && bColorVdb && bImprovedSkylight && !bTrilinearSampling)
					PROCESS_SHADER(FVdbShaderPS_FogVolume_Blackbody_Color_EnvLight)
				else if (bTemperatureVdb && bColorVdb && bImprovedSkylight && bTrilinearSampling)
					PROCESS_SHADER(FVdbShaderPS_FogVolume_Blackbody_Color_EnvLight_Trilinear)
			}
		}
	}

private:

	template<typename VertexShaderType, typename PixelShaderType>
	bool GetPassShaders(
		const FMaterial& Material,
		FVertexFactoryType* VertexFactoryType,
		TShaderRef<VertexShaderType>& VertexShader,
		TShaderRef<PixelShaderType>& PixelShader
	)
	{
		FMaterialShaderTypes ShaderTypes;
		ShaderTypes.AddShaderType<VertexShaderType>();
		ShaderTypes.AddShaderType<PixelShaderType>();

		FMaterialShaders Shaders;
		if (!Material.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
		{
			return false;
		}

		Shaders.TryGetVertexShader(VertexShader);
		Shaders.TryGetPixelShader(PixelShader);

		return VertexShader.IsValid() && PixelShader.IsValid();
	}

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
		if (!GetPassShaders(
			MaterialResource,
			VertexFactory->GetType(),
			PassShaders.VertexShader,
			PassShaders.PixelShader))
		{
			return;
		}


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
	bool bTrilinearSampling;
	bool bTemperatureVdb;
	bool bColorVdb;
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
	return FVdbCVars::CVarVolumetricVdb.GetValueOnRenderThread() && VertexFactory.IsValid();
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

void FVdbMaterialRendering::Init(UTextureRenderTarget2D* DefaultRenderTarget)
{
	if (IsInRenderingThread())
	{
		DefaultVdbRenderTarget = DefaultRenderTarget;
		InitRendering();
	}
	else
	{
		ENQUEUE_RENDER_COMMAND(InitVdbRendering)(
			[this, DefaultRenderTarget](FRHICommandListImmediate& RHICmdList)
			{
				Init(DefaultRenderTarget);
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

			// Render VDBs before or after Transparent objects
			if (FVdbCVars::CVarVolumetricVdbAfterTransparents.GetValueOnRenderThread())
			{
				RenderDelegateHandle = RendererModule->RegisterOverlayRenderDelegate(RenderDelegate);
			}
			else
			{
				RenderDelegateHandle = RendererModule->RegisterPostOpaqueRenderDelegate(RenderDelegate);
			}
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
		PassParameters->View = View->ViewUniformBuffer;
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

			FRHITextureViewCache TexCache;

			for (const FVdbMaterialSceneProxy* Proxy : Proxies)
			{
				if (Proxy && Proxy->GetMaterial() && Proxy->IsVisible(&InView) && Proxy->GetDensityRenderResource())
				{
					DrawDynamicMeshPass(InView, RHICmdList,
						[&](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
						{
							FVdbElementData ShaderElementData;
							ShaderElementData.CustomIntData0 = Proxy->GetCustomIntData0();
							ShaderElementData.CustomIntData1 = Proxy->GetCustomIntData1();
							ShaderElementData.CustomFloatData0 = Proxy->GetCustomFloatData0();
							ShaderElementData.CustomFloatData1 = Proxy->GetCustomFloatData1();
							ShaderElementData.CustomFloatData2 = Proxy->GetCustomFloatData2();
							ShaderElementData.DensityBufferSRV = Proxy->GetDensityRenderResource()->GetBufferSRV();
							ShaderElementData.TemperatureBufferSRV = Proxy->GetTemperatureRenderResource() ? Proxy->GetTemperatureRenderResource()->GetBufferSRV() : nullptr;
							ShaderElementData.ColorBufferSRV = Proxy->GetColorRenderResource() ? Proxy->GetColorRenderResource()->GetBufferSRV() : nullptr;
							if (!ShaderElementData.DensityBufferSRV)
								return;

							FTexture* CurveAtlas = Proxy->GetBlackbodyAtlasResource();
							FTextureRHIRef CurveAtlasRHI = CurveAtlas ? CurveAtlas->GetTextureRHI() : nullptr;
							ShaderElementData.BlackbodyColorSRV = CurveAtlasRHI ? TexCache.GetOrCreateSRV(CurveAtlasRHI, FRHITextureSRVCreateInfo()) : GBlackTextureWithSRV->ShaderResourceViewRHI;

							FVdbMeshProcessor PassMeshProcessor(
								InView.Family->Scene->GetRenderScene(),
								&InView,
								DynamicMeshPassContext,
								Proxy->IsLevelSet(), Proxy->IsTranslucentLevelSet(),
								Proxy->UseImprovedSkylight(),
								Proxy->UseTrilinearSampling() || FVdbCVars::CVarVolumetricVdbTrilinear.GetValueOnRenderThread(),
								ShaderElementData.TemperatureBufferSRV != nullptr,
								ShaderElementData.ColorBufferSRV != nullptr,
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
	UniformParameters->Threshold = FMath::Max(0.0, FVdbCVars::CVarVolumetricVdbThreshold.GetValueOnAnyThread());
	UniformParameters->LinearTexSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	TRDGUniformBufferRef<FVdbShaderParams> VdbUniformBuffer = GraphBuilder.CreateUniformBuffer(UniformParameters);

	if (!OpaqueProxies.IsEmpty())
	{
		SCOPE_CYCLE_COUNTER(STAT_VdbOpaque_RT);
		DrawVdbProxies(OpaqueProxies, false, VdbUniformBuffer, nullptr);
	}

	if (!TranslucentProxies.IsEmpty())
	{
		SCOPE_CYCLE_COUNTER(STAT_VdbTranslucent_RT);

		FRDGTexture* VdbCurrRenderTexture = nullptr;
		if (DefaultVdbRenderTargetTex && DefaultVdbRenderTargetTex->GetTextureRHI())
		{
			VdbCurrRenderTexture = RegisterExternalTexture(GraphBuilder, DefaultVdbRenderTargetTex->GetTextureRHI(), TEXT("VdbRenderTarget"));
		}
		else
		{
			FRDGTextureDesc TexDesc = Parameters.ColorTexture->Desc;
			TexDesc.Format = PF_FloatRGBA; // force RGBA. Depending on quality settings, ColorTexture might not have alpha
			TexDesc.ClearValue = FClearValueBinding(FLinearColor::Transparent);
			VdbCurrRenderTexture = GraphBuilder.CreateTexture(TexDesc, TEXT("VdbRenderTexture"));
		}

		DrawVdbProxies(TranslucentProxies, true, VdbUniformBuffer, VdbCurrRenderTexture);

		// Add optional post-processing (blurring, denoising etc.)
		EVdbDenoiserMethod Method = FVdbCVars::CVarVolumetricVdbDenoiser.GetValueOnAnyThread() >= 0 ?
			EVdbDenoiserMethod(FVdbCVars::CVarVolumetricVdbDenoiser.GetValueOnAnyThread()) : DenoiserMethod;
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

void FVdbMaterialRendering::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	// Reset visibility on all registered FVdbProxies, before SceneVisibility is computed 
	for (FVdbMaterialSceneProxy* Proxy : VdbProxies)
	{
		Proxy->ResetVisibility();
		Proxy->UpdateCurveAtlasTex();
	}
}

// Called on game thread when view family is about to be rendered.
void FVdbMaterialRendering::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (DefaultVdbRenderTarget)
	{
		if (const FRenderTarget* RefRenderTarget = InViewFamily.RenderTarget)
		{
			const FSceneTexturesConfig& Config = FSceneTexturesConfig::Get();
			if ((Config.Extent.X != DefaultVdbRenderTarget->SizeX ||
				Config.Extent.Y != DefaultVdbRenderTarget->SizeY ||
				DefaultVdbRenderTarget->RenderTargetFormat != RTF_RGBA16f) &&
				(Config.Extent.X > 0 && Config.Extent.Y > 0))
			{
				DefaultVdbRenderTarget->ClearColor = FLinearColor::Transparent;
				DefaultVdbRenderTarget->InitCustomFormat(Config.Extent.X, Config.Extent.Y, PF_FloatRGBA, true);
				DefaultVdbRenderTarget->UpdateResourceImmediate(true);
			}
		}

		DefaultVdbRenderTargetTex = DefaultVdbRenderTarget->GetResource();
	}
	else
	{
		DefaultVdbRenderTargetTex = nullptr;
	}
}
