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

#include "VdbComponentBase.h"

#include "VdbComponent.h"
#include "VdbCommon.h"
#include "VdbVolume.h"
#include "Rendering/VdbSceneProxy.h"
#include "Rendering/VdbRendering.h"

#include "RendererInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "VdbComponent"

FIntVector UVdbComponentBase::DummyIntVect;
FMatrix UVdbComponentBase::DummyMatrix;

//-----------------------------------------------------------------------------
// Component
//-----------------------------------------------------------------------------

UVdbComponentBase::UVdbComponentBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMaterial(TEXT("/SparseVolumetrics/M_Vdb_DefaultUnlit"));
	Material = DefaultMaterial.Object;
}

UVdbComponentBase::~UVdbComponentBase() 
{
}

void UVdbComponentBase::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (Material != nullptr)
	{
		OutMaterials.Add(Material);
	}
}

#if WITH_EDITOR

void MarkRenderStateDirtyForAllVdbComponents(UObject* InputObject)
{
	// Mark referencers as dirty so that the display actually refreshes with this new data
	TArray<FReferencerInformation> OutInternalReferencers;
	TArray<FReferencerInformation> OutExternalReferencers;
	InputObject->RetrieveReferencers(&OutInternalReferencers, &OutExternalReferencers);

	for (FReferencerInformation& RefInfo : OutExternalReferencers)
	{
		UVdbComponentBase* Component = Cast<UVdbComponentBase>(RefInfo.Referencer);
		if (Component)
		{
			Component->MarkRenderStateDirty();
		}
	}
}

void UVdbComponentBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName MaterialName = GET_MEMBER_NAME_CHECKED(UVdbComponentBase, Material);

	const FName PropertyName = PropertyChangedEvent.Property->GetFName();
	if (PropertyName == MaterialName)
	{
		MarkRenderStateDirty();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

#undef LOCTEXT_NAMESPACE