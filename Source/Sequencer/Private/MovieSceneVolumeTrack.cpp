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

#include "MovieSceneVolumeTrack.h"

#include "MovieSceneVolumeSection.h"
#include "Compilation/MovieSceneCompilerRules.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "MovieSceneVolumeTemplate.h"
#include "Compilation/IMovieSceneTemplateGenerator.h"
#include "MovieSceneVolumeTemplate.h"
#include "MovieScene.h"
#include "VolumeSequencerCommon.h"

#define LOCTEXT_NAMESPACE "MovieSceneVolumeTrack"


/* UMovieSceneVolumeTrack structors
 *****************************************************************************/

UMovieSceneVolumeTrack::UMovieSceneVolumeTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(124, 15, 124, 65);
#endif

	SupportedBlendTypes.Add(EMovieSceneBlendType::Absolute);

	EvalOptions.bCanEvaluateNearestSection = true;
	EvalOptions.bEvaluateInPreroll = true;
}


/* UMovieSceneVolumeTrack interface
 *****************************************************************************/

UMovieSceneSection* UMovieSceneVolumeTrack::AddNewAnimation(FFrameNumber KeyTime, UActorComponent* ActorComponent, FVolumeTrackHandlerBase* TrackHandler)
{
	UMovieSceneVolumeSection* NewSection = Cast<UMovieSceneVolumeSection>(CreateNewSection());
	{
		TObjectPtr<UObject> Volume = TrackHandler->GetVolume(ActorComponent);
		float VolumeDuration = TrackHandler->GetAnimationDuration(Volume);

		FFrameTime AnimationLength = VolumeDuration * GetTypedOuter<UMovieScene>()->GetTickResolution();
		int32 IFrameNumber = AnimationLength.FrameNumber.Value + (int)(AnimationLength.GetSubFrame() + 0.5f) + 1;
		NewSection->InitialPlacementOnRow(AnimationSections, KeyTime, IFrameNumber, INDEX_NONE);
		
		NewSection->Params.Volume = Volume;
		NewSection->Params.TrackHandlerId = TrackHandler->GetId();
	}

	AddSection(*NewSection);

	return NewSection;
}


TArray<UMovieSceneSection*> UMovieSceneVolumeTrack::GetAnimSectionsAtTime(FFrameNumber Time)
{
	TArray<UMovieSceneSection*> Sections;
	for (auto Section : AnimationSections)
	{
		if (Section->IsTimeWithinSection(Time))
		{
			Sections.Add(Section);
		}
	}

	return Sections;
}

FMovieSceneEvalTemplatePtr UMovieSceneVolumeTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneVolumeSectionTemplate(*CastChecked<UMovieSceneVolumeSection>(&InSection));
}

/* UMovieSceneTrack interface
 *****************************************************************************/


const TArray<UMovieSceneSection*>& UMovieSceneVolumeTrack::GetAllSections() const
{
	return AnimationSections;
}


bool UMovieSceneVolumeTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneVolumeSection::StaticClass();
}

UMovieSceneSection* UMovieSceneVolumeTrack::CreateNewSection()
{
	return NewObject<UMovieSceneVolumeSection>(this, NAME_None, RF_Transactional);
}


void UMovieSceneVolumeTrack::RemoveAllAnimationData()
{
	AnimationSections.Empty();
}


bool UMovieSceneVolumeTrack::HasSection(const UMovieSceneSection& Section) const
{
	return AnimationSections.Contains(&Section);
}


void UMovieSceneVolumeTrack::AddSection(UMovieSceneSection& Section)
{
	AnimationSections.Add(&Section);
}


void UMovieSceneVolumeTrack::RemoveSection(UMovieSceneSection& Section)
{
	AnimationSections.Remove(&Section);
}

void UMovieSceneVolumeTrack::RemoveSectionAt(int32 SectionIndex)
{
	AnimationSections.RemoveAt(SectionIndex);
}


bool UMovieSceneVolumeTrack::IsEmpty() const
{
	return AnimationSections.Num() == 0;
}

#if WITH_EDITORONLY_DATA

FText UMovieSceneVolumeTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Volumetric Animation");
}

#endif


#undef LOCTEXT_NAMESPACE
