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

#include "VdbToVolumeTextureComponent.h"

#include "VdbCommon.h"
#include "Rendering/VdbRenderBuffer.h"
#include "VdbVolumeStatic.h"
#include "VdbVolumeSequence.h"
#include "VdbAssetComponent.h"
#include "VdbSequenceComponent.h"

#include "GlobalShader.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "Engine/VolumeTexture.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHICommandList.h"
#include "TextureRenderTargetVolumeResource.h"

#include "RendererInterface.h"

#define LOCTEXT_NAMESPACE "VdbToVolumeComponent"

// Shader that copies a sparse VDB volume to a dense Volume texture (aka 3d Texture)
class FCopyVdbToVolumeCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCopyVdbToVolumeCS);
	SHADER_USE_PARAMETER_STRUCT(FCopyVdbToVolumeCS, FGlobalShader);

	class FPackMode : SHADER_PERMUTATION_INT("PACKING_MODE", (int)EVdbToVolumeMethod::Count - 1);
	using FPermutationDomain = TShaderPermutationDomain<FPackMode>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector3f, VolumeOffset)
		SHADER_PARAMETER(FVector3f, VolumeSize)
		SHADER_PARAMETER(FIntVector, TextureSize)
		SHADER_PARAMETER_SRV(StructuredBuffer<uint>, VdbPrimary)
		SHADER_PARAMETER_SRV(StructuredBuffer<uint>, VdbSecondary)
		SHADER_PARAMETER_UAV(RWTexture3D<float>, OutputTexture)
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
		OutEnvironment.SetDefine(TEXT("SHADER_VERSION_MAJOR"), NANOVDB_MAJOR_VERSION_NUMBER);
		OutEnvironment.SetDefine(TEXT("SHADER_VERSION_MINOR"), NANOVDB_MINOR_VERSION_NUMBER);
	}

	static const int ThreadGroupSize = 8;
};

IMPLEMENT_GLOBAL_SHADER(FCopyVdbToVolumeCS, "/Plugin/VdbVolume/Private/VdbToVolume.usf", "MainCS", SF_Compute);

UVdbToVolumeTextureComponent::UVdbToVolumeTextureComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#ifdef TICK_EVERY_FRAME
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bTickInEditor = true;
#endif
}

UVdbToVolumeTextureComponent::~UVdbToVolumeTextureComponent() {}

#ifdef TICK_EVERY_FRAME
void UVdbToVolumeTextureComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (VdbAssets)
		CopyVdbToVolume_GameThread(VdbAssets->GetCurrFrameIndex());
}
#endif

void UVdbToVolumeTextureComponent::SetVdbAssets(UVdbAssetComponent* Component)
{
	VdbAssets = Component;
#ifndef TICK_EVERY_FRAME // As long as we are ticking every frame, we don't need to register to the delegate
	Component->OnFrameChanged.AddUObject(this, &UVdbToVolumeTextureComponent::CopyVdbToVolume_GameThread);
#endif
}

