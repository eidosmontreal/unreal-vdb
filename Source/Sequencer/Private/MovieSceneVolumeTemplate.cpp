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

#include "MovieSceneVolumeTemplate.h"

#include "Compilation/MovieSceneCompilerRules.h"
#include "Components/ActorComponent.h"
#include "Evaluation/MovieSceneEvaluation.h"
#include "IMovieScenePlayer.h"
#include "UObject/ObjectKey.h"
#include "VolumeSequencerCommon.h"


DECLARE_CYCLE_STAT(TEXT("Volumetric animation Evaluate"), MovieSceneEval_Volume_Evaluate, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Volumetric animation Token Execute"), MovieSceneEval_Volume_TokenExecute, STATGROUP_MovieSceneEval);

/** Used to set Manual Tick back to previous when outside section */
struct FPreAnimatedVolumeTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const
	{
		struct FToken : IMovieScenePreAnimatedToken
		{
			FToken(UObject& Object)
			{
				// Cache this object's current update flag and animation mode
				TPair<UActorComponent*, FVolumeTrackHandlerBase*> ComponentHandlerPair = TryExtractVolumeComponent(&Object);
				UActorComponent* ActorComponent = ComponentHandlerPair.Key;
				FVolumeTrackHandlerBase* TrackHandler = ComponentHandlerPair.Value;
				bInManualTick = TrackHandler->GetManualTick(ActorComponent);
			}

			virtual void RestoreState(UObject& ObjectToRestore, const UE::MovieScene::FRestoreStateParams& Params)
			{
				TPair<UActorComponent*, FVolumeTrackHandlerBase*> ComponentHandlerPair = TryExtractVolumeComponent(&ObjectToRestore);
				UActorComponent* ActorComponent = ComponentHandlerPair.Key;
				FVolumeTrackHandlerBase* TrackHandler = ComponentHandlerPair.Value;
				TrackHandler->SetManualTick(ActorComponent, bInManualTick);
				TrackHandler->ResetAnimationTime(ActorComponent);
			}

			bool bInManualTick;
		};

		return FToken(Object);
	}

	static FMovieSceneAnimTypeID GetAnimTypeID()
	{
		return TMovieSceneAnimTypeID<FPreAnimatedVolumeTokenProducer>();
	}
};


/** A movie scene execution token that executes a volumetric animation */
struct FVolumeExecutionToken
	: IMovieSceneExecutionToken

{
	FVolumeExecutionToken(const FMovieSceneVolumeSectionTemplateParameters &InParams) :
		Params(InParams)
	{}

	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_Volume_TokenExecute)
		if (Operand.ObjectBindingID.IsValid())
		{
			for (TWeakObjectPtr<> WeakObj : Player.FindBoundObjects(Operand))
			{
				if (UObject* Obj = WeakObj.Get())
				{
					TPair<UActorComponent*, FVolumeTrackHandlerBase*> ComponentHandlerPair = TryExtractVolumeComponent(Obj);
					UActorComponent* ActorComponent = ComponentHandlerPair.Key;
					FVolumeTrackHandlerBase* TrackHandler = ComponentHandlerPair.Value;
					
					if (ActorComponent && ActorComponent->IsRegistered())
					{						
						// Set the Volume on the component only if it's set and valid in the Params
						if (Params.Volume && (Params.Volume != TrackHandler->GetVolume(ActorComponent)))
						{
							TrackHandler->SetVolume(ActorComponent, Params.Volume);
						}
						else
						{
							// It could be unset if the Params was referencing a transient Volume
							// In that case, use the Volume that is set on the component
							Params.Volume = TrackHandler->GetVolume(ActorComponent);
							Params.TrackHandlerId = TrackHandler->GetId();
						}

						Player.SavePreAnimatedState(*ActorComponent, FPreAnimatedVolumeTokenProducer::GetAnimTypeID(), FPreAnimatedVolumeTokenProducer());

						TrackHandler->SetManualTick(ActorComponent, true);

						// calculate the time at which to evaluate the animation
						float VolumeDuration = TrackHandler->GetAnimationDuration(Params.Volume);
						float EvalTime = Params.MapTimeToAnimation(VolumeDuration, Context.GetTime(), Context.GetFrameRate());
						TrackHandler->TickAtThisTime(ActorComponent, EvalTime, Context.GetStatus() == EMovieScenePlayerStatus::Playing, Params.bReverse, true);
					}
				}
			}
		}
	}

	FMovieSceneVolumeSectionTemplateParameters Params;
};

FMovieSceneVolumeSectionTemplate::FMovieSceneVolumeSectionTemplate(const UMovieSceneVolumeSection& InSection)
	: Params(const_cast<FMovieSceneVolumeParams&> (InSection.Params), InSection.GetInclusiveStartFrame(), InSection.GetExclusiveEndFrame())
{
}

//We use a token here so we can set the manual tick state back to what it was previously when outside this section.
//This is similar to how Skeletal Animation evaluation also works.
void FMovieSceneVolumeSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_Volume_Evaluate)
		ExecutionTokens.Add(FVolumeExecutionToken(Params));
}

float FMovieSceneVolumeSectionTemplateParameters::MapTimeToAnimation(float ComponentDuration, FFrameTime InPosition, FFrameRate InFrameRate) const
{
	const float SequenceLength = ComponentDuration;
	const FFrameTime AnimationLength = SequenceLength * InFrameRate;
	const int32 LengthInFrames = AnimationLength.FrameNumber.Value + (int)(AnimationLength.GetSubFrame() + 0.5f) + 1;
	//we only play end if we are not looping, and assuming we are looping if Length is greater than default length;
	const bool bLooping = (SectionEndTime.Value - SectionStartTime.Value + StartFrameOffset + EndFrameOffset) > LengthInFrames;

	InPosition = FMath::Clamp(InPosition, FFrameTime(SectionStartTime), FFrameTime(SectionEndTime - 1));

	const float SectionPlayRate = PlayRate;
	const float AnimPlayRate = FMath::IsNearlyZero(SectionPlayRate) ? 1.0f : SectionPlayRate;

	const float FirstLoopSeqLength = SequenceLength - InFrameRate.AsSeconds(FirstLoopStartFrameOffset + StartFrameOffset + EndFrameOffset);
	const float SeqLength = SequenceLength - InFrameRate.AsSeconds(StartFrameOffset + EndFrameOffset);

	float AnimPosition = FFrameTime::FromDecimal((InPosition - SectionStartTime).AsDecimal() * AnimPlayRate) / InFrameRate;
	AnimPosition += InFrameRate.AsSeconds(FirstLoopStartFrameOffset);
	if (SeqLength > 0.f && (bLooping || !FMath::IsNearlyEqual(AnimPosition, SeqLength, 1e-4f)))
	{
		AnimPosition = FMath::Fmod(AnimPosition, SeqLength);
	}
	AnimPosition += InFrameRate.AsSeconds(StartFrameOffset);
	if (bReverse)
	{
		AnimPosition = SequenceLength - AnimPosition;
	}

	return AnimPosition;
}
