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

#include "VdbCommon.h"
#include "MeshMaterialShader.h"
#include "InstanceCulling/InstanceCullingMergedContext.h"

THIRD_PARTY_INCLUDES_START
#include <nanovdb/NanoVDB.h>
THIRD_PARTY_INCLUDES_END

namespace VdbShaders
{
	static bool IsSupportedVertexFactoryType(const FVertexFactoryType* VertexFactoryType)
	{
		static FName VdbVfName = FName(TEXT("FVolumeMeshVertexFactory"), FNAME_Find);
		return VertexFactoryType == FindVertexFactoryType(VdbVfName);
	}
}

struct FVdbElementData : public FMeshMaterialShaderElementData
{
	FIntVector4 CustomIntData0; // x: MaxRayDepth, y: SamplesPerPixel, z: colored transmittance, w: temporal noise
	FVector4f CustomFloatData0; // x: Local step size, y: Shadow step size mutliplier, z: voxel size, w: jittering
	FVector4f CustomFloatData1; // x: anisotropy, y: albedo, z: blackbody intensity, w: blackbody temperature
	FVector4f CustomFloatData2; // x: density mul, y: padding, z: ambient, w: unused
	FShaderResourceViewRHIRef DensityBufferSRV;
	FShaderResourceViewRHIRef TemperatureBufferSRV;
	FShaderResourceViewRHIRef ColorBufferSRV;
	TStaticArray<FShaderResourceViewRHIRef, NUM_EXTRA_VDBS> ExtraBuffersSRV;
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
	SHADER_PARAMETER(float, Threshold)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FVdbShaderParametersPS, )
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVdbShaderParams, VdbUniformBuffer)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FInstanceCullingGlobalUniforms, InstanceCulling)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

