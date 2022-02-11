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
#include "UObject/Object.h"

class FRDGBuilder;
class FRDGTexture;
class FSceneView;

UENUM(BlueprintType)
enum class EVdbDenoiserMethod : uint8
{
	None			UMETA(DisplayName = "No denoising"),
	GaussianBlur    UMETA(DisplayName = "Gaussian blur"),
	BoxBlur			UMETA(DisplayName = "Box blur"),
	// Add your own method here

	Count			UMETA(Hidden, DisplayName = "Invalid")
};

namespace VdbDenoiser
{
	FRDGTexture* ApplyDenoising(FRDGBuilder& GraphBuilder, FRDGTexture* InputTexture, const FSceneView* View, const FIntRect& ViewportRect, EVdbDenoiserMethod Method);
};
