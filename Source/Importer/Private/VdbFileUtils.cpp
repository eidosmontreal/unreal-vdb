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

#include "VdbFileUtils.h"
#include "VdbFileUtilsPublic.h"

#include "Misc/Paths.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "VdbImporterWindow.h"

THIRD_PARTY_INCLUDES_START
#pragma warning(push)
#pragma warning(disable:4583)
#pragma warning(disable:4582)
#include <nanovdb/util/OpenToNanoVDB.h> // converter from OpenVDB to NanoVDB (includes NanoVDB.h and GridManager.h)
#include <nanovdb/util/IO.h> // this is required to read (and write) NanoVDB files on the host
#pragma warning(pop)
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogVdbFiles, Log, All);

nanovdb::GridHandle<> VdbFileUtils::LoadVdbFromFile(const FString& Filepath, const FName& GridName)
{
	std::string path(TCHAR_TO_UTF8(*Filepath));
	std::string gridName(TCHAR_TO_UTF8(*GridName.ToString()));
	return nanovdb::io::readGrid(path, gridName);
}

// Heavily inspired by OpenVDB samples
TArray<FVdbGridInfoPtr> VdbFileUtils::ParseVdbFromFile(const FString& Path)
{
	TArray<FVdbGridInfoPtr> VdbGrids;

	FString Filename = FPaths::GetCleanFilename(Path);
	FString Extension = FPaths::GetExtension(Filename, false);
	const std::string path(TCHAR_TO_UTF8(*Path));

	auto CoordAsString = [](const auto& ijk) // openvdb::Coord or nanovdb::Coord
	{
		std::ostringstream ostr;
		ostr << ijk[0] << "x" << ijk[1] << "x" << ijk[2];
		return FString(ostr.str().c_str());
	};

	auto BytesAsString = [](uint64 n)
	{
		std::ostringstream ostr;
		ostr << std::setprecision(3);
		if (n >> 30) {
			ostr << (double(n) / double(uint64(1) << 30)) << "GB";
		}
		else if (n >> 20) {
			ostr << (double(n) / double(uint64(1) << 20)) << "MB";
		}
		else if (n >> 10) {
			ostr << (double(n) / double(uint64(1) << 10)) << "KB";
		}
		else {
			ostr << n << "B";
		}
		return FString(ostr.str().c_str());
	};

	auto SizeAsString = [](uint64 n)
	{
		std::ostringstream ostr;
		ostr << std::setprecision(3);
		if (n < 1000) {
			ostr << n;
		}
		else if (n < 1000000) {
			ostr << (double(n) / 1.0e3) << "K";
		}
		else if (n < 1000000000) {
			ostr << (double(n) / 1.0e6) << "M";
		}
		else {
			ostr << (double(n) / 1.0e9) << "G";
		}
		return FString(ostr.str().c_str());
	};

	if (Extension == "vdb")
	{
		openvdb::GridPtrVecPtr grids;
		openvdb::MetaMap::Ptr meta;

		openvdb::initialize();
		openvdb::io::File file(path);
		try
		{
			file.open();
			grids = file.getGrids();
			meta = file.getMetadata();
			file.close();
		}
		catch (openvdb::Exception& e)
		{
			UE_LOG(LogVdbFiles, Error, TEXT("Could not read VDB file %s:\n%s"), *Path, *FString(e.what()));
			return TArray<FVdbGridInfoPtr>();
		}

		for (const openvdb::GridBase::ConstPtr grid : *grids)
		{
			if (!grid) continue;

			openvdb::CoordBBox bbox = grid->evalActiveVoxelBoundingBox();

			FVdbGridInfoPtr GridInfo = MakeShared<FVdbGridInfo>();
			GridInfo->GridName = FName(grid->getName().c_str());
			GridInfo->Type = FString(grid->valueType().c_str());
			GridInfo->Class = FString(grid->gridClassToString(grid->getGridClass()).c_str());
			GridInfo->Dimensions = CoordAsString(bbox.extents());
			GridInfo->ActiveVoxels = SizeAsString(grid->activeVoxelCount());
			//GridInfo->MemorySize = BytesAsString(grid->memUsage()); // in bytes. Returns wrong values, unless it's been preloaded. Remove until fixed

			VdbGrids.Add(GridInfo);
		}
	}
	else if (Extension == "nvdb")
	{
		try
		{
			std::vector<nanovdb::io::GridMetaData> MetaDatas = nanovdb::io::readGridMetaData(path);
			for (nanovdb::io::GridMetaData& MetaData : MetaDatas)
			{
				FVdbGridInfoPtr GridInfo = MakeShared<FVdbGridInfo>();
				GridInfo->GridName = FName(MetaData.gridName.c_str());
				GridInfo->Type = FString(nanovdb::toStr(MetaData.gridType));
				GridInfo->Class = FString(nanovdb::toStr(MetaData.gridClass));
				GridInfo->Dimensions = CoordAsString(MetaData.indexBBox.max() - MetaData.indexBBox.min());
				GridInfo->ActiveVoxels = SizeAsString(MetaData.voxelCount);
				//GridInfo->MemorySize = BytesAsString(MetaData.memUsage()); // in bytes.

				VdbGrids.Add(GridInfo);
			}
		}
		catch (const std::exception& e)
		{
			UE_LOG(LogVdbFiles, Error, TEXT("Could not read NVDB file %s:\n%s"), *Path, *FString(e.what()));
			return TArray<FVdbGridInfoPtr>();
		}
	}

	return VdbGrids;
}