template<bool IsLevelSet, bool UseTemperatureBuffer, bool UseColorBuffer, bool UseExtraBuffers, bool NicerEnvLight>
class FVdbShaderPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FVdbShaderPS, MeshMaterial);

	LAYOUT_FIELD(FShaderResourceParameter, DensityVdbBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, TemperatureVdbBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, ColorVdbBuffer);
	LAYOUT_FIELD(FShaderParameter, CustomIntData0);
	LAYOUT_FIELD(FShaderParameter, CustomFloatData0);
	LAYOUT_FIELD(FShaderParameter, CustomFloatData1);
	LAYOUT_FIELD(FShaderParameter, CustomFloatData2);

	LAYOUT_FIELD(FShaderResourceParameter, ExtraVdbFloatBuffer1);
	LAYOUT_FIELD(FShaderResourceParameter, ExtraVdbFloatBuffer2);
	LAYOUT_FIELD(FShaderResourceParameter, ExtraVdbFloatBuffer3);
	LAYOUT_FIELD(FShaderResourceParameter, ExtraVdbFloatBuffer4);
	LAYOUT_FIELD(FShaderResourceParameter, ExtraVdbVectorBuffer1);
	LAYOUT_FIELD(FShaderResourceParameter, ExtraVdbVectorBuffer2);
	LAYOUT_FIELD(FShaderResourceParameter, ExtraVdbVectorBuffer3);
	LAYOUT_FIELD(FShaderResourceParameter, ExtraVdbVectorBuffer4);

	FVdbShaderPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		DensityVdbBuffer.Bind(Initializer.ParameterMap, TEXT("DensityVdbBuffer"));
		TemperatureVdbBuffer.Bind(Initializer.ParameterMap, TEXT("TemperatureVdbBuffer"));
		ColorVdbBuffer.Bind(Initializer.ParameterMap, TEXT("ColorVdbBuffer"));
		CustomIntData0.Bind(Initializer.ParameterMap, TEXT("CustomIntData0"));
		CustomFloatData0.Bind(Initializer.ParameterMap, TEXT("CustomFloatData0"));
		CustomFloatData1.Bind(Initializer.ParameterMap, TEXT("CustomFloatData1"));
		CustomFloatData2.Bind(Initializer.ParameterMap, TEXT("CustomFloatData2"));

		ExtraVdbFloatBuffer1.Bind(Initializer.ParameterMap, TEXT("ExtraVdbFloatBuffer1"));
		ExtraVdbFloatBuffer2.Bind(Initializer.ParameterMap, TEXT("ExtraVdbFloatBuffer2"));
		ExtraVdbFloatBuffer3.Bind(Initializer.ParameterMap, TEXT("ExtraVdbFloatBuffer3"));
		ExtraVdbFloatBuffer4.Bind(Initializer.ParameterMap, TEXT("ExtraVdbFloatBuffer4"));
		ExtraVdbVectorBuffer1.Bind(Initializer.ParameterMap, TEXT("ExtraVdbVectorBuffer1"));
		ExtraVdbVectorBuffer2.Bind(Initializer.ParameterMap, TEXT("ExtraVdbVectorBuffer2"));
		ExtraVdbVectorBuffer3.Bind(Initializer.ParameterMap, TEXT("ExtraVdbVectorBuffer3"));
		ExtraVdbVectorBuffer4.Bind(Initializer.ParameterMap, TEXT("ExtraVdbVectorBuffer4"));

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
		OutEnvironment.SetDefine(TEXT("USE_TEMPERATURE_VDB"), UseTemperatureBuffer);
		OutEnvironment.SetDefine(TEXT("USE_COLOR_VDB"), UseColorBuffer);
		OutEnvironment.SetDefine(TEXT("USE_EXTRA_VDBS"), UseExtraBuffers);
		OutEnvironment.SetDefine(TEXT("NICER_BUT_EXPENSIVE_ENVLIGHT"), NicerEnvLight);
		OutEnvironment.SetDefine(TEXT("USE_FORCE_TEXTURE_MIP"), TEXT("1"));
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

		ShaderBindings.Add(DensityVdbBuffer, ShaderElementData.DensityBufferSRV);
		ShaderBindings.Add(TemperatureVdbBuffer, ShaderElementData.TemperatureBufferSRV);
		ShaderBindings.Add(ColorVdbBuffer, ShaderElementData.ColorBufferSRV);
		ShaderBindings.Add(CustomIntData0, ShaderElementData.CustomIntData0);
		ShaderBindings.Add(CustomFloatData0, ShaderElementData.CustomFloatData0);
		ShaderBindings.Add(CustomFloatData1, ShaderElementData.CustomFloatData1);
		ShaderBindings.Add(CustomFloatData2, ShaderElementData.CustomFloatData2);

		ShaderBindings.Add(ExtraVdbFloatBuffer1, ShaderElementData.ExtraBuffersSRV[0]);
		ShaderBindings.Add(ExtraVdbFloatBuffer2, ShaderElementData.ExtraBuffersSRV[1]);
		ShaderBindings.Add(ExtraVdbFloatBuffer3, ShaderElementData.ExtraBuffersSRV[2]);
		ShaderBindings.Add(ExtraVdbFloatBuffer4, ShaderElementData.ExtraBuffersSRV[3]);
		ShaderBindings.Add(ExtraVdbVectorBuffer1, ShaderElementData.ExtraBuffersSRV[4]);
		ShaderBindings.Add(ExtraVdbVectorBuffer2, ShaderElementData.ExtraBuffersSRV[5]);
		ShaderBindings.Add(ExtraVdbVectorBuffer3, ShaderElementData.ExtraBuffersSRV[6]);
		ShaderBindings.Add(ExtraVdbVectorBuffer4, ShaderElementData.ExtraBuffersSRV[7]);
	}
};
// TODO: this is getting ridiculous. Find other solution
typedef FVdbShaderPS<true, false, false, false, false> FVdbShaderPS_LevelSet;
typedef FVdbShaderPS<true, true, false, false, false> FVdbShaderPS_LevelSet_Translucent; // reusing USE_TEMPERATURE_VDB variation for translucency to avoid another variation
typedef FVdbShaderPS<true, true, false, false, true> FVdbShaderPS_LevelSet_Translucent_EnvLight; // reusing USE_TEMPERATURE_VDB variation for translucency to avoid another variation
typedef FVdbShaderPS<false, false, false, false, false>  FVdbShaderPS_FogVolume;
typedef FVdbShaderPS<false, false, false, false, true>  FVdbShaderPS_FogVolume_EnvLight;
typedef FVdbShaderPS<false, false, true, false, false>  FVdbShaderPS_FogVolume_Color;
typedef FVdbShaderPS<false, false, true, false, true>  FVdbShaderPS_FogVolume_Color_EnvLight;
typedef FVdbShaderPS<false, false, false, true, false>  FVdbShaderPS_FogVolume_Extra;
typedef FVdbShaderPS<false, false, false, true, true>  FVdbShaderPS_FogVolume_Extra_EnvLight;
typedef FVdbShaderPS<false, false, true, true, false>  FVdbShaderPS_FogVolume_Color_Extra;
typedef FVdbShaderPS<false, false, true, true, true>  FVdbShaderPS_FogVolume_Color_Extra_EnvLight;
typedef FVdbShaderPS<false, true, false, false, false>  FVdbShaderPS_FogVolume_Blackbody;
typedef FVdbShaderPS<false, true, false, false, true>  FVdbShaderPS_FogVolume_Blackbody_EnvLight;
typedef FVdbShaderPS<false, true, true, false, false>  FVdbShaderPS_FogVolume_Blackbody_Color;
typedef FVdbShaderPS<false, true, true, false, true>  FVdbShaderPS_FogVolume_Blackbody_Color_EnvLight;
typedef FVdbShaderPS<false, true, false, true, false>  FVdbShaderPS_FogVolume_Blackbody_Extra;
typedef FVdbShaderPS<false, true, false, true, true>  FVdbShaderPS_FogVolume_Blackbody_Extra_EnvLight;
typedef FVdbShaderPS<false, true, true, true, false>  FVdbShaderPS_FogVolume_Blackbody_Color_Extra;
typedef FVdbShaderPS<false, true, true, true, true>  FVdbShaderPS_FogVolume_Blackbody_Color_Extra_EnvLight;


