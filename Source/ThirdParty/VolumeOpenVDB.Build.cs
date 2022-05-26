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

public class VolumeOpenVDB : ModuleRules
{
	private enum VDBVersion
	{
		VDB_8_1,
	}

	public VolumeOpenVDB(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// For boost:: and TBB:: code
		bEnableUndefinedIdentifierWarnings = false;
		bUseRTTI = true;

		// https://www.openvdb.org/documentation/doxygen/build.html#buildGuide
		// "Blosc and ZLib are treated as required dependencies in these install instructions"

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"IntelTBB",
				"zlib",
				// ... add other public dependencies that you statically link with here ...
			}
			);

		// Boost (headers only) and OpenVDB (needs lib)
		PublicIncludePaths.Add(ModuleDirectory);

		// OpenVDB (8.X.X) library
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Set which version to use.
			string VersionStr = "8.1"; // latest

			string DeployDirName = Path.Combine(ModuleDirectory, "Deploy");
			string VersionDir = Path.Combine("x64", VersionStr);
			string LibDirName = Path.Combine(DeployDirName, "lib", VersionDir);

			PublicDefinitions.Add("OPENVDB_STATICLIB");
			PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "openvdb", VersionStr));
			PublicAdditionalLibraries.Add(Path.Combine(LibDirName, "libopenvdb.lib"));

			// Add Blosc, used during OpenVDB decompression
			string DllName = "blosc.dll";
			// Delay-load the DLL, so we can load it from the right place first
			PublicDelayLoadDLLs.Add(DllName); 
			// Ensure that the DLL is staged along with the executable
			RuntimeDependencies.Add("$(BinaryOutputDir)/" + DllName, Path.Combine(DeployDirName, "bin", VersionDir, DllName), StagedFileType.NonUFS);
			PublicAdditionalLibraries.Add(Path.Combine(LibDirName, "blosc.lib"));

			// Deploy blosc dependency dlls
			DllName = "snappy.dll";
			RuntimeDependencies.Add("$(BinaryOutputDir)/" + DllName, Path.Combine(DeployDirName, "bin", VersionDir, DllName), StagedFileType.NonUFS);
			DllName = "lz4.dll";
			RuntimeDependencies.Add("$(BinaryOutputDir)/" + DllName, Path.Combine(DeployDirName, "bin", VersionDir, DllName), StagedFileType.NonUFS);
			DllName = "zlib1.dll";
			RuntimeDependencies.Add("$(BinaryOutputDir)/" + DllName, Path.Combine(DeployDirName, "bin", VersionDir, DllName), StagedFileType.NonUFS);
			DllName = "zstd.dll";
			RuntimeDependencies.Add("$(BinaryOutputDir)/" + DllName, Path.Combine(DeployDirName, "bin", VersionDir, DllName), StagedFileType.NonUFS);
		}
		else
		{
			string Err = "We don't support OpenVDB on other platforms yet.";
			System.Console.WriteLine(Err);
			throw new BuildException(Err);
		}
	}
}
