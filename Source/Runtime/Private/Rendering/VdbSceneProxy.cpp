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

#include "VdbSceneProxy.h"
#include "VdbComponent.h"
#include "VdbVolume.h"
#include "VdbCommon.h"
#include "VolumeRuntimeModule.h"
#include "VdbSequenceComponent.h"
#include "VdbVolumeSequence.h"
#include "Rendering/MeshCube.h"
#include "Rendering/VdbRendering.h"
#include "Materials/Material.h"

FVdbSceneProxy::FVdbSceneProxy(const UVdbComponentBase* InComponent)
	: FPrimitiveSceneProxy(InComponent)
	, VdbComponent(InComponent)
	, Material(InComponent->GetMaterial(0))
	, DensityMultiplier(InComponent->DensityMultiplier)
	, StepMultiplier(InComponent->StepMultiplier)
{
	LevelSet = InComponent->GetVdbType() == EVdbType::SignedDistance;
	VdbRenderExtension = FVolumeRuntimeModule::GetRenderExtension();

	IndexMin = InComponent->GetRenderInfos()->GetIndexMin();
	IndexSize = InComponent->GetRenderInfos()->GetIndexSize();
	IndexToLocal = InComponent->GetRenderInfos()->GetIndexToLocal();
	RenderBuffer = InComponent->GetRenderInfos()->GetRenderResource();
}

// This setups associated volume mesh for built-in Unreal passes. 
// Actual rendering is prepared FVdbRendering::PostOpaque_RenderThread.
void FVdbSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	SCOPE_CYCLE_COUNTER(STAT_VdbSceneProxy_GetDynamicMeshElements);
	check(IsInRenderingThread());

	if (!Material || Material->GetMaterial()->MaterialDomain != MD_Volume)
		return;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FSceneView* View = Views[ViewIndex];

		if (IsShown(View) && (VisibilityMap & (1 << ViewIndex)) && VdbRenderExtension->ShouldRenderVolumetricVdb())
		{
			VisibleViews.Add(View);

			FVdbVertexFactoryUserDataWrapper& UserData = Collector.AllocateOneFrameResource<FVdbVertexFactoryUserDataWrapper>();
			UserData.Data.IndexMin = GetIndexMin();
			UserData.Data.IndexSize = GetIndexSize();
			UserData.Data.IndexToLocal = GetIndexToLocal();

			FMeshBatch& Mesh = Collector.AllocateMesh();
			Mesh.bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

			VdbRenderExtension->CreateMeshBatch(Mesh, this, UserData, Material->GetRenderProxy());

			Collector.AddMesh(ViewIndex, Mesh);

			{
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
				RenderBounds(PDI, ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
			}
		}
	}
}

FPrimitiveViewRelevance FVdbSceneProxy::GetViewRelevance(const FSceneView* View) const
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

SIZE_T FVdbSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

void FVdbSceneProxy::CreateRenderThreadResources()
{
	FPrimitiveSceneProxy::CreateRenderThreadResources();

	VdbRenderExtension->AddVdbProxy(this);
}

void FVdbSceneProxy::DestroyRenderThreadResources()
{
	FPrimitiveSceneProxy::DestroyRenderThreadResources();

	VdbRenderExtension->RemoveVdbProxy(this);
}

void FVdbSceneProxy::Update(const FMatrix& InIndexToLocal, const FVector& InIndexMin, const FVector& InIndexSize, FVdbRenderBuffer* InRenderBuffer)
{
	IndexToLocal = InIndexToLocal;
	IndexMin = InIndexMin;
	IndexSize = InIndexSize;
	RenderBuffer = InRenderBuffer;
}