//-----------------------------------------------------------------------------

BEGIN_UNIFORM_BUFFER_STRUCT(FVdbPrincipledShaderParams, )
	// Volume properties
	SHADER_PARAMETER_SRV(StructuredBuffer<uint>, VdbDensity)
	SHADER_PARAMETER_SRV(StructuredBuffer<uint>, VdbTemperature)
	SHADER_PARAMETER_SRV(StructuredBuffer<uint>, VdbColor)
	SHADER_PARAMETER_SRV(StructuredBuffer<uint>, ExtraVdbFloatBuffer1)
	SHADER_PARAMETER_SRV(StructuredBuffer<uint>, ExtraVdbFloatBuffer2)
	SHADER_PARAMETER_SRV(StructuredBuffer<uint>, ExtraVdbFloatBuffer3)
	SHADER_PARAMETER_SRV(StructuredBuffer<uint>, ExtraVdbFloatBuffer4)
	SHADER_PARAMETER_SRV(StructuredBuffer<uint>, ExtraVdbVectorBuffer1)
	SHADER_PARAMETER_SRV(StructuredBuffer<uint>, ExtraVdbVectorBuffer2)
	SHADER_PARAMETER_SRV(StructuredBuffer<uint>, ExtraVdbVectorBuffer3)
	SHADER_PARAMETER_SRV(StructuredBuffer<uint>, ExtraVdbVectorBuffer4)

	SHADER_PARAMETER(FVector3f, VolumeScale)
	SHADER_PARAMETER(FVector3f, VolumeTranslation)
	SHADER_PARAMETER(FMatrix44f, VolumeToLocal)
	SHADER_PARAMETER(FMatrix44f, LocalToWorld)
	SHADER_PARAMETER(FMatrix44f, WorldToLocal)
	SHADER_PARAMETER(uint32, SamplesPerPixel)
	SHADER_PARAMETER(uint32, MaxRayDepth)
	SHADER_PARAMETER(float, StepSize)
	SHADER_PARAMETER(float, VoxelSize)
	SHADER_PARAMETER(uint32, ColoredTransmittance)
	SHADER_PARAMETER(uint32, TemporalNoise)
	// Material parameters
	SHADER_PARAMETER(FVector3f, Color)
	SHADER_PARAMETER(float, DensityMult)
	SHADER_PARAMETER(float, Albedo)
	SHADER_PARAMETER(float, Ambient)
	SHADER_PARAMETER(float, Anisotropy)
	SHADER_PARAMETER(float, EmissionStrength)
	SHADER_PARAMETER(FVector3f, EmissionColor)
	SHADER_PARAMETER(FVector3f, BlackbodyTint)
	SHADER_PARAMETER(float, BlackbodyIntensity)
	SHADER_PARAMETER(float, Temperature)
	SHADER_PARAMETER(float, UseDirectionalLight)
	SHADER_PARAMETER(float, UseEnvironmentLight)
END_UNIFORM_BUFFER_STRUCT()

class FVdbPrincipledVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVdbPrincipledVS);
	SHADER_USE_PARAMETER_STRUCT(FVdbPrincipledVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVdbPrincipledShaderParams, VdbGlobalParams)
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

class FVdbPrincipledPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVdbPrincipledPS);
	SHADER_USE_PARAMETER_STRUCT(FVdbPrincipledPS, FGlobalShader)

	class FPathTracing : SHADER_PERMUTATION_BOOL("PATH_TRACING");
	class FUseTemperature : SHADER_PERMUTATION_BOOL("USE_TEMPERATURE_VDB");
	class FUseColor : SHADER_PERMUTATION_BOOL("USE_COLOR_VDB");
	class FLevelSet : SHADER_PERMUTATION_BOOL("LEVEL_SET");
	class FTrilinear : SHADER_PERMUTATION_BOOL("USE_TRILINEAR_SAMPLING");
	class FExtraVdbs : SHADER_PERMUTATION_BOOL("USE_EXTRA_VDBS");
	using FPermutationDomain = TShaderPermutationDomain<FPathTracing, FUseTemperature, FUseColor, FLevelSet, FTrilinear, FExtraVdbs>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Scene / Unreal data
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		// VdbRendering data
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevAccumTex)
		SHADER_PARAMETER(uint32, NumAccumulations)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVdbPrincipledShaderParams, VdbGlobalParams)
		// Debug
		SHADER_PARAMETER(uint32, DisplayBounds)
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
