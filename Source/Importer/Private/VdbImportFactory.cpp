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

#include "VdbImportFactory.h"
#include "VdbVolumeStatic.h"
#include "VdbVolumeSequence.h"
#include "VdbImporterOptions.h"
#include "VdbImporterWindow.h"
#include "VdbFileUtils.h"

#include "AssetImportTask.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/FeedbackContext.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Subsystems/ImportSubsystem.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogVdbImporter, Log, All);

#define LOCTEXT_NAMESPACE "VdbImporterFactory"

#if WITH_EDITORONLY_DATA

namespace VdbImporterImpl
{
	bool ShowOptionsWindow(const FString& Filepath, const FString& PackagePath, UVdbImporterOptions& ImporterOptions, TArray<FVdbGridInfoPtr>& GridsInfo)
	{
		TSharedPtr<SWindow> ParentWindow;
		if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
		{
			IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
			ParentWindow = MainFrame.GetParentWindow();
		}

		TSharedRef<SWindow> Window = SNew(SWindow).Title(LOCTEXT("VdbImporterWindowTitle", "VDB Import Options")).SizingRule(ESizingRule::Autosized);

		TSharedPtr<SVdbImporterWindow> OptionsWindow;
		Window->SetContent(
			SAssignNew(OptionsWindow, SVdbImporterWindow)
			.ImportOptions(&ImporterOptions)
			.WidgetWindow(Window)
			.FileNameText(FText::Format(LOCTEXT("VdbImportOptionsFileName", "  Import File  :    {0}"), FText::FromString(FPaths::GetCleanFilename(Filepath))))
			.FilePathText(FText::FromString(Filepath))
			.VdbGridsInfo(GridsInfo)
			.PackagePathText(FText::Format(LOCTEXT("VdbFImportOptionsPackagePath", "  Import To   :    {0}"), FText::FromString(PackagePath))));

		FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
		return OptionsWindow->ShouldImport();
	}

	auto GetFrameNumber = [](const FString& Filename)
	{
		const FString FilenameWithoutPath = FPaths::GetBaseFilename(Filename);
		const int32 LastNonDigitIndex = FilenameWithoutPath.FindLastCharByPredicate([](TCHAR Letter) { return !FChar::IsDigit(Letter); }) + 1;
		const FString StrNumberPart = FilenameWithoutPath.RightChop(LastNonDigitIndex);

		int32 Number = -1;
		if (StrNumberPart.IsNumeric())
		{
			TTypeFromString<int32>::FromString(Number, *StrNumberPart);
		}
		return Number;
	};
}

UVdbImportFactory::UVdbImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditAfterNew = false;
	bEditorImport = true;
	bText = false;

	SupportedClass = nullptr;	// UVdbImportFactory supports both volume and sequence

	Formats.Add(TEXT("vdb;OpenVDB format"));
	Formats.Add(TEXT("nvdb;NanoVDB format"));
}

bool UVdbImportFactory::DoesSupportClass(UClass* Class)
{
	return (Class == UVdbVolumeStatic::StaticClass() || Class == UVdbVolumeSequence::StaticClass());
}

UClass* UVdbImportFactory::ResolveSupportedClass()
{
	// It's ok to ignore UVdbVolumeSequence, the important thing is to have "SupportedClass == nullptr" and to implement "DoesSupportClass" properly
	return UVdbVolumeStatic::StaticClass();
}

nanovdb::GridType ToGridType(EQuantizationType Type)
{
	switch (Type)
	{
	case EQuantizationType::Fp4: return nanovdb::GridType::Fp4;
	case EQuantizationType::Fp8: return nanovdb::GridType::Fp8;
	case EQuantizationType::Fp16: return nanovdb::GridType::Fp16;
	case EQuantizationType::FpN: return nanovdb::GridType::FpN;
	default: return nanovdb::GridType::Unknown;
	}
}

