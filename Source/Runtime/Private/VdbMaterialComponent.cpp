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
#include "Curves/CurveLinearColorAtlas.h"

#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "VdbMaterialComponent"

UVdbMaterialComponent::UVdbMaterialComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMaterial(TEXT("/SparseVolumetrics/Materials/M_VDB_Lit_Inst"));
	Material = DefaultMaterial.Object;

	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> DefaultRenderTarget(TEXT("TextureRenderTarget2D'/SparseVolumetrics/Misc/RT_VdbMatRenderTarget.RT_VdbMatRenderTarget'"));
	RenderTarget = DefaultRenderTarget.Object;
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
	if (!VdbAssets->DensityVolume || !VdbAssets->DensityVolume->IsValid() || VdbAssets->DensityVolume->IsVectorGrid() || !GetMaterial(0))
		return nullptr;

	return new FVdbMaterialSceneProxy(VdbAssets, this);
}

FBoxSphereBounds UVdbMaterialComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (VdbAssets->DensityVolume != nullptr)
	{
		FBoxSphereBounds VdbBounds(VdbAssets->DensityVolume->GetGlobalBounds());
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

	UVdbVolumeSequence* DensitySequence = Cast<UVdbVolumeSequence>(VdbAssets->DensityVolume);
	const FVolumeRenderInfos* RenderInfosDensity = DensitySequence->GetRenderInfos(FrameIndex);

	UVdbVolumeSequence* TemperatureSequence = Cast<UVdbVolumeSequence>(VdbAssets->TemperatureVolume);
	const FVolumeRenderInfos* RenderInfosTemperature = TemperatureSequence ? TemperatureSequence->GetRenderInfos(FrameIndex) : nullptr;

	UVdbVolumeSequence* ColorSequence = Cast<UVdbVolumeSequence>(VdbAssets->ColorVolume);
	const FVolumeRenderInfos* RenderInfosColor = ColorSequence ? ColorSequence->GetRenderInfos(FrameIndex) : nullptr;

	if (RenderInfosDensity)
	{
		ENQUEUE_RENDER_COMMAND(UploadVdbGpuData)(
			[this,
			VdbMaterialSceneProxy,
			IndexMin = RenderInfosDensity->GetIndexMin(),
			IndexSize = RenderInfosDensity->GetIndexSize(),
			IndexToLocal = RenderInfosDensity->GetIndexToLocal(),
			DensRenderBuffer = RenderInfosDensity->GetRenderResource(),
			TempRenderBuffer = RenderInfosTemperature ? RenderInfosTemperature->GetRenderResource() : nullptr,
			ColorRenderBuffer = RenderInfosColor ? RenderInfosColor->GetRenderResource() : nullptr]
		(FRHICommandList& RHICmdList)
		{
			VdbMaterialSceneProxy->Update(IndexToLocal, IndexMin, IndexSize, DensRenderBuffer, TempRenderBuffer, ColorRenderBuffer);
		});
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

#if WITH_EDITOR

void UVdbMaterialComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property
		&& (PropertyChangedEvent.Property->GetName() == TEXT("BlackBodyCurveAtlas") || 
			PropertyChangedEvent.Property->GetName() == TEXT("BlackBodyCurve")))
	{
		if (!BlackBodyCurveAtlas)
		{
			// Need a Curve Atlas before selecting a Curve
			BlackBodyCurve = nullptr;
		}
		else
		{
			int32 CurveIndex = INDEX_NONE;
			if (!BlackBodyCurveAtlas->GetCurveIndex(BlackBodyCurve, CurveIndex))
			{
				// Selected Curve should be part of the selected Curve Atlas. Otherwise reset.
				BlackBodyCurve = nullptr;
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

#undef LOCTEXT_NAMESPACE