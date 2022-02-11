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

#include "ActorFactoryVdbVolumeSequence.h"

#include "VdbSequenceActor.h"
#include "VdbSequenceComponent.h"
#include "VdbVolumeSequence.h"

#include "AssetData.h"


UActorFactoryVdbVolumeSequence::UActorFactoryVdbVolumeSequence(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DisplayName = FText::FromString( "VDB Volume Sequence" );
	NewActorClass = AVdbSequenceActor::StaticClass();
	bUseSurfaceOrientation = true;
	bShowInEditorQuickMenu = true;
}

bool UActorFactoryVdbVolumeSequence::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid())
	{
		return true;
	}

	if (!AssetData.GetClass()->IsChildOf(UVdbVolumeSequence::StaticClass()))
	{
		OutErrorMsg = FText::FromString("A valid UVdbVolumeSequence must be specified.");
		return false;
	}

	return true;
}

void UActorFactoryVdbVolumeSequence::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	UVdbVolumeSequence* VdbVolumeSequence = CastChecked<UVdbVolumeSequence>(Asset);

	// Change properties
	AVdbSequenceActor* VdbSequenceActor = CastChecked<AVdbSequenceActor>(NewActor);

	UVdbSequenceComponent* VdbSequenceComponent = VdbSequenceActor->GetVdbSequenceComponent();
	VdbSequenceComponent->UnregisterComponent();
	VdbSequenceComponent->VdbSequence = VdbVolumeSequence;
	VdbSequenceComponent->RegisterComponent();
}

void UActorFactoryVdbVolumeSequence::PostCreateBlueprint(UObject* Asset, AActor* CDO)
{
	if (Asset != NULL && CDO != NULL)
	{
		UVdbVolumeSequence* VdbVolumeSequence = CastChecked<UVdbVolumeSequence>(Asset);
		AVdbSequenceActor* VdbSequenceActor = CastChecked<AVdbSequenceActor>(CDO);
		UVdbSequenceComponent* VdbSequenceComponent = VdbSequenceActor->GetVdbSequenceComponent();
		VdbSequenceComponent->VdbSequence = VdbVolumeSequence;
	}
}