UObject* UVdbImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename,
	const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, Parms);

	// try reading & parsing file
	TArray<FVdbGridInfoPtr> GridsInfo = VdbFileUtils::ParseVdbFromFile(Filename);

	// if error, early quit:
	if (GridsInfo.IsEmpty())
	{
		// Failed to read the file info, fail the import
		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);
		return nullptr;
	}

	bOutOperationCanceled = false;

	TArray<FString> SortedVBDFilenames = ExtractVDBFilenamesForSequence(Filename);
	TStrongObjectPtr<UVdbImporterOptions> ImporterOptions(NewObject<UVdbImporterOptions>(GetTransientPackage(), TEXT("VDB Importer Options")));
	ImporterOptions->IsSequence = (SortedVBDFilenames.Num() > 1);
	ImporterOptions->ImportAsSequence = ImporterOptions->IsSequence;
	if (ImporterOptions->IsSequence) 
	{
		ImporterOptions->FirstFrame = VdbImporterImpl::GetFrameNumber(SortedVBDFilenames[0]);
		ImporterOptions->LastFrame = VdbImporterImpl::GetFrameNumber(SortedVBDFilenames.Last());
	}

	if (!IsAutomatedImport())
	{
		const FString Filepath = FPaths::ConvertRelativePathToFull(Filename);
		const FString PackagePath = InParent->GetPathName();

		if (!VdbImporterImpl::ShowOptionsWindow(Filepath, PackagePath, *ImporterOptions, GridsInfo))
		{
			bOutOperationCanceled = true;
			return nullptr;
		}
	}
	else
	{
		if (AssetImportTask->Options != nullptr)
		{
			UVdbImporterOptions* Options = Cast<UVdbImporterOptions>(AssetImportTask->Options);
			if (Options == nullptr)
			{
				UE_LOG(LogVdbImporter, Display, TEXT("The options set in the Asset Import Task are not of type UVdbImporterOptions and will be ignored"));
			}
			else
			{
				ImporterOptions->Quantization = Options->Quantization;
				ImporterOptions->ImportAsSequence = Options->ImportAsSequence;
			}
		}
	}

	TArray<UObject*> ResultAssets;
	if (!bOutOperationCanceled)
	{
		if (ImporterOptions->ImportAsSequence)
		{
			if (ImporterOptions->FirstFrame > ImporterOptions->LastFrame)
			{
				UE_LOG(LogVdbImporter, Error, TEXT("Sequence first frame (%d) must be lower than last frame (%d). Invalid case, abort import."), ImporterOptions->FirstFrame, ImporterOptions->LastFrame);
				bOutOperationCanceled = true;
				return nullptr;
			}

			int32 NumGridsToProcess = GridsInfo.FilterByPredicate([](const FVdbGridInfoPtr& Info){ return Info->ShouldImport; }).Num();
			
			FScopedSlowTask ImportTask(SortedVBDFilenames.Num() * NumGridsToProcess, LOCTEXT("ImportingSeq", "Importing Sequence(s)"));
			ImportTask.MakeDialog(true);

			for (FVdbGridInfoPtr GridInfo : GridsInfo)
			{
				// Parse sequence for each grid
				if (GridInfo->ShouldImport)
				{
					TArray<uint8> StreamedDataTempBytes;

					FString SequenceName = InName.ToString();
					{
						int32 IndexLastUnderscore;
						if (SequenceName.FindLastChar('_', IndexLastUnderscore) && !SequenceName.Contains("_seq"))
						{
							SequenceName = SequenceName.LeftChop(SequenceName.Len() - IndexLastUnderscore);
							if (GridsInfo.Num() > 1) 
							{
								SequenceName += "_seq_" + GridInfo->GridName.ToString();
							}
							else
							{
								SequenceName += "_seq";
							}
						}
					}
			
					FString VdbPath = FPaths::GetPath(Filename) + "\\";
					FName SequenceFName(SequenceName);
					UVdbVolumeSequence* VolumeSequence = NewObject<UVdbVolumeSequence>(InParent, UVdbVolumeSequence::StaticClass(), SequenceFName, Flags);			

					for (int32 VDBFileIndex = 0; VDBFileIndex < SortedVBDFilenames.Num(); VDBFileIndex++)
					{
						if (ImportTask.ShouldCancel())
						{
							bOutOperationCanceled = true;
							return nullptr;
						}
						ImportTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("ImportingSeqUpdate", "Importing Grid \"{0}\", frame {1}/{2}"), FText::FromName(GridInfo->GridName), VDBFileIndex, SortedVBDFilenames.Num()));

						const FString& VDBFilenameWithoutPath = SortedVBDFilenames[VDBFileIndex];
						int32 FrameNumber = VdbImporterImpl::GetFrameNumber(VDBFilenameWithoutPath);
						if (FrameNumber < ImporterOptions->FirstFrame || FrameNumber > ImporterOptions->LastFrame)
							continue;

						FString VDBFilename = VdbPath + VDBFilenameWithoutPath;

						// try reading & parsing file
						TArray<FVdbGridInfoPtr> FrameGridsInfo = VdbFileUtils::ParseVdbFromFile(VDBFilename);

						FVdbGridInfoPtr* FrameGridInfo = FrameGridsInfo.FindByPredicate([&GridInfo](const FVdbGridInfoPtr& Other) { return Other->GridName == GridInfo->GridName; } );
						if (FrameGridInfo)
						{
							nanovdb::GridType GridType = ToGridType(ImporterOptions->Quantization);
							nanovdb::GridHandle<> gridHandle = VdbFileUtils::LoadVdb(VDBFilename, FrameGridInfo->Get()->GridName, GridType);

							if (gridHandle)
							{
								// Setup per frame infos that will always be loaded with the sequence
								VolumeSequence->AddFrame(gridHandle, ImporterOptions->Quantization);

								// Setup stream data
								nanovdb::GridHandle<nanovdb::HostBuffer> NanoGridHandle = MoveTemp(gridHandle);
								StreamedDataTempBytes.Reset();
								FMemoryWriter StreamedDataWriter(StreamedDataTempBytes, /*bIsPersistent=*/ true);
								StreamedDataWriter << NanoGridHandle;

								FVdbSequenceChunk* Chunk = new(VolumeSequence->GetChunks()) FVdbSequenceChunk();
								Chunk->FirstFrame = FrameNumber;
								Chunk->LastFrame = FrameNumber;
								Chunk->DataSize = StreamedDataTempBytes.Num();
								Chunk->BulkData.Lock(LOCK_READ_WRITE);
								void* NewChunkData = Chunk->BulkData.Realloc(StreamedDataTempBytes.Num());
								FMemory::Memcpy(NewChunkData, StreamedDataTempBytes.GetData(), StreamedDataTempBytes.Num());
								Chunk->BulkData.Unlock();
							}
						}
						else
						{
							UE_LOG(LogVdbImporter, Warning, TEXT("Sequence frame %d has an invalid VDB grid. This will not stop import, but the result sequence will be incomplete."), FrameNumber);
						}

					}

					VolumeSequence->FinalizeImport(Filename);
					ResultAssets.Add(VolumeSequence);
				}
			}
		}
		else
		{
			// Import as volume. All grids can be imported
			for (FVdbGridInfoPtr GridInfo : GridsInfo)
			{
				if (GridInfo->ShouldImport)
				{
					nanovdb::GridType GridType = ToGridType(ImporterOptions->Quantization);
					nanovdb::GridHandle<> gridHandle = VdbFileUtils::LoadVdb(Filename, GridInfo->GridName, GridType);

					if (gridHandle)
					{
						FName NewName(InName.ToString() + "_" + GridInfo->GridName.ToString());

						bool UseNewName = GridsInfo.Num() > 1 && !InName.ToString().Contains(GridInfo->GridName.ToString());

						UVdbVolumeStatic* Vol = NewObject<UVdbVolumeStatic>(InParent, UVdbVolumeStatic::StaticClass(), UseNewName ? NewName : InName, Flags);
						Vol->Import(MoveTemp(gridHandle), ImporterOptions->Quantization);
						Vol->GetAssetImportData()->Update(Filename);

						ResultAssets.Add(Vol);
					}
				}
			}
		}

		AdditionalImportedObjects.Reserve(ResultAssets.Num());
		for (UObject* Object : ResultAssets)
		{
			if (Object)
			{
				GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, Object);
				Object->MarkPackageDirty();
				Object->PostEditChange();
				AdditionalImportedObjects.Add(Object);
			}
		}
	}

	return (ResultAssets.Num() > 0) ? ResultAssets[0] : nullptr;
}

