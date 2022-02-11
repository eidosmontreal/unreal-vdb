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
#include "VolumeSequencerCommon.h"

class FVdbVolumeSequenceTrackHandler : public FVolumeTrackHandlerBase
{
public:
	FText GetLabelText() const override;
	FText GetToolTipText() const override;
	FText GetHoverText() const override;
	uint32 GetId() const override;
	UActorComponent* TryCastAsVolumeComponent(const UObject* ObjectToTest) const override;
	TObjectPtr<UObject> GetVolume(const UActorComponent* Comp) const override;
	void SetVolume(UActorComponent* Comp, const TObjectPtr<UObject>& Volume) override;
	bool GetManualTick(const UActorComponent* Comp) const override;
	void SetManualTick(UActorComponent* Comp, bool ManualTick) const override;
	void ResetAnimationTime(UActorComponent* Comp) const override;
	void TickAtThisTime(UActorComponent* Comp, const float Time, bool bInIsRunning, bool bInBackwards, bool bInIsLooping) const override;
	int32 GetFrameAtTime(const TObjectPtr<UObject>& Volume, float AnimTime) const override;
	float GetAnimationDuration(const TObjectPtr<UObject>& Volume) const override;
	const UClass* GetVolumeComponentClass() const override;
	const UClass* GetVolumeAssetClass() const override;
};