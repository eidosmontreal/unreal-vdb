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

#include "MeshMaterialShader.h"

THIRD_PARTY_INCLUDES_START
#include <nanovdb/NanoVDB.h>
THIRD_PARTY_INCLUDES_END

namespace VdbShaders
{
	static bool IsSupportedVertexFactoryType(const FVertexFactoryType* VertexFactoryType)
	{
		static FName VdbVfName = FName(TEXT("FCubeMeshVertexFactory"), FNAME_Find);
		return VertexFactoryType == FindVertexFactoryType(VdbVfName);
	}
}

struct FVdbElementData : public FMeshMaterialShaderElementData
{
	float DensityMultiplier;
	float StepMultiplier;
	FShaderResourceViewRHIRef BufferSRV;
};

class FVdbShaderVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FVdbShaderVS , MeshMaterial);

	FVdbShaderVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{}

	FVdbShaderVS() = default;

public:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
			Parameters.MaterialParameters.MaterialDomain == MD_Volume &&
			FMeshMaterialShader::ShouldCompilePermutation(Parameters) &&
			VdbShaders::IsSupportedVertexFactoryType(Parameters.VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};


BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FVdbShaderParams, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
	SHADER_PARAMETER(uint32, SamplesPerPixel)
	SHADER_PARAMETER(uint32, MaxRayDepth)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FVdbShaderParametersPS, )
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVdbShaderParams, VdbUniformBuffer)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

template<bool IsLevelSet>
class FVdbShaderPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FVdbShaderPS, MeshMaterial);

	LAYOUT_FIELD(FShaderResourceParameter, VdbBuffer);
	LAYOUT_FIELD(FShaderParameter, DensityMultiplier);
	LAYOUT_FIELD(FShaderParameter, StepMultiplier);

	FVdbShaderPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		VdbBuffer.Bind(Initializer.ParameterMap, TEXT("VdbBuffer"));
		DensityMultiplier.Bind(Initializer.ParameterMap, TEXT("DensityMult"));
		StepMultiplier.Bind(Initializer.ParameterMap, TEXT("StepMultiplier"));

		PassUniformBuffer.Bind(Initializer.ParameterMap, FVdbShaderParams::StaticStructMetadata.GetShaderVariableName());
	}

	FVdbShaderPS() {}

public:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return FVdbShaderVS::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("VDB_LEVEL_SET"), IsLevelSet);
		OutEnvironment.SetDefine(TEXT("SHADER_VERSION_MAJOR"), NANOVDB_MAJOR_VERSION_NUMBER);
		OutEnvironment.SetDefine(TEXT("SHADER_VERSION_MINOR"), NANOVDB_MINOR_VERSION_NUMBER);
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FVdbElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);

		ShaderBindings.Add(VdbBuffer, ShaderElementData.BufferSRV);
		ShaderBindings.Add(DensityMultiplier, ShaderElementData.DensityMultiplier);
		ShaderBindings.Add(StepMultiplier, ShaderElementData.StepMultiplier);
	}
};
typedef FVdbShaderPS<true> FVdbShaderPS_LevelSet;
typedef FVdbShaderPS<false>  FVdbShaderPS_FogVolume;


//-----------------------------------------------------------------------------

BEGIN_UNIFORM_BUFFER_STRUCT(FVdbResearchShaderParams, )
	// Volume properties
	SHADER_PARAMETER_SRV(StructuredBuffer<uint>, VdbDensity)
	SHADER_PARAMETER_SRV(StructuredBuffer<uint>, VdbTemperature)
	SHADER_PARAMETER(FVector, VolumeScale)
	SHADER_PARAMETER(FVector, VolumeTranslation)
	SHADER_PARAMETER(FMatrix, LocalToWorld)
	SHADER_PARAMETER(FMatrix, WorldToLocal)
	SHADER_PARAMETER(uint32, SamplesPerPixel)
	SHADER_PARAMETER(uint32, MaxRayDepth)
	// Material parameters
	SHADER_PARAMETER(FVector, Color)
	SHADER_PARAMETER(float, DensityMult)
	SHADER_PARAMETER(float, Albedo)
	SHADER_PARAMETER(float, Anisotropy)
	SHADER_PARAMETER(FVector, EmissionColor)
	SHADER_PARAMETER(float, EmissionStrength)
	SHADER_PARAMETER(FVector, BlackbodyTint)
	SHADER_PARAMETER(float, BlackbodyIntensity)
	SHADER_PARAMETER(float, Temperature)
END_UNIFORM_BUFFER_STRUCT()

class FVdbResearchVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVdbResearchVS);
	SHADER_USE_PARAMETER_STRUCT(FVdbResearchVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVdbResearchShaderParams, VdbGlobalParams)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_VERTEX"), 1);
		OutEnvironment.SetDefine(TEXT("SHADER_VERSION_MAJOR"), NANOVDB_MAJOR_VERSION_NUMBER);
		OutEnvironment.SetDefine(TEXT("SHADER_VERSION_MINOR"), NANOVDB_MINOR_VERSION_NUMBER);
	}
};

class FVdbResearchPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVdbResearchPS);
	SHADER_USE_PARAMETER_STRUCT(FVdbResearchPS, FGlobalShader)

	class FPathTracing : SHADER_PERMUTATION_BOOL("PATH_TRACING");
	class FUseTemperature : SHADER_PERMUTATION_BOOL("USE_TEMPERATURE");
	using FPermutationDomain = TShaderPermutationDomain<FPathTracing, FUseTemperature>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Scene / Unreal data
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		// VdbRendering data
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevAccumTex)
		SHADER_PARAMETER(uint32, NumAccumulations)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVdbResearchShaderParams, VdbGlobalParams)
		// Debug
		SHADER_PARAMETER(uint32, DisplayBounds)
		SHADER_PARAMETER(uint32, DisplayUnfinishedPaths)
		// Render Target
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5); 
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_PIXEL"), 1);
		OutEnvironment.SetDefine(TEXT("SHADER_VERSION_MAJOR"), NANOVDB_MAJOR_VERSION_NUMBER);
		OutEnvironment.SetDefine(TEXT("SHADER_VERSION_MINOR"), NANOVDB_MINOR_VERSION_NUMBER);
	}
};
