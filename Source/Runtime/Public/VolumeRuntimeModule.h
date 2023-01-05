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
#include "Modules/ModuleManager.h"
#include "VdbVolumeSequenceTrackHandler.h"

class FVolumeRuntimeModule : public IModuleInterface
{
	typedef TSharedPtr<class FVdbMaterialRendering, ESPMode::ThreadSafe> TRenderExtensionPtr;
	typedef TSharedPtr<class FVdbPrincipledRendering, ESPMode::ThreadSafe> TRenderPrincipledPtr;

public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static TRenderExtensionPtr GetRenderExtension(UTextureRenderTarget2D* DefaultRenderTarget);
	static TRenderPrincipledPtr GetRenderPrincipledMgr(UTextureRenderTarget2D* DefaultRenderTarget);

private:
	TRenderExtensionPtr GetOrCreateRenderExtension(UTextureRenderTarget2D* DefaultRenderTarget);
	TRenderPrincipledPtr GetOrCreateRenderPrincipledMgr(UTextureRenderTarget2D* DefaultRenderTarget);

	TRenderExtensionPtr VdbMaterialRenderExtension; // Regular renderer
	TRenderPrincipledPtr VdbPrincipledRenderExtension; // Experimentation renderer

	FVdbVolumeSequenceTrackHandler VdbVolumeSequenceTrackHandler;
};
