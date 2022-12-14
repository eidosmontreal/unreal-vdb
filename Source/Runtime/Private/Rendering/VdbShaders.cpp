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

#include "VdbShaders.h"

IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FVdbShaderParams, "VdbParams", SceneTextures);
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FVdbPrincipledShaderParams, "VdbPrincipledParams");

IMPLEMENT_MATERIAL_SHADER_TYPE(, FVdbShaderVS, TEXT("/Plugin/VdbVolume/Private/VdbVertexShader.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_LevelSet, TEXT("/Plugin/VdbVolume/Private/VdbLevelSet.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_LevelSet_Translucent, TEXT("/Plugin/VdbVolume/Private/VdbLevelSet.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_LevelSet_Translucent_EnvLight, TEXT("/Plugin/VdbVolume/Private/VdbLevelSet.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_FogVolume, TEXT("/Plugin/VdbVolume/Private/VdbFogVolume.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_FogVolume_Trilinear, TEXT("/Plugin/VdbVolume/Private/VdbFogVolume.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_FogVolume_EnvLight, TEXT("/Plugin/VdbVolume/Private/VdbFogVolume.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_FogVolume_EnvLight_Trilinear, TEXT("/Plugin/VdbVolume/Private/VdbFogVolume.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_FogVolume_Color, TEXT("/Plugin/VdbVolume/Private/VdbFogVolume.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_FogVolume_Color_Trilinear, TEXT("/Plugin/VdbVolume/Private/VdbFogVolume.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_FogVolume_Color_EnvLight, TEXT("/Plugin/VdbVolume/Private/VdbFogVolume.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_FogVolume_Color_EnvLight_Trilinear, TEXT("/Plugin/VdbVolume/Private/VdbFogVolume.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_FogVolume_Blackbody, TEXT("/Plugin/VdbVolume/Private/VdbFogVolume.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_FogVolume_Blackbody_Trilinear, TEXT("/Plugin/VdbVolume/Private/VdbFogVolume.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_FogVolume_Blackbody_EnvLight, TEXT("/Plugin/VdbVolume/Private/VdbFogVolume.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_FogVolume_Blackbody_EnvLight_Trilinear, TEXT("/Plugin/VdbVolume/Private/VdbFogVolume.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_FogVolume_Blackbody_Color, TEXT("/Plugin/VdbVolume/Private/VdbFogVolume.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_FogVolume_Blackbody_Color_Trilinear, TEXT("/Plugin/VdbVolume/Private/VdbFogVolume.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_FogVolume_Blackbody_Color_EnvLight, TEXT("/Plugin/VdbVolume/Private/VdbFogVolume.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FVdbShaderPS_FogVolume_Blackbody_Color_EnvLight_Trilinear, TEXT("/Plugin/VdbVolume/Private/VdbFogVolume.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FVdbPrincipledVS, "/Plugin/VdbVolume/Private/VdbPrincipled.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FVdbPrincipledPS, "/Plugin/VdbVolume/Private/VdbPrincipled.usf", "MainPS", SF_Pixel);