void UVdbToVolumeTextureComponent::CopyVdbToVolume_GameThread(uint32 FrameIndex)
{
	if (!VdbAssets || !VolumeRenderTarget || Method == EVdbToVolumeMethod::Disabled)
		return;

	UpdateRenderTargetIfNeeded();

	const FVolumeRenderInfos* RenderInfosPrimary = VdbAssets->GetRenderInfos(VdbAssets->DensityVolume); // FLOAT
	const FVolumeRenderInfos* RenderInfosSecondary = VdbAssets->GetRenderInfos(VdbAssets->TemperatureVolume); // FLOAT
	const FVolumeRenderInfos* RenderInfosTertiary = VdbAssets->GetRenderInfos(VdbAssets->ColorVolume); // VECTOR3

	const FVolumeRenderInfos* FirstRenderInfos = nullptr;
	const FVolumeRenderInfos* SecondRenderInfos = nullptr;

	switch (Method)
	{
		case EVdbToVolumeMethod::PrimaryR8:
		case EVdbToVolumeMethod::PrimaryR16F:
		{
			if (!RenderInfosPrimary)
			{
				UE_LOG(LogSparseVolumetrics, Error, TEXT("UVdbToVolumeTextureComponent: Missing Density grid of %s."), *VdbAssets->GetName());
				return;
			}
			if (RenderInfosPrimary->IsVectorGrid())
			{
				UE_LOG(LogSparseVolumetrics, Error, TEXT("UVdbToVolumeTextureComponent: Trying to use a VectorGrid as a FloatGrid (%s)."), *VdbAssets->DensityVolume->GetName());
				return;
			}
			FirstRenderInfos = RenderInfosPrimary;
			break;
		}
		case EVdbToVolumeMethod::PrimaryRGB8:
		case EVdbToVolumeMethod::PrimaryRGB16F:
		{
			if (!RenderInfosTertiary)
			{
				UE_LOG(LogSparseVolumetrics, Error, TEXT("UVdbToVolumeTextureComponent: Missing Color grid of %s."), *VdbAssets->GetName());
				return;
			}
			if (!RenderInfosTertiary->IsVectorGrid())
			{
				UE_LOG(LogSparseVolumetrics, Error, TEXT("UVdbToVolumeTextureComponent: Trying to use %s as VectorGrid but it is a FloatGrid."), *VdbAssets->ColorVolume->GetName());
				return;
			}
			FirstRenderInfos = RenderInfosTertiary;
			break;
		}
		case EVdbToVolumeMethod::PrimarySecondaryRG8:
		case EVdbToVolumeMethod::PrimarySecondaryRG16F:
		{
			if (!RenderInfosPrimary || !RenderInfosSecondary)
			{
				UE_LOG(LogSparseVolumetrics, Error, TEXT("UVdbToVolumeTextureComponent: Missing Density and/or Temperature grid of %s."), *VdbAssets->GetName());
				return;
			}
			if (RenderInfosPrimary->IsVectorGrid() || RenderInfosSecondary->IsVectorGrid())
			{
				UE_LOG(LogSparseVolumetrics, Error, TEXT("UVdbToVolumeTextureComponent: Trying to use a VectorGrid as a FloatGrid (either %s or %s)."), *VdbAssets->DensityVolume->GetName(), *VdbAssets->TemperatureVolume->GetName());
				return;
			}
			FirstRenderInfos = RenderInfosPrimary;
			SecondRenderInfos = RenderInfosSecondary;
			break;
		}
		case EVdbToVolumeMethod::PrimarySecondaryRGBA8:
		case EVdbToVolumeMethod::PrimarySecondaryRGBA16F:
		{
			if (!RenderInfosPrimary || !RenderInfosTertiary)
			{
				UE_LOG(LogSparseVolumetrics, Error, TEXT("UVdbToVolumeTextureComponent: Missing Density and/or Color grid of %s."), *VdbAssets->GetName());
				return;
			}
			if (RenderInfosPrimary->IsVectorGrid() || !RenderInfosTertiary->IsVectorGrid())
			{
				UE_LOG(LogSparseVolumetrics, Error, TEXT("UVdbToVolumeTextureComponent: Density Volume %s should be a FloatGrid and Color Volume %s should be a VectorGrid."), *VdbAssets->DensityVolume->GetName(), *VdbAssets->ColorVolume->GetName());
				return;
			}
			FirstRenderInfos = RenderInfosPrimary;
			SecondRenderInfos = RenderInfosTertiary;
			break;
		}
	}

	ENQUEUE_RENDER_COMMAND(CopyVdbToVolumeTexture)(
		[this,
		RenderTarget = VolumeRenderTarget->GameThread_GetRenderTargetResource(),
		PrimaryVol = FirstRenderInfos->GetRenderResource(),
		SecondaryVol = SecondRenderInfos ? SecondRenderInfos->GetRenderResource() : nullptr,
		VolumeOffset = RenderInfosPrimary->GetIndexMin(),
		VolumeSize = RenderInfosPrimary->GetIndexSize(),
		TextureSize = VdbAssets->DensityVolume->GetLargestVolume()]
	(FRHICommandListImmediate& RHICmdList)
	{
		CopyVdbToVolume_RenderThread(RHICmdList, RenderTarget, PrimaryVol, SecondaryVol, VolumeOffset, VolumeSize, TextureSize);
	});
}

