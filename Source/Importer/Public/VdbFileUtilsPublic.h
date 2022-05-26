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

class FString;

struct FVbdVoxelValue
{
	FVector CoordWorldSpace;
	float VoxelValue;
};

struct FVbdGridFrameInfos
{
	FBox WorldSpaceBBox;
	FBox IndexSpaceBBox;
	float VoxelSize;
	uint32 ActiveVoxelCount;
	float BackgroundValue;
	float MinValue;
	float MaxValue;
	TArray<FVbdVoxelValue> VoxelValues;
};

struct FVDBGridAnimationInfos
{
	FString GridName;
	FString GridType;
	FString GridClass;
	float MinValue;
	float MaxValue;
	FVector NbVoxels;
	FBox WorldSpaceBBox;
	TArray<FVbdGridFrameInfos> GridFrameInfosArray;
};

struct FVDBAnimationInfos
{
	float VoxelSize;
	FVector NbVoxels;
	FBox WorldSpaceBbox;
	TArray<FVDBGridAnimationInfos> GridAnimationInfosArray;
};

namespace VdbFileUtils
{
	VOLUMEIMPORTER_API bool GetVdbFrameInfos(const FString& Filepath, int32 FrameIndex, uint32 NbFramesInAnimation, FVDBAnimationInfos& AnimationInfos, bool FlipYandZ, bool LogTimes);
};