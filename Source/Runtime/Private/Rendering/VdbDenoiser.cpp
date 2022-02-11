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

#include "VdbDenoiser.h"

#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "ShaderParameters.h"
#include "SceneView.h"
#include "ShaderParameterStruct.h"

// FogVolumes Vdb Denoiser (right now, it is just a simple blur proof of concept)
class FDenoiseCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDenoiseCS);
	SHADER_USE_PARAMETER_STRUCT(FDenoiseCS, FGlobalShader);

	class FDenoiserMethod : SHADER_PERMUTATION_INT("METHOD", (int)EVdbDenoiserMethod::Count);
	using FPermutationDomain = TShaderPermutationDomain<FDenoiserMethod>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), ThreadGroupSize);
	}

	static const int ThreadGroupSize = 32;
};

IMPLEMENT_GLOBAL_SHADER(FDenoiseCS, "/Plugin/VdbVolume/Private/VdbDenoiser.usf", "MainCS", SF_Compute);

FRDGTexture* VdbDenoiser::ApplyDenoising(FRDGBuilder& GraphBuilder, FRDGTexture* InputTexture, const FSceneView* View, const FIntRect& ViewportRect, EVdbDenoiserMethod Method)
{
	if (Method == EVdbDenoiserMethod::None || Method >= EVdbDenoiserMethod::Count)
		return InputTexture;

	FRDGTexture* VdbDenoiseRenderTexture = GraphBuilder.CreateTexture(InputTexture->Desc, TEXT("VdbDenoiseRenderTexture"));
	FRDGTextureUAV* TexUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(VdbDenoiseRenderTexture));

	auto* PassParameters = GraphBuilder.AllocParameters<FDenoiseCS::FParameters>();
	PassParameters->View = View->ViewUniformBuffer;
	PassParameters->InputTexture = InputTexture;
	PassParameters->OutputTexture = TexUAV;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Vdb FogVolumes Denoiser"),
		PassParameters,
		ERDGPassFlags::Compute,
		[ViewportRect, PassParameters, Method](FRHICommandList& RHICmdList)
		{
			FDenoiseCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FDenoiseCS::FDenoiserMethod>((int)Method);

			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FDenoiseCS> VdbShader(GlobalShaderMap, PermutationVector);

			FIntVector DispatchCount = FComputeShaderUtils::GetGroupCount(ViewportRect.Size(), FDenoiseCS::ThreadGroupSize);
			FComputeShaderUtils::Dispatch(RHICmdList, VdbShader, *PassParameters, DispatchCount);
		});

	return VdbDenoiseRenderTexture;
}