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
#include "UObject/ObjectPtr.h"

class UActorComponent;


class FVolumeTrackHandlerBase
{
public:
	virtual FText GetLabelText() const = 0;
	virtual FText GetToolTipText() const = 0;
	virtual FText GetHoverText() const = 0;
	virtual uint32 GetId() const = 0;
	virtual UActorComponent* TryCastAsVolumeComponent(const UObject* Obj) const = 0;
	virtual const UClass* GetVolumeComponentClass() const = 0;
	virtual const UClass* GetVolumeAssetClass() const = 0;
	virtual TObjectPtr<UObject> GetVolume(const UActorComponent* Comp) const = 0;
	virtual void SetVolume(UActorComponent* Comp, const TObjectPtr<UObject>& Volume) = 0;
	virtual int32 GetFrameAtTime(const TObjectPtr<UObject>& Volume, float AnimTime) const = 0;
	virtual float GetAnimationDuration(const TObjectPtr<UObject>& Volume) const = 0;
	virtual bool GetManualTick(const UActorComponent* Comp) const = 0;
	virtual void SetManualTick(UActorComponent* Comp, bool ManualTick) const = 0;
	virtual void ResetAnimationTime(UActorComponent* Comp) const = 0;
	virtual void TickAtThisTime(UActorComponent* Comp, const float Time, bool bInIsRunning, bool bInBackwards, bool bInIsLooping) const = 0;

	bool IsSupportedVolumeComponentClass(const UClass* ObjectClass) const;
	UActorComponent* TryExtractVolumeComponent(UObject* BoundObject) const;
};

class FVolumeTrackHandlersScoped
{
public: 
	FVolumeTrackHandlersScoped();
	~FVolumeTrackHandlersScoped();

	TArray<FVolumeTrackHandlerBase*>& TrackHandlers;
};

FVolumeTrackHandlersScoped GetVolumeTrackHandlers();
VOLUMESEQUENCER_API void RegisterVolumeTrackHandler(FVolumeTrackHandlerBase* TrackHandler);
VOLUMESEQUENCER_API void UnregisterVolumeTrackHandler(FVolumeTrackHandlerBase* TrackHandler);
VOLUMESEQUENCER_API FVolumeTrackHandlerBase* GetVolumeTrackHandlerFromId(uint32 TrackHandlerId);
TPair<UActorComponent*, FVolumeTrackHandlerBase*> TryExtractVolumeComponent(UObject* BoundObject);

