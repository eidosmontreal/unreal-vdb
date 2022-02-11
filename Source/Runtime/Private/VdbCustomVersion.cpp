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

#include "VdbCustomVersion.h"
#include "Serialization/CustomVersion.h"

const FGuid FVdbCustomVersion::GUID(0x39eb2bb0, 0x5f07e234, 0x4bed8840, 0x46a0a6c0);

// Register the custom version with core
FCustomVersionRegistration GRegisterVdbCustomVersion(FVdbCustomVersion::GUID, FVdbCustomVersion::LatestVersion, TEXT("VdbVersion"));
