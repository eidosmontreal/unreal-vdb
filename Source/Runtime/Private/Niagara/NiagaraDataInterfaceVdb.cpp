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

#include "NiagaraDataInterfaceVdb.h"
#include "Rendering/VdbRenderBuffer.h"
#include "VdbVolumeStatic.h"

#include "NiagaraTypes.h"
#include "NiagaraWorldManager.h"
#include "NiagaraSystemInstance.h"
#include "ShaderParameterUtils.h"

#define LOCTEXT_NAMESPACE "UNiagaraDataInterfaceVdb"

// Name of all the functions available in the data interface
static const FName InitVolumeName(TEXT("InitVolume"));
static const FName SampleVolumeName(TEXT("SampleVolume"));
static const FName SampleVolumeFastName(TEXT("SampleVolumeFast"));
static const FName SampleVolumePosName(TEXT("SampleVolumePos"));
static const FName LevelSetZeroCrossingName(TEXT("LevelSetZeroCrossing"));
static const FName LevelSetComputeNormalName(TEXT("LevelSetComputeNormal"));
static const FName RayClipName(TEXT("RayClip"));
// Spaces conversions
static const FName LocalToVdbSpaceName(TEXT("LocalToVdbSpace"));
static const FName LocalToVdbSpacePosName(TEXT("LocalToVdbSpacePos"));
static const FName LocalToVdbSpaceDirName(TEXT("LocalToVdbSpaceDir"));
static const FName VdbToLocalSpaceName(TEXT("VdbToLocalSpace"));
static const FName VdbToLocalSpacePosName(TEXT("VdbToLocalSpacePos"));
static const FName VdbToLocalSpaceDirName(TEXT("VdbToLocalSpaceDir"));
static const FName VdbSpaceToIjkName(TEXT("VdbSpaceToIjk"));
static const FName IjkToVdbSpaceName(TEXT("IjkToVdbSpace"));
// Ray operations
static const FName RayFromStartEndName(TEXT("RayFromStartEnd"));
static const FName RayFromStartDirName(TEXT("RayFromStartDir"));

static const FString VolumeName(TEXT("Volume_"));
static const FString IndexMinName(TEXT("IndexMin_"));
static const FString IndexMaxName(TEXT("IndexMax_"));

struct FNiagaraDataIntefaceProxyVdb : public FNiagaraDataInterfaceProxy
{
	FShaderResourceViewRHIRef SrvRHI;
	FIntVector IndexMin;
	FIntVector IndexMax;

	virtual void ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& Instance) override { check(false); }
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override { return 0; }
};

struct FNiagaraDataInterfaceParametersCS_VDB : public FNiagaraDataInterfaceParametersCS
{
	DECLARE_TYPE_LAYOUT(FNiagaraDataInterfaceParametersCS_VDB, NonVirtual);
public:
	void Bind(const FNiagaraDataInterfaceGPUParamInfo& ParameterInfo, const class FShaderParameterMap& ParameterMap)
	{
		VdbVolumeStatic.Bind(ParameterMap, *(VolumeName + ParameterInfo.DataInterfaceHLSLSymbol));
		IndexMin.Bind(ParameterMap, *(IndexMinName + ParameterInfo.DataInterfaceHLSLSymbol));
		IndexMax.Bind(ParameterMap, *(IndexMaxName + ParameterInfo.DataInterfaceHLSLSymbol));
	}

	void Set(FRHICommandList& RHICmdList, const FNiagaraDataInterfaceSetArgs& Context) const
	{
		check(IsInRenderingThread());

		FRHIComputeShader* ComputeShaderRHI = RHICmdList.GetBoundComputeShader();
		FNiagaraDataIntefaceProxyVdb* Proxy = static_cast<FNiagaraDataIntefaceProxyVdb*>(Context.DataInterface);

		if (Proxy && Proxy->SrvRHI)
		{
			SetSRVParameter(RHICmdList, ComputeShaderRHI, VdbVolumeStatic, Proxy->SrvRHI);
			SetShaderValue(RHICmdList, ComputeShaderRHI, IndexMin, Proxy->IndexMin);
			SetShaderValue(RHICmdList, ComputeShaderRHI, IndexMax, Proxy->IndexMax);
		}
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, VdbVolumeStatic);
	LAYOUT_FIELD(FShaderParameter, IndexMin);
	LAYOUT_FIELD(FShaderParameter, IndexMax);
};

