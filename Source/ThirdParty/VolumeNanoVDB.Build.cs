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

using System.IO;
using UnrealBuildTool;

public class VolumeNanoVDB : ModuleRules
{
	private enum NVDBVersion
	{
		NVDB_32_3, // Corresponds to VDB_8_1
	}

	public VolumeNanoVDB(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// Must match OpenVDB version, used in same plugin, cf UOpenVDB.VDBVersion
		string VersionStr = "32.3";

		// Include NanoVDB (headers only)
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "nanovdb", VersionStr));
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "blosc"));

		PublicDefinitions.Add("NANOVDB_USE_ZIP");
		PublicDefinitions.Add("NANOVDB_USE_TBB");
		PublicDefinitions.Add("NANOVDB_USE_BLOSC");

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"zlib",
				"IntelTBB",
			}
			);
	}
}
