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

using UnrealBuildTool;

public class VolumeImporter : ModuleRules
{
	public VolumeImporter(ReadOnlyTargetRules Target) : base(Target)
	{
		bEnableExceptions = true;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// For boost:: and TBB:: code
		bUseRTTI = true;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// ... add private dependencies that you statically link with here ...	
				"Core",
				"VolumeRuntime",
				"CoreUObject",
				"MainFrame",
				"UnrealEd",
				"Slate",
				"SlateCore",
				"InputCore",
				"Engine",
				"Projects",
				"ToolMenus",
				"AssetTools",
				"EditorStyle",
				"VolumeOpenVDB",
				"VolumeNanoVDB"
			}
			);
	}
}
