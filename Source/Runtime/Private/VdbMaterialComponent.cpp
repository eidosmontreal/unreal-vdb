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

#include "VdbMaterialComponent.h"

#include "VdbCommon.h"
#include "VdbVolumeBase.h"
#include "VdbVolumeSequence.h"
#include "VdbAssetComponent.h"
#include "Rendering/VdbMaterialSceneProxy.h"

//#include "RendererInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "VdbMaterialComponent"

UVdbMaterialComponent::UVdbMaterialComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMaterial(TEXT("/SparseVolumetrics/Materials/M_VDB_Lit_Inst"));
	Material = DefaultMaterial.Object;
}

UVdbMaterialComponent::~UVdbMaterialComponent() {}

void UVdbMaterialComponent::SetVdbAssets(UVdbAssetComponent* Comp) 
{
	VdbAssets = Comp;
	Comp->OnFrameChanged.AddUObject(this, &UVdbMaterialComponent::UpdateSceneProxy);
}

void UVdbMaterialComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (Material != nullptr)
	{
		OutMaterials.Add(Material);
	}
}

FPrimitiveSceneProxy* UVdbMaterialComponent::CreateSceneProxy()
{
	if (!VdbAssets->PrimaryVolume || !VdbAssets->PrimaryVolume->IsValid() || VdbAssets->PrimaryVolume->IsVectorGrid() || !GetMaterial(0))
		return nullptr;

	return new FVdbMaterialSceneProxy(VdbAssets, this);
}

FBoxSphereBounds UVdbMaterialComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (VdbAssets->PrimaryVolume != nullptr)
	{
		FBoxSphereBounds VdbBounds(VdbAssets->PrimaryVolume->GetGlobalBounds());
		return VdbBounds.TransformBy(LocalToWorld);
	}
	else
	{
		return Super::CalcBounds(LocalToWorld);
	}
}

void UVdbMaterialComponent::UpdateSceneProxy(uint32 FrameIndex)
{
	FVdbMaterialSceneProxy* VdbMaterialSceneProxy = static_cast<FVdbMaterialSceneProxy*>(SceneProxy);
	if (VdbMaterialSceneProxy == nullptr)
	{
		return;
	}

	UVdbVolumeSequence* PrimarySequence = Cast<UVdbVolumeSequence>(VdbAssets->PrimaryVolume);
	const FVolumeRenderInfos* RenderInfosPrimary = PrimarySequence->GetRenderInfos(FrameIndex);

	UVdbVolumeSequence* SecondarySequence = Cast<UVdbVolumeSequence>(VdbAssets->SecondaryVolume);
	const FVolumeRenderInfos* RenderInfosSecondary = SecondarySequence ? SecondarySequence->GetRenderInfos(FrameIndex) : nullptr;

	if (RenderInfosPrimary)
	{
		ENQUEUE_RENDER_COMMAND(UploadVdbGpuData)(
			[this,
			VdbMaterialSceneProxy,
			IndexMin = RenderInfosPrimary->GetIndexMin(),
			IndexSize = RenderInfosPrimary->GetIndexSize(),
			IndexToLocal = RenderInfosPrimary->GetIndexToLocal(),
			PrimaryRenderBuffer = RenderInfosPrimary->GetRenderResource(),
			SecondaryRenderBuffer = RenderInfosSecondary ? RenderInfosSecondary->GetRenderResource() : nullptr]
		(FRHICommandList& RHICmdList)
		{
			VdbMaterialSceneProxy->Update(IndexToLocal, IndexMin, IndexSize, PrimaryRenderBuffer, SecondaryRenderBuffer);
		});

		if (VdbAssets->FloatVolume1 || VdbAssets->FloatVolume2 || VdbAssets->FloatVolume3 || VdbAssets->FloatVolume4 ||
			VdbAssets->VectorVolume1 || VdbAssets->VectorVolume2 || VdbAssets->VectorVolume3 || VdbAssets->VectorVolume4)
		{
			auto FillValue = [FrameIndex](UVdbVolumeBase* Base, FVdbRenderBuffer*& Buffer)
			{
				UVdbVolumeSequence* Seq = Cast<UVdbVolumeSequence>(Base);
				Buffer = Seq ? Seq->GetRenderInfos(FrameIndex)->GetRenderResource() : nullptr;
			};
			TStaticArray<FVdbRenderBuffer*, NUM_EXTRA_VDBS> Buffers;
			FillValue(VdbAssets->FloatVolume1, Buffers[0]);
			FillValue(VdbAssets->FloatVolume2, Buffers[1]);
			FillValue(VdbAssets->FloatVolume3, Buffers[2]);
			FillValue(VdbAssets->FloatVolume4, Buffers[3]);
			FillValue(VdbAssets->VectorVolume1, Buffers[4]);
			FillValue(VdbAssets->VectorVolume2, Buffers[5]);
			FillValue(VdbAssets->VectorVolume3, Buffers[6]);
			FillValue(VdbAssets->VectorVolume4, Buffers[7]);

			ENQUEUE_RENDER_COMMAND(UploadVdbGpuData)(
				[this, VdbMaterialSceneProxy, Buffers](FRHICommandList& RHICmdList)
				{
					VdbMaterialSceneProxy->UpdateExtraBuffers(Buffers);
				});
		}
	}
}

int32 UVdbMaterialComponent::GetNumMaterials() const 
{ 
	return (Material != nullptr) ? 1 : 0; 
}

void UVdbMaterialComponent::SetMaterial(int32 ElementIndex, class UMaterialInterface* InMaterial)
{
	if (InMaterial != Material)
	{
		Material = InMaterial;
		MarkRenderStateDirty();
	}
}

template<typename T>
void UVdbMaterialComponent::SetAttribute(T& Attribute, const T& NewValue)
{
	if (AreDynamicDataChangesAllowed() && Attribute != NewValue)
	{
		Attribute = NewValue;
		MarkRenderStateDirty();
	}
}
template void UVdbMaterialComponent::SetAttribute<float>(float& Attribute, const float& NewValue);

#undef LOCTEXT_NAMESPACE