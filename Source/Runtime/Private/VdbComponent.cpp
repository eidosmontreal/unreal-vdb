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
}

UVdbComponent::~UVdbComponent() {}

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

FVolumeRenderInfos* UVdbComponent::GetRenderInfos() const
{
	if (VdbVolume != nullptr)
	{
		return &VdbVolume->GetRenderInfos();
	}
	else
	{
		return nullptr;
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

//-----------------------------------------------------------------------------
// Actor
//-----------------------------------------------------------------------------

AVdbActor::AVdbActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VdbComponent = CreateDefaultSubobject<UVdbComponent>(TEXT("VdbComponent"));
	RootComponent = VdbComponent;

	// Force a 90deg rotation to fit with Unreal coordinate system (left handed, z-up)
	FTransform Transform(FRotator(0.0f, 0.0f, -90.0f));
	VdbComponent->SetWorldTransform(Transform);
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