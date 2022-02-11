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

#include "VdbComponent.h"
#include "VdbVolume.h"


UActorFactoryVdbVolume::UActorFactoryVdbVolume(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DisplayName = FText::FromString( "Vdb Volume" );
	NewActorClass = AVdbActor::StaticClass();
	bUseSurfaceOrientation = true;
	bShowInEditorQuickMenu = true;
}

bool UActorFactoryVdbVolume::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid())
	{
		return true;
	}

	if (!AssetData.GetClass()->IsChildOf(UVdbVolume::StaticClass()))
	{
		OutErrorMsg = FText::FromString("A valid UVdbVolume must be specified.");
		return false;
	}

	return true;
}

void UActorFactoryVdbVolume::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	UVdbVolume* VdbVolume = CastChecked<UVdbVolume>(Asset);

	// Change properties
	AVdbActor* VdbActor = CastChecked<AVdbActor>(NewActor);

	UVdbComponent* VdbComponent = VdbActor->GetVdbComponent();
	VdbComponent->UnregisterComponent();
	VdbComponent->VdbVolume = VdbVolume;
	VdbComponent->RegisterComponent();
}

void UActorFactoryVdbVolume::PostCreateBlueprint(UObject* Asset, AActor* CDO)
{
	if (Asset != NULL && CDO != NULL)
	{
		UVdbVolume* VdbVolume = CastChecked<UVdbVolume>(Asset);
		AVdbActor* VdbActor = CastChecked<AVdbActor>(CDO);
		UVdbComponent* VdbComponent = VdbActor->GetVdbComponent();
		VdbComponent->VdbVolume = VdbVolume;
	}
}
