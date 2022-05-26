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

#include "VdbVolumeSequenceTrackHandler.h"

#include "VdbSequenceComponent.h"
#include "VdbVolumeSequence.h"


FText FVdbVolumeSequenceTrackHandler::GetLabelText() const
{
	return NSLOCTEXT("Sequencer", "AddVdbSequence", "Vdb Sequence");
}

FText FVdbVolumeSequenceTrackHandler::GetToolTipText() const
{
	return NSLOCTEXT("Sequencer", "AddVdbSequenceTooltip", "Adds a Vdb sequence track");
}

FText FVdbVolumeSequenceTrackHandler::GetHoverText() const
{
	return NSLOCTEXT("Sequencer", "VdbSequenceHoverText", "Vdb Sequence");
}

uint32 FVdbVolumeSequenceTrackHandler::GetId() const
{
	return 'VdbS';
}

UActorComponent* FVdbVolumeSequenceTrackHandler::TryCastAsVolumeComponent(const UObject* ObjectToTest) const
{
	const UVdbSequenceComponent* VdbSequenceComponent = Cast<const UVdbSequenceComponent>(ObjectToTest);
	return (UVdbSequenceComponent*)VdbSequenceComponent;
}

TObjectPtr<UObject> FVdbVolumeSequenceTrackHandler::GetVolume(const UActorComponent* Comp) const
{
	const UVdbSequenceComponent* VdbSequenceComponent = CastChecked<const UVdbSequenceComponent>(Comp);
	return VdbSequenceComponent->GetPrimarySequence();
}

void FVdbVolumeSequenceTrackHandler::SetVolume(UActorComponent* Comp, const TObjectPtr<UObject>& Volume)
{
	// not supported
}

bool FVdbVolumeSequenceTrackHandler::GetManualTick(const UActorComponent* Comp) const
{
	const UVdbSequenceComponent* VdbSequenceComponent = CastChecked<const UVdbSequenceComponent>(Comp);
	return VdbSequenceComponent->GetManualTick();
}

void FVdbVolumeSequenceTrackHandler::SetManualTick(UActorComponent* Comp, bool ManualTick) const
{
	UVdbSequenceComponent* VdbSequenceComponent = CastChecked<UVdbSequenceComponent>(Comp);
	VdbSequenceComponent->SetManualTick(ManualTick);
}

void FVdbVolumeSequenceTrackHandler::ResetAnimationTime(UActorComponent* Comp) const
{
	UVdbSequenceComponent* VdbSequenceComponent = CastChecked<UVdbSequenceComponent>(Comp);
	VdbSequenceComponent->ResetAnimationTime();
}

void FVdbVolumeSequenceTrackHandler::TickAtThisTime(UActorComponent* Comp, const float Time, bool bInIsRunning, bool bInBackwards, bool bInIsLooping) const
{
	UVdbSequenceComponent* VdbSequenceComponent = CastChecked<UVdbSequenceComponent>(Comp);
	VdbSequenceComponent->TickAtThisTime(Time, bInIsRunning, bInBackwards, bInIsLooping);
}

int32 FVdbVolumeSequenceTrackHandler::GetFrameAtTime(const TObjectPtr<UObject>& Volume, float AnimTime) const
{
	const UVdbVolumeSequence* VdbSequence = CastChecked<const UVdbVolumeSequence>(Volume.Get());
	return VdbSequence->GetFrameIndexFromTime(AnimTime);
}

float FVdbVolumeSequenceTrackHandler::GetAnimationDuration(const TObjectPtr<UObject>& Volume) const
{
	const UVdbVolumeSequence* VdbSequence = CastChecked<const UVdbVolumeSequence>(Volume.Get());
	return VdbSequence->GetDurationInSeconds();
}

const UClass* FVdbVolumeSequenceTrackHandler::GetVolumeComponentClass() const
{
	return UVdbSequenceComponent::StaticClass();
}

const UClass* FVdbVolumeSequenceTrackHandler::GetVolumeAssetClass() const
{
	return UVdbVolumeSequence::StaticClass();
}