bool UVdbImportFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename);
	if (Extension == TEXT("vdb") || Extension == TEXT("nvdb"))
	{
		return true;
	}

	return false;
}

void UVdbImportFactory::CleanUp()
{
	// cleanup any resources/buffers
	Super::CleanUp();
}

TArray<FString> UVdbImportFactory::ExtractVDBFilenamesForSequence(const FString& Filename)
{
	const FString InputFilePath = FPaths::GetPath(Filename);
	const FString BaseFilenameWithoutPath = FPaths::GetBaseFilename(Filename);
	const int32 NumberStartIndex = BaseFilenameWithoutPath.FindLastCharByPredicate([](TCHAR Letter) { return !FChar::IsDigit(Letter); }) + 1;
	const FString BaseName = BaseFilenameWithoutPath.Left(NumberStartIndex);

	TArray<FString> VBDFilenames;
	IFileManager::Get().FindFiles(VBDFilenames, *InputFilePath, TEXT("*.vdb"));
	VBDFilenames = VBDFilenames.FilterByPredicate([BaseName](const FString& Name) { return Name.Contains(BaseName); });

	int32 MinimumNumber = MAX_int32;
	for (FString& UnsortedFilename : VBDFilenames)
	{
		int32 FrameNumber = VdbImporterImpl::GetFrameNumber(UnsortedFilename);
		if (FrameNumber == -1)
		{
			return TArray<FString>();
		}
		MinimumNumber = FMath::Min(MinimumNumber, FrameNumber);
	}

	const int32 NumFrames = VBDFilenames.Num();

	// Collect and sort by names files to merge
	TArray<FString> VBDFilenamesSorted;
	VBDFilenamesSorted.SetNum(NumFrames);

	for (FString& UnsortedFilename : VBDFilenames)
	{
		FString CurrentFilenameWithoutExtension = FPaths::GetBaseFilename(UnsortedFilename);

		// Extract frame number
		int32 Number = -1;
		if (NumberStartIndex <= CurrentFilenameWithoutExtension.Len() - 1)
		{
			FString StrNumberPart = CurrentFilenameWithoutExtension.RightChop(NumberStartIndex);
			if (StrNumberPart.IsNumeric())
			{
				TTypeFromString<int32>::FromString(Number, *StrNumberPart);

				if (Number < 0)
				{
					return TArray<FString>();
				}
			} 
			else
			{
				return TArray<FString>();
			}
		}
		else
		{
			return TArray<FString>();
		}

		int32 Index = Number - MinimumNumber;
		if (Index >= 0 && Index < NumFrames)
		{
			VBDFilenamesSorted[Index] = std::move(UnsortedFilename);
		}
		else
		{
			UE_LOG(LogVdbImporter, Error, TEXT("VDB importer: vdb file numbers are not contiguous, import aborted."));
			return TArray<FString>();
		}
	}

	return VBDFilenamesSorted;
}

#endif

#undef LOCTEXT_NAMESPACE