IMPLEMENT_TYPE_LAYOUT(FNiagaraDataInterfaceParametersCS_VDB);
IMPLEMENT_NIAGARA_DI_PARAMETER(UNiagaraDataInterfaceVdb, FNiagaraDataInterfaceParametersCS_VDB);

////////////////////////////////////////////////////////////////////////////

UNiagaraDataInterfaceVdb::UNiagaraDataInterfaceVdb(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Proxy.Reset(new FNiagaraDataIntefaceProxyVdb());
}

void UNiagaraDataInterfaceVdb::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		ENiagaraTypeRegistryFlags Flags = 
			//ENiagaraTypeRegistryFlags::AllowAnyVariable |
			ENiagaraTypeRegistryFlags::AllowUserVariable |
			ENiagaraTypeRegistryFlags::AllowEmitterVariable |
			ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
}

void UNiagaraDataInterfaceVdb::PostLoad()
{
	Super::PostLoad();

	MarkRenderDataDirty();
}

#if WITH_EDITOR

void UNiagaraDataInterfaceVdb::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceVdb, VdbVolumeStatic))
	{
	}

	MarkRenderDataDirty();
}

#endif

bool UNiagaraDataInterfaceVdb::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	UNiagaraDataInterfaceVdb* DestinationDI = CastChecked<UNiagaraDataInterfaceVdb>(Destination);
	DestinationDI->VdbVolumeStatic = VdbVolumeStatic;
	Destination->MarkRenderDataDirty();

	return true;
}

bool UNiagaraDataInterfaceVdb::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}

	const UNiagaraDataInterfaceVdb* OtherDI = CastChecked<const UNiagaraDataInterfaceVdb>(Other);
	return OtherDI->VdbVolumeStatic == VdbVolumeStatic;
}

