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

#include "VdbMaterialSceneProxy.h"
#include "VdbAssetComponent.h"
#include "VdbMaterialComponent.h"
#include "VdbVolumeStatic.h"
#include "VdbCommon.h"
#include "VolumeRuntimeModule.h"
#include "VdbSequenceComponent.h"
#include "VdbVolumeSequence.h"
#include "Rendering/VolumeMesh.h"
#include "Rendering/VdbMaterialRendering.h"
#include "Materials/Material.h"
#include "Algo/AnyOf.h"

FVdbMaterialSceneProxy::FVdbMaterialSceneProxy(const UVdbAssetComponent* AssetComponent, const UVdbMaterialComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
	, VdbMaterialComponent(InComponent)
	, Material(InComponent->GetMaterial(0))
{
	LevelSet = AssetComponent->GetVdbClass() == EVdbClass::SignedDistance;
	TranslucentLevelSet = LevelSet && InComponent->TranslucentLevelSet;
	ImprovedSkylight = InComponent->ImprovedSkylight;
	VdbMaterialRenderExtension = FVolumeRuntimeModule::GetRenderExtension();

	const FVolumeRenderInfos* PrimaryRenderInfos = AssetComponent->GetRenderInfos(AssetComponent->DensityVolume);
	DensityRenderBuffer = PrimaryRenderInfos ? PrimaryRenderInfos->GetRenderResource() : nullptr;

	IndexMin = PrimaryRenderInfos->GetIndexMin();
	IndexSize = PrimaryRenderInfos->GetIndexSize();
	IndexToLocal = PrimaryRenderInfos->GetIndexToLocal();

	CustomIntData0 = FIntVector4(InComponent->MaxRayDepth, InComponent->SamplesPerPixel, InComponent->ColoredTransmittance, InComponent->TemporalNoise);
	float VoxelSize = AssetComponent->DensityVolume->GetVoxelSize();
	CustomFloatData0 = FVector4f(InComponent->LocalStepSize, InComponent->ShadowStepSizeMultiplier, VoxelSize, InComponent->Jittering);
	CustomFloatData1 = FVector4f(InComponent->Anisotropy, InComponent->Albedo, InComponent->BlackbodyIntensity, InComponent->BlackbodyTemperature);
	CustomFloatData2 = FVector4f(InComponent->DensityMultiplier, InComponent->VolumePadding, InComponent->Ambient, 0.0);

	auto FillValue = [AssetComponent](UVdbVolumeBase* Base, FVdbRenderBuffer*& Buffer)
	{
		const FVolumeRenderInfos* RenderInfos = AssetComponent->GetRenderInfos(Base);
		Buffer = RenderInfos ? RenderInfos->GetRenderResource() : nullptr;
	};

	FillValue(AssetComponent->TemperatureVolume, TemperatureRenderBuffer);
	FillValue(AssetComponent->ColorVolume, ColorRenderBuffer);

	if (AssetComponent->FloatVolume1 || AssetComponent->FloatVolume2 || AssetComponent->FloatVolume3 || AssetComponent->FloatVolume4 ||
		AssetComponent->VectorVolume1 || AssetComponent->VectorVolume2 || AssetComponent->VectorVolume3 || AssetComponent->VectorVolume4)
	{
		// Extra non-realtime data
		FillValue(AssetComponent->FloatVolume1, ExtraRenderBuffers[0]);
		FillValue(AssetComponent->FloatVolume2, ExtraRenderBuffers[1]);
		FillValue(AssetComponent->FloatVolume3, ExtraRenderBuffers[2]);
		FillValue(AssetComponent->FloatVolume4, ExtraRenderBuffers[3]);
		FillValue(AssetComponent->VectorVolume1, ExtraRenderBuffers[4]);
		FillValue(AssetComponent->VectorVolume2, ExtraRenderBuffers[5]);
		FillValue(AssetComponent->VectorVolume3, ExtraRenderBuffers[6]);
		FillValue(AssetComponent->VectorVolume4, ExtraRenderBuffers[7]);
	}
	else
	{
		for (auto& Buffer : ExtraRenderBuffers)
		{
			Buffer = nullptr;
		}
	}
}

// This setups associated volume mesh for built-in Unreal passes. 
// Actual rendering is prepared FVdbMaterialRendering::PostOpaque_RenderThread.
void FVdbMaterialSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	SCOPE_CYCLE_COUNTER(STAT_VdbSceneProxy_GetDynamicMeshElements);
	check(IsInRenderingThread());

	if (!Material || Material->GetMaterial()->MaterialDomain != MD_Volume)
		return;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FSceneView* View = Views[ViewIndex];

		if (IsShown(View) && (VisibilityMap & (1 << ViewIndex)) && VdbMaterialRenderExtension->ShouldRenderVolumetricVdb())
		{
			VisibleViews.Add(View);

			FVdbVertexFactoryUserDataWrapper& UserData = Collector.AllocateOneFrameResource<FVdbVertexFactoryUserDataWrapper>();
			UserData.Data.IndexMin = GetIndexMin();
			UserData.Data.IndexSize = GetIndexSize();
			UserData.Data.IndexToLocal = GetIndexToLocal();

			FMeshBatch& Mesh = Collector.AllocateMesh();
			Mesh.bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

			VdbMaterialRenderExtension->CreateMeshBatch(Mesh, this, UserData, Material->GetRenderProxy());

			Collector.AddMesh(ViewIndex, Mesh);

			{
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
				RenderBounds(PDI, ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
			}
		}
	}
}

FPrimitiveViewRelevance FVdbMaterialSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View) && ShouldRenderInMainPass();
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	return Result;
}

SIZE_T FVdbMaterialSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

void FVdbMaterialSceneProxy::CreateRenderThreadResources()
{
	FPrimitiveSceneProxy::CreateRenderThreadResources();

	VdbMaterialRenderExtension->AddVdbProxy(this);
}

void FVdbMaterialSceneProxy::DestroyRenderThreadResources()
{
	FPrimitiveSceneProxy::DestroyRenderThreadResources();

	VdbMaterialRenderExtension->RemoveVdbProxy(this);
}

void FVdbMaterialSceneProxy::Update(const FMatrix44f& InIndexToLocal, const FVector3f& InIndexMin, const FVector3f& InIndexSize, FVdbRenderBuffer* PrimRenderBuffer, FVdbRenderBuffer* SecRenderBuffer, FVdbRenderBuffer* TerRenderBuffer)
{
	IndexToLocal = InIndexToLocal;
	IndexMin = InIndexMin;
	IndexSize = InIndexSize;
	DensityRenderBuffer = PrimRenderBuffer;
	TemperatureRenderBuffer = SecRenderBuffer;
	ColorRenderBuffer = TerRenderBuffer;
}

void FVdbMaterialSceneProxy::UpdateExtraBuffers(const TStaticArray<FVdbRenderBuffer*, 8>& RenderBuffers)
{
	ExtraRenderBuffers = RenderBuffers;
}

bool FVdbMaterialSceneProxy::UseExtraRenderResources() const 
{ 
	return Algo::AnyOf(ExtraRenderBuffers, [](FVdbRenderBuffer* Buf) {return Buf != nullptr; }); 
}
