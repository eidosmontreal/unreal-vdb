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

UVdbComponentBase::UVdbComponentBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UVdbComponentBase::~UVdbComponentBase() 
{
}

void UVdbComponentBase::SetVdbSequence(UVdbVolumeBase* SeqVolume, UVdbSequenceComponent* SeqComponent)
{
	if (SeqVolume && SeqVolume->IsSequence())
	{
		UVdbVolumeSequence* VdbSequence = CastChecked<UVdbVolumeSequence>(SeqVolume);
		SeqComponent->SetVdbSequence(VdbSequence);
	}
	else
	{
		SeqComponent->SetVdbSequence(nullptr);
	}
	MarkRenderStateDirty();
}

const FVolumeRenderInfos* UVdbComponentBase::GetRenderInfos(const UVdbVolumeBase* VdbVolume, const UVdbSequenceComponent* SeqComponent) const
{
	if (VdbVolume != nullptr)
	{
		bool ValidSequence = VdbVolume->IsSequence() && SeqComponent;
		if (ValidSequence)
		{
			const uint32 FrameIndex = SeqComponent->GetFrameIndexFromElapsedTime();
			return VdbVolume->GetRenderInfos(FrameIndex);
		}
		else
		{
			return VdbVolume->GetRenderInfos(0);
		}
	}
	else
	{
		return nullptr;
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

#endif

#undef LOCTEXT_NAMESPACE