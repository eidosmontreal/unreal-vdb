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

#include "VdbAssetComponent.h"

#include "VdbCommon.h"
#include "VdbVolumeBase.h"

#define LOCTEXT_NAMESPACE "VdbAssetComponent"

UVdbAssetComponent::UVdbAssetComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
UVdbAssetComponent::~UVdbAssetComponent() {}

TArray<const UVdbVolumeBase*> UVdbAssetComponent::GetConstVolumes() const
{ 
	TArray<const UVdbVolumeBase*> Array;
	if (PrimaryVolume) Array.Add(PrimaryVolume.Get());
	if (SecondaryVolume) Array.Add(SecondaryVolume.Get());
	return Array;
}

TArray<UVdbVolumeBase*> UVdbAssetComponent::GetVolumes()
{
	TArray<UVdbVolumeBase*> Array;
	if (PrimaryVolume) Array.Add(PrimaryVolume.Get());
	if (SecondaryVolume) Array.Add(SecondaryVolume.Get());
	return Array;
}

const FVolumeRenderInfos* UVdbAssetComponent::GetRenderInfos(const UVdbVolumeBase* VdbVolume) const
{
	if (VdbVolume != nullptr)
	{
		bool ValidSequence = VdbVolume->IsSequence();
		if (ValidSequence)
		{
			return VdbVolume->GetRenderInfos(CurrFrameIndex);
		}
		else
		{
			return VdbVolume->GetRenderInfos(0);
		}
	}
	else
	{
		return nullptr;
	}
}

EVdbClass UVdbAssetComponent::GetVdbClass() const
{
	if (PrimaryVolume != nullptr)
	{
		return PrimaryVolume->GetVdbClass();
	}
	else
	{
		return EVdbClass::Undefined;
	}
}

void UVdbAssetComponent::BroadcastFrameChanged(uint32 Frame)
{
	if (CurrFrameIndex != Frame)
	{
	CurrFrameIndex = Frame;
		TargetFrameIndex = CurrFrameIndex;
	OnFrameChanged.Broadcast(Frame);
	OnVdbChanged.Broadcast((int32)Frame);
}
}

void UVdbAssetComponent::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	if (PrimaryVolume)
	{
		Objects.Add(PrimaryVolume.Get());
	}
	if (SecondaryVolume)
	{
		Objects.Add(SecondaryVolume.Get());
	}
}

FVector3f UVdbAssetComponent::GetVolumeSize() const
{
	if (PrimaryVolume)
	{
		return FVector3f(PrimaryVolume->GetBounds(TargetFrameIndex).GetSize());
	}
	return FVector3f::OneVector;
}

FVector3f UVdbAssetComponent::GetVolumeOffset() const
{
	if (PrimaryVolume)
	{
		return FVector3f(PrimaryVolume->GetBounds(TargetFrameIndex).Min);
	}
	return FVector3f::ZeroVector;
}

FVector3f UVdbAssetComponent::GetVolumeUvScale() const
{
	if (PrimaryVolume)
	{
		const FIntVector& LargestVolume = PrimaryVolume->GetLargestVolume();
		const FVector3f& VolumeSize = PrimaryVolume->GetRenderInfos(TargetFrameIndex)->GetIndexSize();

		return FVector3f(	VolumeSize.X / (float)LargestVolume.X, 
							VolumeSize.Y / (float)LargestVolume.Y,
							VolumeSize.Z / (float)LargestVolume.Z);
	}
	return FVector3f::OneVector;
}

#undef LOCTEXT_NAMESPACE