bool VdbFileUtils::GetVdbFrameInfos(const FString& Filepath, int32 FrameIndex, uint32 NbFramesInAnimation, FVDBAnimationInfos& AnimationInfos, bool FlipYandZ, bool LogTimes)
{	
	double TotalTime = 0;
	double TimeForInitialization = 0;	
	double TimeForReadGrids = 0;
	double TimeForProcessGrids = 0;
	FDurationTimer TotalTimeTimer(TotalTime);

	openvdb::GridPtrVecPtr grids;
	openvdb::MetaMap::Ptr meta;

	FDurationTimer InitializationTimer(TimeForInitialization);
	openvdb::initialize();
	InitializationTimer.Stop();

	FDurationTimer ReadGridsTimer(TimeForReadGrids);
	const std::string ConvertedFilepath(TCHAR_TO_UTF8(*Filepath));
	openvdb::io::File file(ConvertedFilepath);
	try
	{
		file.open();
		grids = file.getGrids();
		meta = file.getMetadata();
		file.close();
	}
	catch (openvdb::Exception& e)
	{
		UE_LOG(LogVdbFiles, Error, TEXT("Could not read VDB file %s:\n%s"), *Filepath, *FString(e.what()));
		return false;
	}
	ReadGridsTimer.Stop();

	if (FrameIndex == 0)
	{
		AnimationInfos.GridAnimationInfosArray.Reserve(grids->size());
		AnimationInfos.GridAnimationInfosArray.Reset();

		for (openvdb::GridBase::Ptr grid : *grids)
		{
			if (!grid)
			{
				continue;
			}

			// Init grid
			auto& GridAnimationInfos = AnimationInfos.GridAnimationInfosArray.AddDefaulted_GetRef();
			GridAnimationInfos.GridName = FString(grid->getName().c_str());
			GridAnimationInfos.GridType = FString(grid->valueType().c_str());
			GridAnimationInfos.GridClass = FString(grid->gridClassToString(grid->getGridClass()).c_str());
			GridAnimationInfos.MinValue = TNumericLimits<float>::Max();
			GridAnimationInfos.MaxValue = TNumericLimits<float>::Lowest();
			GridAnimationInfos.WorldSpaceBBox.Init();

			// Reserve space for all frames in the animation
			GridAnimationInfos.GridFrameInfosArray.Reserve(NbFramesInAnimation);

			// Make sure grid transform is linear
			openvdb::FloatGrid::Ptr FloatGrid = openvdb::gridPtrCast<openvdb::FloatGrid>(grid);
			const openvdb::math::Transform& GridTransform = grid->constTransform();
			if (!GridTransform.isLinear())
			{
				UE_LOG(LogVdbFiles, Error, TEXT("VDB file %s contains unsupported non-linear transform"), *Filepath);
				return false;
			}

			// Voxel size must be constant across all grids
			float VoxelSize = GridTransform.voxelSize().x();
			if ((GridTransform.voxelSize().x() != GridTransform.voxelSize().y()) || (GridTransform.voxelSize().x() != GridTransform.voxelSize().z()))
			{
				UE_LOG(LogVdbFiles, Error, TEXT("VDB file %s contains non-square voxels. This is not supported"), *Filepath);
				return false;
			}

			if (AnimationInfos.GridAnimationInfosArray.Num() == 1)
			{
				AnimationInfos.VoxelSize = VoxelSize;
			} 
			else
			{
				if (AnimationInfos.VoxelSize != VoxelSize)
				{
					UE_LOG(LogVdbFiles, Error, TEXT("VDB file %s contains grid with different voxel size. This is not supported"), *Filepath);
					return false;
				}
			}
		}
	}

	FDurationTimer ProcessGridsTimer(TimeForProcessGrids);
	uint32 GridIndex = 0;
	for (openvdb::GridBase::Ptr grid : *grids)
	{
		if (!grid)
		{
			continue;
		}			

		FVDBGridAnimationInfos& GridAnimationInfos = AnimationInfos.GridAnimationInfosArray[GridIndex];
		GridIndex++;

		FVbdGridFrameInfos& GridFrameInfos = GridAnimationInfos.GridFrameInfosArray.AddDefaulted_GetRef();

		// We only support NanoVDB LevelSets and FogVolumes which are floating point grids
		if (grid->isType<openvdb::FloatGrid>())
		{
			openvdb::FloatGrid::Ptr FloatGrid = openvdb::gridPtrCast<openvdb::FloatGrid>(grid);
			GridFrameInfos.ActiveVoxelCount = (uint32)grid->activeVoxelCount();
			GridFrameInfos.BackgroundValue = FloatGrid->background();

			// Perform some validation
			const openvdb::math::Transform& GridTransform = grid->constTransform();
			if (!GridTransform.isLinear())
			{
				UE_LOG(LogVdbFiles, Error, TEXT("VDB file %s contains unsupported non-linear transform"), *Filepath);
				return false;
			}

			float VoxelSize = GridTransform.voxelSize().x();
			if ((GridTransform.voxelSize().x() != GridTransform.voxelSize().y()) || (GridTransform.voxelSize().x() != GridTransform.voxelSize().z()))
			{
				UE_LOG(LogVdbFiles, Error, TEXT("VDB file %s contains non-square voxels. This is not supported"), *Filepath);
				return false;
			}
			if (AnimationInfos.VoxelSize != VoxelSize)
			{
				UE_LOG(LogVdbFiles, Error, TEXT("VDB file %s contains grid with different voxel size. This is not supported"), *Filepath);
				return false;
			}

			// Setup index space bounds for current frame of current grid
			openvdb::CoordBBox IndexSpaceBBox = grid->evalActiveVoxelBoundingBox();
			GridFrameInfos.IndexSpaceBBox.Min.X = IndexSpaceBBox.min().x();
			GridFrameInfos.IndexSpaceBBox.Min.Y = IndexSpaceBBox.min().y();
			GridFrameInfos.IndexSpaceBBox.Min.Z = IndexSpaceBBox.min().z();
			GridFrameInfos.IndexSpaceBBox.Max.X = IndexSpaceBBox.max().x();
			GridFrameInfos.IndexSpaceBBox.Max.Y = IndexSpaceBBox.max().y();			
			GridFrameInfos.IndexSpaceBBox.Max.Z = IndexSpaceBBox.max().z();

			// Setup world space bounds for current frame of current grid
			openvdb::math::Vec3d WorldSpaceBBoxMin = GridTransform.indexToWorld(IndexSpaceBBox.min().asVec3d());
			openvdb::math::Vec3d WorldSpaceBBoxMax = GridTransform.indexToWorld(IndexSpaceBBox.max().asVec3d());
			GridFrameInfos.WorldSpaceBBox = FBox(FVector(WorldSpaceBBoxMin.x(), WorldSpaceBBoxMin.y(), WorldSpaceBBoxMin.z()), 
													FVector(WorldSpaceBBoxMax.x(), WorldSpaceBBoxMax.y(), WorldSpaceBBoxMax.z()));
			if (FlipYandZ)
			{
				Swap(GridFrameInfos.WorldSpaceBBox.Min.Y, GridFrameInfos.WorldSpaceBBox.Min.Z);
				Swap(GridFrameInfos.WorldSpaceBBox.Max.Y, GridFrameInfos.WorldSpaceBBox.Max.Z);
			}

			GridAnimationInfos.WorldSpaceBBox += GridFrameInfos.WorldSpaceBBox;

			// Extract all active voxel values and update current frame's min/max value
			GridFrameInfos.MinValue = TNumericLimits<float>::Max();
			GridFrameInfos.MaxValue = TNumericLimits<float>::Lowest();

			GridFrameInfos.VoxelValues.Reserve(GridFrameInfos.ActiveVoxelCount);
			for (openvdb::FloatGrid::ValueOnIter iter = FloatGrid->beginValueOn(); iter; ++iter)
			{
				openvdb::Coord CurCoord = iter.getCoord();
				openvdb::math::Vec3d PosWorldSpace = GridTransform.indexToWorld(CurCoord);

				// Extract voxel value
				FVbdVoxelValue& VoxelValue = GridFrameInfos.VoxelValues.AddDefaulted_GetRef();
				VoxelValue.VoxelValue = iter.getValue();				
				VoxelValue.CoordWorldSpace.X = PosWorldSpace.x();
				VoxelValue.CoordWorldSpace.Y = PosWorldSpace.y();
				VoxelValue.CoordWorldSpace.Z = PosWorldSpace.z();
				if (FlipYandZ)
				{
					Swap(VoxelValue.CoordWorldSpace.Y, VoxelValue.CoordWorldSpace.Z);
				}

				// Update current frame's min/max value
				GridFrameInfos.MinValue = FMath::Min(GridFrameInfos.MinValue, VoxelValue.VoxelValue);
				GridFrameInfos.MaxValue = FMath::Max(GridFrameInfos.MaxValue, VoxelValue.VoxelValue);
			}

			// Update animation's min/max values
			GridAnimationInfos.MinValue = FMath::Min(GridAnimationInfos.MinValue, GridFrameInfos.MinValue);
			GridAnimationInfos.MaxValue = FMath::Max(GridAnimationInfos.MaxValue, GridFrameInfos.MaxValue);
		}
		else
		{
			UE_LOG(LogVdbFiles, 
					Error, 
					TEXT("Cannot import grid %s (of type %s) from file %s. We only support float (scalar) grids yet."),
					*FString(grid->getName().c_str()), 
					*FString(grid->valueType().c_str()),
					*Filepath);
		}
	}
	ProcessGridsTimer.Stop();

	TotalTimeTimer.Stop();
	if (LogTimes)
	{
		UE_LOG(LogVdbFiles, 
			   Error,
			   TEXT("GetVdbFrameInfos: %f (Init=%f, Read=%f, Process=%f) - %s"), 
			   (float)(TotalTime * 1000), 
			   (float)(TimeForInitialization * 1000), 
			   (float)(TimeForReadGrids * 1000), 
			   (float)(TimeForProcessGrids * 1000), 
			   *Filepath);
	}

	return true;
}

