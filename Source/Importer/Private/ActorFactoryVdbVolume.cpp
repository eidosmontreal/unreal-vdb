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

#include "ActorFactoryVdbVolume.h"
#include "AssetData.h"

#include "VdbMaterialActor.h"
#include "VdbAssetComponent.h"
#include "VdbMaterialComponent.h"
#include "VdbSequenceComponent.h"
#include "VdbVolumeStatic.h"
#include "VdbVolumeSequence.h"


UActorFactoryVdbVolume::UActorFactoryVdbVolume(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DisplayName = FText::FromString("Vdb Actor");
	NewActorClass = AVdbMaterialActor::StaticClass();
	bUseSurfaceOrientation = true;
	bShowInEditorQuickMenu = true;
}

bool UActorFactoryVdbVolume::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid())
	{
		return true;
	}

	if (!AssetData.GetClass()->IsChildOf(UVdbVolumeBase::StaticClass()))
	{
		OutErrorMsg = FText::FromString("A valid UVdbVolume must be specified.");
		return false;
	}

	return true;
}

void UActorFactoryVdbVolume::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	UVdbVolumeBase* VdbVolume = CastChecked<UVdbVolumeBase>(Asset);

	// Change properties
	AVdbMaterialActor* VdbActor = CastChecked<AVdbMaterialActor>(NewActor);

	UVdbAssetComponent* VdbMaterialComponent = VdbActor->GetVdbAssetComponent();
	VdbMaterialComponent->UnregisterComponent();
	VdbMaterialComponent->DensityVolume = VdbVolume;
	VdbMaterialComponent->RegisterComponent();
}

void UActorFactoryVdbVolume::PostCreateBlueprint(UObject* Asset, AActor* CDO)
{
	if (Asset != NULL && CDO != NULL)
	{
		UVdbVolumeBase* VdbVolume = CastChecked<UVdbVolumeBase>(Asset);
		AVdbMaterialActor* VdbActor = CastChecked<AVdbMaterialActor>(CDO);
		UVdbAssetComponent* VdbMaterialComponent = VdbActor->GetVdbAssetComponent();
		VdbMaterialComponent->DensityVolume = VdbVolume;
	}
}
