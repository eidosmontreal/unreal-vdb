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

#include "VdbComponent.h"

#include "VdbCommon.h"
#include "VdbVolume.h"
#include "VdbVolumeSequence.h"
#include "VdbSequenceComponent.h"
#include "Rendering/VdbSceneProxy.h"
#include "Rendering/VdbRendering.h"

#include "RendererInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "VdbComponent"

//-----------------------------------------------------------------------------
// Component
//-----------------------------------------------------------------------------

UVdbComponent::UVdbComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMaterial(TEXT("/SparseVolumetrics/M_Vdb_DefaultUnlit"));
	Material = DefaultMaterial.Object;
}

UVdbComponent::~UVdbComponent() {}

void UVdbComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (Material != nullptr)
	{
		OutMaterials.Add(Material);
	}
}

FPrimitiveSceneProxy* UVdbComponent::CreateSceneProxy()
{
	if (!VdbVolume || !VdbVolume->IsValid() || !GetMaterial(0))
		return nullptr;

	return new FVdbSceneProxy(this);
}

FBoxSphereBounds UVdbComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (VdbVolume != nullptr)
	{
		FBoxSphereBounds VdbBounds(VdbVolume->GetBounds());
		return VdbBounds.TransformBy(LocalToWorld);
	}
	else
	{
		return Super::CalcBounds(LocalToWorld);
	}
}

EVdbType UVdbComponent::GetVdbType() const
{
	if (VdbVolume != nullptr)
	{
		return VdbVolume->GetType();
	}
	else
	{
		return EVdbType::Undefined;
	}
}

#if WITH_EDITOR

void UVdbComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName VdbVolumeName = GET_MEMBER_NAME_CHECKED(UVdbComponent, VdbVolume);

	const FName PropertyName = PropertyChangedEvent.Property->GetFName();
	if (PropertyName == VdbVolumeName)
	{
		SetVdbSequence(VdbVolume, SequenceComponent);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

bool UVdbComponent::UpdateSceneProxy(uint32 FrameIndex, UVdbVolumeSequence* VdbSequence)
{
	FVdbSceneProxy* VdbSceneProxy = static_cast<FVdbSceneProxy*>(SceneProxy);

	if (!VdbSequence->IsGridDataInMemory(FrameIndex, true) || (VdbSceneProxy == nullptr))
	{
		return false;
	}

	const FVolumeRenderInfos* RenderInfos = VdbSequence->GetRenderInfos(FrameIndex);
	if (RenderInfos)
	{
		ENQUEUE_RENDER_COMMAND(UploadVdbGpuData)(
			[this,
			VdbSceneProxy,
			IndexMin = RenderInfos->GetIndexMin(),
			IndexSize = RenderInfos->GetIndexSize(),
			IndexToLocal = RenderInfos->GetIndexToLocal(),
			RenderBuffer = RenderInfos->GetRenderResource()]
		(FRHICommandList& RHICmdList)
		{
			VdbSceneProxy->Update(IndexToLocal, IndexMin, IndexSize, RenderBuffer);
		});
	}

	return true;
}

//-----------------------------------------------------------------------------
// Actor
//-----------------------------------------------------------------------------

AVdbActor::AVdbActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VdbComponent = CreateDefaultSubobject<UVdbComponent>(TEXT("VdbComponent"));
	SeqComponent = CreateDefaultSubobject<UVdbSequenceComponent>(TEXT("SeqComponent"));
	RootComponent = VdbComponent;

	// Force a 90deg rotation to fit with Unreal coordinate system (left handed, z-up)
	FTransform Transform(FRotator(0.0f, 0.0f, -90.0f));
	VdbComponent->SetWorldTransform(Transform);

	// These two components are tightly coupled
	SeqComponent->SetVdbComponent(VdbComponent);
	VdbComponent->SetSeqComponent(SeqComponent);
}

#if WITH_EDITOR
bool AVdbActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (VdbComponent->VdbVolume)
	{
		Objects.Add(VdbComponent->VdbVolume.Get());
	}
	return true;
}
#endif

#undef LOCTEXT_NAMESPACE