void UNiagaraDataInterfaceVdb::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	auto InitSignature = [](const FName& Name, const FText& Desc)
	{ 
		FNiagaraFunctionSignature Sig;
		Sig.Name = Name;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.bSupportsCPU = false;
		Sig.bSupportsGPU = true;
		Sig.SetDescription(Desc);
		return Sig;
	};

	{
		FNiagaraFunctionSignature Sig = InitSignature(InitVolumeName, LOCTEXT("InitVolume", "Mandatory function to init VDB volume sampling."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Accessor")));  // fake int to make sure we force users to init volume before using it
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("GridType")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(SampleVolumeName, LOCTEXT("SampleVolume", "Sample VDB volume at IJK coordinates. Supports all grid types."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Accessor")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("GridType")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("i")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("j")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("k")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(SampleVolumeFastName, LOCTEXT("SampleVolume", "Sample VDB volume at IJK coordinates. Optimal way to do it, but only supports 32f grids (i.e non-quantized)."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Accessor")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("i")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("j")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("k")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(SampleVolumePosName, LOCTEXT("SampleVolumePos", "Sample VDB volume at 3D position."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Accessor")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("GridType")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(LevelSetZeroCrossingName, LOCTEXT("LevelSetZeroCrossing", "Trace ray and checks if it crosses a LevelSet in the volume. Returns if hit, which ijk index and value v."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Accessor")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("GridType")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FVdbRay::StaticStruct()), TEXT("Ray")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Hit")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FVdbLevelSetHit::StaticStruct()), TEXT("HitResults")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(LevelSetComputeNormalName, LOCTEXT("LevelSetComputeNormal", "Computes LevelSet normal from successful Zero Crossing hit."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Accessor")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("GridType")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("v")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("i")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("j")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("k")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(RayClipName, LOCTEXT("RayClip", "Fast Ray update against Volume Bounding Box. Returns false if ray doesn't collide with volume. Updates Ray start and end according to Volume bounding box."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FVdbRay::StaticStruct()), TEXT("Ray")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Hit")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FVdbRay::StaticStruct()), TEXT("Ray")));
		OutFunctions.Add(Sig);
	}
	// Space conversions
	{
		FNiagaraFunctionSignature Sig = InitSignature(LocalToVdbSpacePosName, LOCTEXT("LocalToVdbSpacePos", "Converts Position from Local space to VDB space (aka index space)."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("LocalPos")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbPos")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(LocalToVdbSpaceDirName, LOCTEXT("LocalToVdbSpaceDir", "Converts Direction from Local space to VDB space (aka index space)."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("LocalDir")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbDir")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(LocalToVdbSpaceName, LOCTEXT("LocalToVdbSpace", "Converts Position and Direction from Local space to VDB space (aka index space)."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("LocalPos")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("LocalDir")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbPos")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbDir")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(VdbToLocalSpacePosName, LOCTEXT("VdbToLocalSpacePos", "Converts Position from Local space to VDB space (aka index space)."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbPos")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("LocalPos")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(VdbToLocalSpaceDirName, LOCTEXT("VdbToLocalSpaceDir", "Converts Direction from Local space to VDB space (aka index space)."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbDir")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("LocalDir")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(VdbToLocalSpaceName, LOCTEXT("VdbToLocalSpace", "Converts Position and Direction from Local space to VDB space (aka index space)."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbPos")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbDir")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("LocalPos")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("LocalDir")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(VdbSpaceToIjkName, LOCTEXT("VdbSpaceToIjk", "Converts VDB position to ijk volume index."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbPos")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("i")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("j")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("k")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(IjkToVdbSpaceName, LOCTEXT("IjkToVdbSpace", "Converts ijk volume index to VDB position."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("i")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("j")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("k")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbPos")));
		OutFunctions.Add(Sig);
	}
	// Ray operations
	{
		FNiagaraFunctionSignature Sig = InitSignature(RayFromStartEndName, LOCTEXT("RayFromStartEnd", "Create Ray From Start and End indications."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Start")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("End")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FVdbRay::StaticStruct()), TEXT("Ray")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(RayFromStartDirName, LOCTEXT("RayFromStartDir", "Create Ray From Start and Direction indications."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Start")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Dir")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FVdbRay::StaticStruct()), TEXT("Ray")));
		OutFunctions.Add(Sig);
	}
}
#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterfaceVdb::GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL)
{
	auto FormatString = [&](const TCHAR* Format)
	{
		TMap<FString, FStringFormatArg> Args = {
			{ TEXT("FunctionName"), FunctionInfo.InstanceName },
			{ TEXT("VolumeName"), VolumeName + ParamInfo.DataInterfaceHLSLSymbol },
			{ TEXT("IndexMin"), IndexMinName + ParamInfo.DataInterfaceHLSLSymbol },
			{ TEXT("IndexMax"), IndexMaxName + ParamInfo.DataInterfaceHLSLSymbol },
		};
		OutHLSL += FString::Format(Format, Args);
		return true;
	};

	if (FunctionInfo.DefinitionName == InitVolumeName)
	{
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(out int Accessor, out int GridType) 
			{
				InitVolume({VolumeName}, GridType);
				Accessor = 1; // useless, but usefull for consistency and readability
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == SampleVolumeName)
	{
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in int Accessor, in int GridType, int i, int j, int k, out float Out_Value) 
			{
				Out_Value = ReadCompressedValue({VolumeName}, VdbAccessor, GridType, int3(i, j, k));
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == SampleVolumeFastName)
	{
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in int Accessor, int i, int j, int k, out float Out_Value) 
			{
				Out_Value = ReadValue({VolumeName}, VdbAccessor, int3(i, j, k));
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == SampleVolumePosName)
	{
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in int Accessor, in int GridType, in float3 Position, out float Out_Value) 
			{
				pnanovdb_coord_t ijk = pnanovdb_hdda_pos_to_ijk(Position);
				Out_Value = ReadCompressedValue({VolumeName}, VdbAccessor, GridType, ijk);
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == LevelSetZeroCrossingName)
	{
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in int Accessor, in int GridType, in VdbRay Ray, out bool Hit, out VdbLevelSetHit HitResults) 
			{
				int3 ijk;
				Hit = pnanovdb_hdda_zero_crossing_improved(GridType, {VolumeName}, VdbAccessor, Ray.Origin, Ray.Tmin, Ray.Direction, Ray.Tmax, HitResults.t, HitResults.v0, ijk);
				HitResults.i = ijk.x; HitResults.j = ijk.y; HitResults.k = ijk.z;
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == LevelSetComputeNormalName)
	{
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in int Accessor, in int GridType, in float v, in int i, in int j, in int k, out float3 Normal) 
			{
				Normal = ZeroCrossingNormal(GridType, {VolumeName}, VdbAccessor, v, int3(i, j, k));
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == RayClipName)
	{
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in VdbRay InRay, out bool Hit, out VdbRay OutRay) 
			{
				OutRay = InRay;
				Hit = pnanovdb_hdda_ray_clip({IndexMin}, {IndexMax}, OutRay.Origin, OutRay.Tmin, OutRay.Direction, OutRay.Tmax);
			}
		)");
		return FormatString(Format);
	}
	// Space conversions
	else if (FunctionInfo.DefinitionName == LocalToVdbSpacePosName)
	{
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in float3 LocalPos, out float3 VdbPos) 
			{
				VdbPos = LocalToIndexPos({VolumeName}, LocalPos);
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == LocalToVdbSpaceDirName)
	{
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in float3 LocalDir, out float3 VdbDir) 
			{
				VdbDir = LocalToIndexDir({VolumeName}, LocalDir);
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == LocalToVdbSpaceName)
	{
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in float3 LocalPos, in float3 LocalDir, out float3 VdbPos, out float3 VdbDir) 
			{
				VdbPos = LocalToIndexPos({VolumeName}, LocalPos);
				VdbDir = LocalToIndexDir({VolumeName}, LocalDir);
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == VdbToLocalSpacePosName)
	{
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in float3 VdbPos, out float3 LocalPos) 
			{
				LocalPos = IndexToLocalPos({VolumeName}, VdbPos);
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == VdbToLocalSpaceDirName)
	{
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in float3 VdbDir, out float3 LocalDir) 
			{
				LocalDir = IndexToLocalDir({VolumeName}, VdbDir);
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == VdbToLocalSpaceName)
	{
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in float3 VdbPos, in float3 VdbDir, out float3 LocalPos, out float3 LocalDir) 
			{
				LocalPos = IndexToLocalPos({VolumeName}, VdbPos);
				LocalDir = IndexToLocalDir({VolumeName}, VdbDir);
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == VdbSpaceToIjkName)
	{
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in float3 VdbPos, out int i, out int j, out int k) 
			{
				int3 ijk = IndexToIjk(VdbPos);
				i = ijk.x;
				j = ijk.y;
				k = ijk.z;
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == IjkToVdbSpaceName)
	{
		static const TCHAR* Format = TEXT(R"(
				void {FunctionName}(in int i, in int j, in int k, out float3 VdbPos) 
				{
					VdbPos = pnanovdb_coord_to_vec3(int3(i, j, k));
				}
			)");
		return FormatString(Format);
	}

	return false;
}

void UNiagaraDataInterfaceVdb::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	OutHLSL += TEXT("StructuredBuffer<uint> ") + VolumeName + ParamInfo.DataInterfaceHLSLSymbol + TEXT(";\n");
	OutHLSL += TEXT("int3 ") + IndexMinName + ParamInfo.DataInterfaceHLSLSymbol + TEXT(";\n");
	OutHLSL += TEXT("int3 ") + IndexMaxName + ParamInfo.DataInterfaceHLSLSymbol + TEXT(";\n");
	OutHLSL += TEXT("\n");
}

void UNiagaraDataInterfaceVdb::GetCommonHLSL(FString& OutHLSL)
{
	OutHLSL += TEXT("#include \"/Plugin/VdbVolume/Private/NiagaraDataInterfaceVdb.ush\"\n");
}

bool UNiagaraDataInterfaceVdb::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	if (!Super::AppendCompileHash(InVisitor))
		return false;

	FSHAHash Hash = GetShaderFileHash((TEXT("/Plugin/VdbVolume/Private/NiagaraDataInterfaceVdb.ush")), EShaderPlatform::SP_PCD3D_SM5);
	InVisitor->UpdateString(TEXT("NiagaraDataInterfaceVdbHLSLSource"), Hash.ToString());
	return true;
}
#endif

void UNiagaraDataInterfaceVdb::PushToRenderThreadImpl()
{
	FNiagaraDataIntefaceProxyVdb* NDIProxy = GetProxyAs<FNiagaraDataIntefaceProxyVdb>();
	
	FVdbRenderBuffer* RT_Resource = VdbVolumeStatic ? VdbVolumeStatic->GetRenderInfos()->GetRenderResource() : nullptr;
	FIntVector IndexMin = VdbVolumeStatic ? VdbVolumeStatic->GetIndexMin() : FIntVector();
	FIntVector IndexMax = VdbVolumeStatic ? VdbVolumeStatic->GetIndexMax() : FIntVector();

	ENQUEUE_RENDER_COMMAND(FPushDIVolumeVdbToRT)
		(
		[NDIProxy, RT_Resource, IndexMin, IndexMax](FRHICommandListImmediate& RHICmdList)
		{
			NDIProxy->SrvRHI = RT_Resource ? RT_Resource->GetBufferSRV() : nullptr;
			NDIProxy->IndexMin = IndexMin;
			NDIProxy->IndexMax = IndexMax;
		}
	);
}

#undef LOCTEXT_NAMESPACE
