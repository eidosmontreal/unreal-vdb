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

#include "VdbPrincipledComponent.h"

#include "VdbCommon.h"
#include "VdbVolumeStatic.h"
#include "VdbVolumeSequence.h"
#include "VdbAssetComponent.h"
#include "VdbSequenceComponent.h"
#include "Rendering/VdbPrincipledRendering.h"
#include "Rendering/VdbPrincipledSceneProxy.h"

#include "RendererInterface.h"

#define LOCTEXT_NAMESPACE "VdbPrincipledComponent"

UVdbPrincipledComponent::UVdbPrincipledComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UVdbPrincipledComponent::~UVdbPrincipledComponent() {}

void UVdbPrincipledComponent::SetVdbAssets(UVdbAssetComponent* Comp)
{
	VdbAssets = Comp;
	Comp->OnFrameChanged.AddUObject(this, &UVdbPrincipledComponent::UpdateSceneProxy);
}

FPrimitiveSceneProxy* UVdbPrincipledComponent::CreateSceneProxy()
{
	if (!VdbAssets->PrimaryVolume || !VdbAssets->PrimaryVolume->IsValid() || VdbAssets->IsVectorGrid())
		return nullptr;

	return new FVdbPrincipledSceneProxy(VdbAssets, this);
}

FBoxSphereBounds UVdbPrincipledComponent::CalcBounds(const FTransform& LocalToWorld) const
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

void UVdbPrincipledComponent::UpdateSceneProxy(uint32 FrameIndex)
{
	FVdbPrincipledSceneProxy* VdbSceneProxy = static_cast<FVdbPrincipledSceneProxy*>(SceneProxy);
	if (VdbSceneProxy == nullptr)
	{
		return;
	}

	UVdbVolumeSequence* DensitySequence = (UVdbVolumeSequence*)VdbAssets->PrimaryVolume;
	const FVolumeRenderInfos* RenderInfosDensity = DensitySequence->GetRenderInfos(FrameIndex);

	UVdbVolumeSequence* TemperatureSequence = (UVdbVolumeSequence*)VdbAssets->SecondaryVolume;
	const FVolumeRenderInfos* RenderInfosTemperature = TemperatureSequence ? TemperatureSequence->GetRenderInfos(FrameIndex) : nullptr;

	if (RenderInfosDensity)
	{
		ENQUEUE_RENDER_COMMAND(UploadVdbGpuData)(
			[this,
			VdbSceneProxy,
			IndexMin = RenderInfosDensity->GetIndexMin(),
			IndexSize = RenderInfosDensity->GetIndexSize(),
			IndexToLocal = RenderInfosDensity->GetIndexToLocal(),
			DensityBuffer = RenderInfosDensity->GetRenderResource(),
			TemperatureBuffer = RenderInfosTemperature ? RenderInfosTemperature->GetRenderResource() : nullptr]
		(FRHICommandList& RHICmdList)
		{
			VdbSceneProxy->Update(IndexToLocal, IndexMin, IndexSize, DensityBuffer, TemperatureBuffer);
		});
	}
}

#undef LOCTEXT_NAMESPACE