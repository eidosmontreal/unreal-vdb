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

#include "VolumeSequencerCommon.h"

#include "GameFramework/Actor.h"

TArray<FVolumeTrackHandlerBase*> g_TrackHandlers;
FCriticalSection g_TrackHandlersCriticalSection;

FVolumeTrackHandlersScoped::FVolumeTrackHandlersScoped()
	: TrackHandlers(g_TrackHandlers)
{
	g_TrackHandlersCriticalSection.Lock();
}

FVolumeTrackHandlersScoped::~FVolumeTrackHandlersScoped()
{
	g_TrackHandlersCriticalSection.Unlock();
}

FVolumeTrackHandlersScoped GetVolumeTrackHandlers()
{
	return FVolumeTrackHandlersScoped();
}

void RegisterVolumeTrackHandler(FVolumeTrackHandlerBase* TrackHandler)
{
	FScopeLock ScopeLock(&g_TrackHandlersCriticalSection);
	g_TrackHandlers.AddUnique(TrackHandler);
}

void UnregisterVolumeTrackHandler(FVolumeTrackHandlerBase* TrackHandler)
{
	FScopeLock ScopeLock(&g_TrackHandlersCriticalSection);
	g_TrackHandlers.Remove(TrackHandler);
}

FVolumeTrackHandlerBase* GetVolumeTrackHandlerFromId(uint32 TrackHandlerId)
{
	if (TrackHandlerId == 0)
	{
		return nullptr;
	}

	FScopeLock ScopeLock(&g_TrackHandlersCriticalSection);
	for (FVolumeTrackHandlerBase* TrackHandler : g_TrackHandlers)
	{
		check(TrackHandler->GetId() != 0);

		if (TrackHandler->GetId() == TrackHandlerId)
		{
			return TrackHandler;
		}
	}

	return nullptr;
}

TPair<UActorComponent*, FVolumeTrackHandlerBase*> TryExtractVolumeComponent(UObject* BoundObject)
{
	FScopeLock ScopeLock(&g_TrackHandlersCriticalSection);
	for (FVolumeTrackHandlerBase* TrackHandler : g_TrackHandlers)
	{
		UActorComponent* ActorComponent = TrackHandler->TryExtractVolumeComponent(BoundObject);
		if (ActorComponent != nullptr)
		{
			return TPair<UActorComponent*, FVolumeTrackHandlerBase*>(ActorComponent, TrackHandler);
		}
	}

	return TPair<UActorComponent*, FVolumeTrackHandlerBase*>(nullptr, nullptr);
}

bool FVolumeTrackHandlerBase::IsSupportedVolumeComponentClass(const UClass* ObjectClass) const
{
	return ObjectClass->IsChildOf(GetVolumeComponentClass());
}

UActorComponent* FVolumeTrackHandlerBase::TryExtractVolumeComponent(UObject* BoundObject) const
{
	// Maybe BoundObject is an actor that contains a volumetric animation component?
	if (AActor* Actor = Cast<AActor>(BoundObject))
	{
		for (UActorComponent* Component : Actor->GetComponents())
		{
			UActorComponent* ExtractedComponent = TryCastAsVolumeComponent(Component);
			if (ExtractedComponent != nullptr)
			{
				check(ExtractedComponent == Component);
				return ExtractedComponent;
			}
		}
	}
	else
	{
		// Maybe BoundObject is directly a volumetric animation component?
		UActorComponent* ExtractedComponent = TryCastAsVolumeComponent(BoundObject);
		if (ExtractedComponent != nullptr)
		{
			return ExtractedComponent;
		}
	}

	return nullptr;
}


