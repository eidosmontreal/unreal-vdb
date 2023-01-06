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
#include "Curves/CurveLinearColorAtlas.h"

FVdbMaterialSceneProxy::FVdbMaterialSceneProxy(const UVdbAssetComponent* AssetComponent, const UVdbMaterialComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
	, VdbMaterialComponent(InComponent)
	, Material(InComponent->GetMaterial(0))
{
	LevelSet = AssetComponent->GetVdbClass() == EVdbClass::SignedDistance;
	TranslucentLevelSet = LevelSet && InComponent->TranslucentLevelSet;
	ImprovedSkylight = InComponent->ImprovedSkylight;
	TrilinearSampling = InComponent->TrilinearSampling;

	VdbMaterialRenderExtension = FVolumeRuntimeModule::GetRenderExtension(InComponent->RenderTarget);

	const FVolumeRenderInfos* PrimaryRenderInfos = AssetComponent->GetRenderInfos(AssetComponent->DensityVolume);
	DensityRenderBuffer = PrimaryRenderInfos ? PrimaryRenderInfos->GetRenderResource() : nullptr;

	IndexMin = PrimaryRenderInfos->GetIndexMin();
	IndexSize = PrimaryRenderInfos->GetIndexSize();
	IndexToLocal = PrimaryRenderInfos->GetIndexToLocal();

	CurveIndex = INDEX_NONE;
	CurveAtlas = InComponent->BlackBodyCurveAtlas;
	if (!InComponent->PhysicallyBasedBlackbody && InComponent->BlackBodyCurve && CurveAtlas)
	{
		CurveAtlas->GetCurveIndex(InComponent->BlackBodyCurve, CurveIndex);
	}
	CurveAtlasTex = CurveAtlas ? CurveAtlas->GetResource() : nullptr;
	uint32 AtlasHeight = CurveAtlas ? CurveAtlas->TextureHeight : 0;

	CustomIntData0 = FIntVector4(InComponent->MaxRayDepth, InComponent->SamplesPerPixel, InComponent->ColoredTransmittance, InComponent->TemporalNoise);
	CustomIntData1 = FIntVector4(CurveIndex, int32(AtlasHeight), 0, 0);
	float VoxelSize = AssetComponent->DensityVolume->GetVoxelSize();
	CustomFloatData0 = FVector4f(InComponent->LocalStepSize, InComponent->ShadowStepSizeMultiplier, VoxelSize, InComponent->Jittering);
	CustomFloatData1 = FVector4f(InComponent->Anisotropy, InComponent->Albedo, InComponent->BlackbodyIntensity, (CurveIndex == INDEX_NONE) ? InComponent->BlackbodyTemperature : InComponent->TemperatureMultiplier);
	CustomFloatData2 = FVector4f(InComponent->DensityMultiplier, InComponent->VolumePadding, InComponent->Ambient, 0.0);

	auto FillValue = [AssetComponent](UVdbVolumeBase* Base, FVdbRenderBuffer*& Buffer)
	{
		const FVolumeRenderInfos* RenderInfos = AssetComponent->GetRenderInfos(Base);
		Buffer = RenderInfos ? RenderInfos->GetRenderResource() : nullptr;
	};

	FillValue(AssetComponent->TemperatureVolume, TemperatureRenderBuffer);
	FillValue(AssetComponent->ColorVolume, ColorRenderBuffer);
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

void FVdbMaterialSceneProxy::UpdateCurveAtlasTex()
{
	// Doing this every frame allows realtime preview and update when modifying color curves
	CurveAtlasTex = CurveAtlas ? CurveAtlas->GetResource() : nullptr;
}