openvdb::GridBase::Ptr VdbFileUtils::OpenVdb(const FString& Path, const FName& GridName)
{
	openvdb::initialize();

	std::string path(TCHAR_TO_UTF8(*Path));
	openvdb::io::File file(path);

	file.open(); // Open the file. This reads the file header, but not any grids.

	std::string gridName(TCHAR_TO_UTF8(*GridName.ToString()));
	openvdb::GridBase::Ptr baseGrid = file.readGrid(gridName);

	file.close();

	return baseGrid;
}

nanovdb::GridHandle<> VdbFileUtils::LoadVdb(const FString& Path, const FName& GridName, nanovdb::GridType Type)
{
	FString Filename = FPaths::GetCleanFilename(Path);
	FString Extension = FPaths::GetExtension(Filename, false);
	if (Extension == "vdb")
	{
		openvdb::GridBase::Ptr VdbGrid = OpenVdb(Path, GridName);

		using openvdb_Vec4fTree = typename openvdb::tree::Tree4<openvdb::Vec4f, 5, 4, 3>::Type;
		using openvdb_Vec4fGrid = openvdb::Grid<openvdb_Vec4fTree>;

		// We only support NanoVDB LevelSets and FogVolumes which are floating point grids
		if (VdbGrid->isType<openvdb::FloatGrid>())
		{
			openvdb::FloatGrid::Ptr FloatGrid = openvdb::gridPtrCast<openvdb::FloatGrid>(VdbGrid);
			switch (Type)
			{
			case nanovdb::GridType::Fp4: return nanovdb::openToNanoVDB<nanovdb::HostBuffer, openvdb::FloatTree, nanovdb::Fp4>(*FloatGrid);
			case nanovdb::GridType::Fp8: return nanovdb::openToNanoVDB<nanovdb::HostBuffer, openvdb::FloatTree, nanovdb::Fp8>(*FloatGrid);
			case nanovdb::GridType::Fp16: return nanovdb::openToNanoVDB<nanovdb::HostBuffer, openvdb::FloatTree, nanovdb::Fp16>(*FloatGrid);
			case nanovdb::GridType::FpN: return nanovdb::openToNanoVDB<nanovdb::HostBuffer, openvdb::FloatTree, nanovdb::FpN>(*FloatGrid);
			default: return nanovdb::openToNanoVDB(VdbGrid);
			}
		}
		else if (VdbGrid->isType<openvdb::VectorGrid>() || VdbGrid->isType<openvdb_Vec4fGrid>())
		{
			return nanovdb::openToNanoVDB(VdbGrid);
		}
		else
		{
			UE_LOG(LogVdbFiles, Error, TEXT("Cannot import grid %s (of type %s) from file %s. We only support float (scalar) grids yet."), 
					*FString(VdbGrid->getName().c_str()), *FString(VdbGrid->valueType().c_str()), *Filename);
		}
	}
	else if (Extension == "nvdb")
	{
		return VdbFileUtils::LoadVdbFromFile(Path, GridName);
	}

	return nanovdb::GridHandle<>();
}