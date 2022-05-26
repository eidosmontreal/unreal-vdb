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
#include "UObject/NoExportTypes.h"
#include "VdbCommon.h"

#include "VdbImporterOptions.generated.h"

UCLASS(config = EditorPerProjectUserSettings, HideCategories = (DebugProperty))
class UVdbImporterOptions : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(config, EditAnywhere, Category = Compression,
		meta = (DisplayName = "Quantization Type", ToolTip = "VDB float grids can be quantized (compressed). Not applicable with Vector Grids. Use this for lighter models in memory. Note that compressed models do not render necessarily faster."))
	EQuantizationType Quantization;

	UPROPERTY(config, VisibleDefaultsOnly, Category = Sequence,
		meta = (ToolTip = "True if file is part of a sequence in the same folder."))
	bool IsSequence = false;

	UPROPERTY(config, EditAnywhere, Category = Sequence,
		meta = (ToolTip = "Import as a sequence or as individual frame.", EditCondition = "IsSequence", EditConditionHides))
	bool ImportAsSequence = false;

	UPROPERTY(config, EditAnywhere, Category = Sequence,
		meta = (ToolTip = "Index of first detected frame.", EditCondition = "IsSequence && ImportAsSequence", EditConditionHides))
	int32 FirstFrame = 0;

	UPROPERTY(config, EditAnywhere, Category = Sequence,
		meta = (ToolTip = "Index of last detected frame.", EditCondition = "IsSequence && ImportAsSequence", EditConditionHides))
	int32 LastFrame = 0;
};
