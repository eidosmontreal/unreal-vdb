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

#include "VdbComposite.h"

#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "PixelShaderUtils.h"
#include "SceneRenderTargetParameters.h"
#include "SceneView.h"

static TAutoConsoleVariable<int32> CVarVdbCompositeDebugMode(
	TEXT("r.Vdb.DebugMode"), 0,
	TEXT("Display VolumetricVdb debug mode. If <= 0, ignore. 1: show Radiance only, 2: show Throughput only."),
	ECVF_RenderThreadSafe);

class FCompositePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCompositePS);
	SHADER_USE_PARAMETER_STRUCT(FCompositePS, FGlobalShader);

	class FDisplayMethod : SHADER_PERMUTATION_INT("DEBUG_DISPLAY", 3);
	using FPermutationDomain = TShaderPermutationDomain<FDisplayMethod>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_SHADER_TYPE(, FCompositePS, TEXT("/Plugin/VdbVolume/Private/VdbComposite.usf"), TEXT("MainPS"), SF_Pixel)

void VdbComposite::CompositeFullscreen(FRDGBuilder& GraphBuilder, FRDGTexture* InputTexture, FRDGTexture* OutTexture, const FSceneView* View)
{
	FIntRect Viewport(FIntPoint(0, 0), OutTexture->Desc.Extent);

	FCompositePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCompositePS::FParameters>();
	PassParameters->ViewUniformBuffer = View->ViewUniformBuffer;
	PassParameters->InputTexture = InputTexture;
	PassParameters->InputSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->RenderTargets[0] = FRenderTargetBinding(OutTexture, ERenderTargetLoadAction::ELoad);

	int DebugDisplayMode = CVarVdbCompositeDebugMode.GetValueOnRenderThread();
	DebugDisplayMode = FMath::Clamp(DebugDisplayMode, 0, 2);
	FCompositePS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FCompositePS::FDisplayMethod>(DebugDisplayMode);

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FCompositePS> PixelShader(GlobalShaderMap, PermutationVector);

	FRHIBlendState* BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		GlobalShaderMap,
		RDG_EVENT_NAME("VdbRendering.Composite %dx%d (PS)", Viewport.Width(), Viewport.Height()),
		PixelShader,
		PassParameters,
		Viewport,
		BlendState);
}