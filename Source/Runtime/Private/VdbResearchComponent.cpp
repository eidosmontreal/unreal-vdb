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

#include "VdbResearchComponent.h"

#include "VdbCommon.h"
#include "VdbVolume.h"
#include "Rendering/VdbResearchRendering.h"
#include "Rendering/VdbResearchSceneProxy.h"

#include "RendererInterface.h"

#define LOCTEXT_NAMESPACE "VdbComponent"

//-----------------------------------------------------------------------------
// Component
//-----------------------------------------------------------------------------

UVdbResearchComponent::UVdbResearchComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UVdbResearchComponent::~UVdbResearchComponent() {}

FPrimitiveSceneProxy* UVdbResearchComponent::CreateSceneProxy()
{
	if (!VdbDensity || !VdbDensity->IsValid())
		return nullptr;

	return new FVdbResearchSceneProxy(this);
}

FBoxSphereBounds UVdbResearchComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// may need to account for later additional local matrix
	if (VdbDensity != nullptr)
	{
		FBoxSphereBounds VdbBounds(VdbDensity->GetBounds());
		return VdbBounds.TransformBy(LocalToWorld);
	}
	else
	{
		return Super::CalcBounds(LocalToWorld);
	}
}

#if WITH_EDITOR

void UVdbResearchComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName DensityName = GET_MEMBER_NAME_CHECKED(UVdbResearchComponent, VdbDensity);
	static const FName TemperatureName = GET_MEMBER_NAME_CHECKED(UVdbResearchComponent, VdbTemperature);

	const FName PropertyName = PropertyChangedEvent.Property->GetFName();
	if (PropertyName == DensityName)
	{
		
	}
	else if (PropertyName == TemperatureName)
	{

	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

//-----------------------------------------------------------------------------
// Actor
//-----------------------------------------------------------------------------

AVdbResearchActor::AVdbResearchActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VdbComponent = CreateDefaultSubobject<UVdbResearchComponent>(TEXT("VdbComponent"));
	RootComponent = VdbComponent;

	// Force a 90deg rotation to fit with Unreal coordinate system (left handed, z-up)
	FTransform Transform(FRotator(0.0f, 0.0f, -90.0f));
	VdbComponent->SetWorldTransform(Transform);
}

#if WITH_EDITOR
bool AVdbResearchActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (VdbComponent->VdbDensity)
	{
		Objects.Add(VdbComponent->VdbDensity.Get());
	}
	if (VdbComponent->VdbTemperature)
	{
		Objects.Add(VdbComponent->VdbTemperature.Get());
	}
	return true;
}
#endif

#undef LOCTEXT_NAMESPACE