void UVdbToVolumeTextureComponent::CopyVdbToVolume_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FTextureRenderTargetResource* RenderTarget,
	FVdbRenderBuffer* PrimaryRenderBuffer, 
	FVdbRenderBuffer* SecondaryRenderBuffer,
	const FVector3f& IndexMin,
	const FVector3f& IndexSize,
	const FIntVector& TextureSize)
{
	FTextureRenderTargetVolumeResource* ResourceVolume = RenderTarget->GetTextureRenderTargetVolumeResource();
	if (!ResourceVolume || !ResourceVolume->GetUnorderedAccessViewRHI() || !PrimaryRenderBuffer)
		return;

	FRDGBuilder GraphBuilder(RHICmdList);

	auto* PassParameters = GraphBuilder.AllocParameters<FCopyVdbToVolumeCS::FParameters>();
	PassParameters->VolumeOffset = IndexMin;
	PassParameters->VolumeSize = IndexSize;
	PassParameters->TextureSize = TextureSize;
	PassParameters->VdbPrimary = PrimaryRenderBuffer->GetBufferSRV();
	PassParameters->VdbSecondary = SecondaryRenderBuffer ? SecondaryRenderBuffer->GetBufferSRV() : PassParameters->VdbPrimary;
	PassParameters->OutputTexture = ResourceVolume->GetUnorderedAccessViewRHI();

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Copy Vdb To Volume Texture"),
		PassParameters,
		ERDGPassFlags::Compute,
		[PassParameters, VolumeSize = IndexSize, PackMethod = (int)Method - 1](FRHICommandListImmediate& RHICmdList)
		{
			FCopyVdbToVolumeCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FCopyVdbToVolumeCS::FPackMode>(PackMethod);

			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FCopyVdbToVolumeCS> VdbShader(GlobalShaderMap, PermutationVector);

			FIntVector DispatchCount = FComputeShaderUtils::GetGroupCount(FIntVector((int32)VolumeSize.X, (int32)VolumeSize.Y, (int32)VolumeSize.Z), FCopyVdbToVolumeCS::ThreadGroupSize);
			DispatchCount += FIntVector(1, 1, 1); // safe margin, to account for trilinear filtering
			FComputeShaderUtils::Dispatch(RHICmdList, VdbShader, *PassParameters, DispatchCount);
		});

	GraphBuilder.Execute();
}

EPixelFormat GetPixelFormat(EVdbToVolumeMethod Method)
{
	EPixelFormat PixelFormat = EPixelFormat::PF_R8;
	switch (Method)
	{
		case EVdbToVolumeMethod::PrimaryR8:						PixelFormat = EPixelFormat::PF_R8; break;
		case EVdbToVolumeMethod::PrimaryR16F:					PixelFormat = EPixelFormat::PF_R16F; break;
		case EVdbToVolumeMethod::PrimaryRGB8:					PixelFormat = EPixelFormat::PF_B8G8R8A8; break;
		case EVdbToVolumeMethod::PrimaryRGB16F:					PixelFormat = EPixelFormat::PF_FloatRGBA; break;
		case EVdbToVolumeMethod::PrimarySecondaryRG8:			PixelFormat = EPixelFormat::PF_R8G8; break;
		case EVdbToVolumeMethod::PrimarySecondaryRG16F:			PixelFormat = EPixelFormat::PF_G16R16F; break;
		case EVdbToVolumeMethod::PrimarySecondaryRGBA8:			PixelFormat = EPixelFormat::PF_B8G8R8A8; break;
		case EVdbToVolumeMethod::PrimarySecondaryRGBA16F:		PixelFormat = EPixelFormat::PF_FloatRGBA; break;
		default: break;
	}
	return PixelFormat;
}


void UVdbToVolumeTextureComponent::UpdateRenderTargetIfNeeded(bool Force)
{
	if (VolumeRenderTarget && VdbAssets && VdbAssets->DensityVolume)
	{
		const FIntVector& MaxSize = VdbAssets->DensityVolume->GetLargestVolume();

		EPixelFormat PixelFormat = GetPixelFormat(Method);
		if (Force || 
			MaxSize.X != VolumeRenderTarget->SizeX || 
			MaxSize.Y != VolumeRenderTarget->SizeY || 
			MaxSize.Z != VolumeRenderTarget->SizeZ || 
			PixelFormat != VolumeRenderTarget->GetFormat())
		{
			VolumeRenderTarget->bHDR = false;
			VolumeRenderTarget->bCanCreateUAV = true;
			VolumeRenderTarget->ClearColor = FLinearColor::Transparent;
			VolumeRenderTarget->Init(MaxSize.X, MaxSize.Y, MaxSize.Z, PixelFormat);
			VolumeRenderTarget->UpdateResourceImmediate(true);
		}
	}
	else if (VolumeRenderTarget)
	{
		// reset to small black RT
		VolumeRenderTarget->bHDR = false;
		VolumeRenderTarget->bCanCreateUAV = true;
		VolumeRenderTarget->ClearColor = FLinearColor::Transparent;
		VolumeRenderTarget->Init(8, 8, 8, EPixelFormat::PF_R8);
		VolumeRenderTarget->UpdateResourceImmediate(true);
	}
}

void UVdbToVolumeTextureComponent::PostLoad()
{
	Super::PostLoad();
	UpdateRenderTargetIfNeeded(true);
}

#undef LOCTEXT_NAMESPACE
