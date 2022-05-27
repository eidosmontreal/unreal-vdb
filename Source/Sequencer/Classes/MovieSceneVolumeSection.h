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

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "UObject/SoftObjectPath.h"
#include "MovieSceneVolumeSection.generated.h"

class FVolumeTrackHandlerBase;

USTRUCT()
 struct FMovieSceneVolumeParams
{
	GENERATED_BODY()

	FMovieSceneVolumeParams();

	/** Gets the animation sequence length, not modified by play rate */
	 float GetSequenceLength() const;
	/** The animation this section plays */
	UPROPERTY(VisibleAnywhere, Category = "Volume", DisplayName = "Volumetric Animation")
	TObjectPtr<UObject> Volume;

	/** The offset for the first loop of the animation clip */
	UPROPERTY(EditAnywhere, Category = "Volume")
	FFrameNumber FirstLoopStartFrameOffset;

	/** The offset into the beginning of the animation clip */
	UPROPERTY(EditAnywhere, Category = "Volume")
	FFrameNumber StartFrameOffset;

	/** The offset into the end of the animation clip */
	UPROPERTY(EditAnywhere, Category = "Volume")
	FFrameNumber EndFrameOffset;

	/** The playback rate of the animation clip */
	UPROPERTY(EditAnywhere, Category = "Volume")
	float PlayRate;

	/** Reverse the playback of the animation clip */
	UPROPERTY(EditAnywhere, Category = "Animation")
	uint32 bReverse : 1;

	// Sharing the volume animation track editor between multiple modules
	UPROPERTY()
	uint32 TrackHandlerId;

    FVolumeTrackHandlerBase* GetTrackHandler() const;
};

/**
 * Movie scene section that control volumetric animation playback
 */
UCLASS(MinimalAPI)
class UMovieSceneVolumeSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ShowOnlyInnerProperties))
	FMovieSceneVolumeParams Params;

	/** Get Frame Time as Animation Time*/
	virtual float MapTimeToAnimation(float ComponentDuration, FFrameTime InPosition, FFrameRate InFrameRate) const;

protected:
	//~ UMovieSceneSection interface
	virtual TOptional<TRange<FFrameNumber> > GetAutoSizeRange() const override;
	virtual void TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft, bool bDeleteKeys) override;
	virtual UMovieSceneSection* SplitSection(FQualifiedFrameTime SplitTime, bool bDeleteKeys) override;
	virtual void GetSnapTimes(TArray<FFrameNumber>& OutSnapTimes, bool bGetSectionBorders) const override;
	virtual TOptional<FFrameTime> GetOffsetTime() const override;

	/** ~UObject interface */
	virtual void Serialize(FArchive& Ar) override;

private:

	//~ UObject interface

#if WITH_EDITOR

	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
public:
	float PreviousPlayRate;

#endif

};
