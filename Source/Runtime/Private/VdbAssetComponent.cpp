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
	if (DensityVolume) Array.Add(DensityVolume.Get());
	if (TemperatureVolume) Array.Add(TemperatureVolume.Get());
	if (ColorVolume) Array.Add(ColorVolume.Get());
	return Array;
}

TArray<UVdbVolumeBase*> UVdbAssetComponent::GetVolumes()
{
	TArray<UVdbVolumeBase*> Array;
	if (DensityVolume) Array.Add(DensityVolume.Get());
	if (TemperatureVolume) Array.Add(TemperatureVolume.Get());
	if (ColorVolume) Array.Add(ColorVolume.Get());
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
	if (DensityVolume != nullptr)
	{
		return DensityVolume->GetVdbClass();
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
	if (DensityVolume)
	{
		Objects.Add(DensityVolume.Get());
	}
	if (TemperatureVolume)
	{
		Objects.Add(TemperatureVolume.Get());
	}
	if (ColorVolume)
	{
		Objects.Add(ColorVolume.Get());
	}
}

FVector3f UVdbAssetComponent::GetVolumeSize() const
{
	if (DensityVolume)
	{
		return FVector3f(DensityVolume->GetBounds(TargetFrameIndex).GetSize());
	}
	return FVector3f::OneVector;
}

FVector3f UVdbAssetComponent::GetVolumeOffset() const
{
	if (DensityVolume)
	{
		return FVector3f(DensityVolume->GetBounds(TargetFrameIndex).Min);
	}
	return FVector3f::ZeroVector;
}

FVector3f UVdbAssetComponent::GetVolumeUvScale() const
{
	if (DensityVolume)
	{
		const FIntVector& LargestVolume = DensityVolume->GetLargestVolume();
		const FVector3f& VolumeSize = DensityVolume->GetRenderInfos(TargetFrameIndex)->GetIndexSize();

		return FVector3f(	VolumeSize.X / (float)LargestVolume.X, 
							VolumeSize.Y / (float)LargestVolume.Y,
							VolumeSize.Z / (float)LargestVolume.Z);
	}
	return FVector3f::OneVector;
}

#if WITH_EDITOR

// Checks if we are inputing correct float volumes on float grids slots and vector volumes on vector grids slots.
// Also checks if other volumes are compatible with the principal (and only mandatory) density volume.
#define CHECK_VOLUMES_POST_EDIT(MemberName, CheckVector) \
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UVdbAssetComponent, MemberName)) \
	{ \
		if (MemberName && MemberName->IsVectorGrid() != CheckVector) \
		{ \
			MemberName = nullptr; \
			if (CheckVector) { UE_LOG(LogSparseVolumetrics, Error, TEXT("UVdbAssetComponent: %s only accepts vector volumes."), TEXT(#MemberName)); } \
			else { UE_LOG(LogSparseVolumetrics, Error, TEXT("UVdbAssetComponent: %s only accepts float volumes."), TEXT(#MemberName)); } \
		} \
		if (MemberName && DensityVolume && DensityVolume->GetLargestVolume() != MemberName->GetLargestVolume()) \
		{ \
			MemberName = nullptr; \
			UE_LOG(LogSparseVolumetrics, Error, TEXT("UVdbAssetComponent: %s must have same dimensions as the principal Density volume \"%s\"."), TEXT(#MemberName), *DensityVolume->GetName()); \
		} \
	}

void UVdbAssetComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	CHECK_VOLUMES_POST_EDIT(DensityVolume, false);
	CHECK_VOLUMES_POST_EDIT(TemperatureVolume, false);
	CHECK_VOLUMES_POST_EDIT(ColorVolume, true);
	CHECK_VOLUMES_POST_EDIT(FloatVolume1, false);
	CHECK_VOLUMES_POST_EDIT(FloatVolume2, false);
	CHECK_VOLUMES_POST_EDIT(FloatVolume3, false);
	CHECK_VOLUMES_POST_EDIT(FloatVolume4, false);
	CHECK_VOLUMES_POST_EDIT(VectorVolume1, true);
	CHECK_VOLUMES_POST_EDIT(VectorVolume2, true);
	CHECK_VOLUMES_POST_EDIT(VectorVolume3, true);
	CHECK_VOLUMES_POST_EDIT(VectorVolume4, true);

	if (DensityVolume)
	{
		// All VDB grids in a same Vdb Actor should match exactly.
		// The principal (and mandatory) Density volume drives all the others. If there is a mismatch 
		// between the Density volume and any other, reset the secondary volumes to avoid any potential GPU crash.
		if (TemperatureVolume && DensityVolume->GetLargestVolume() != TemperatureVolume->GetLargestVolume()) TemperatureVolume = nullptr;
		if (ColorVolume && DensityVolume->GetLargestVolume() != ColorVolume->GetLargestVolume()) ColorVolume = nullptr;
		if (FloatVolume1 && DensityVolume->GetLargestVolume() != FloatVolume1->GetLargestVolume()) FloatVolume1 = nullptr;
		if (FloatVolume2 && DensityVolume->GetLargestVolume() != FloatVolume2->GetLargestVolume()) FloatVolume2 = nullptr;
		if (FloatVolume3 && DensityVolume->GetLargestVolume() != FloatVolume3->GetLargestVolume()) FloatVolume3 = nullptr;
		if (FloatVolume4 && DensityVolume->GetLargestVolume() != FloatVolume4->GetLargestVolume()) FloatVolume4 = nullptr;
		if (VectorVolume1 && DensityVolume->GetLargestVolume() != VectorVolume1->GetLargestVolume()) VectorVolume1 = nullptr;
		if (VectorVolume2 && DensityVolume->GetLargestVolume() != VectorVolume2->GetLargestVolume()) VectorVolume2 = nullptr;
		if (VectorVolume3 && DensityVolume->GetLargestVolume() != VectorVolume3->GetLargestVolume()) VectorVolume3 = nullptr;
		if (VectorVolume4 && DensityVolume->GetLargestVolume() != VectorVolume4->GetLargestVolume()) VectorVolume4 = nullptr;
	}

	return Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

#undef LOCTEXT_NAMESPACE
