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

#include "VdbToVolumeTextureFactory.h"
#include "VdbVolumeStatic.h"

#include "Styling/SlateBrush.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Engine/VolumeTexture.h"
#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVdbToVolume, Log, All);
DEFINE_LOG_CATEGORY(LogVdbToVolume);

#define LOCTEXT_NAMESPACE "VolumeTextureFactory"

UVdbToVolumeTextureFactory::UVdbToVolumeTextureFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UVolumeTexture::StaticClass();
}

FText UVdbToVolumeTextureFactory::GetDisplayName() const
{
	return LOCTEXT("VdbToVolumeTextureFactoryDescription", "Vdb To Volume Texture");
}

bool UVdbToVolumeTextureFactory::ConfigureProperties()
{
	return true;
}

template<typename T>
bool ReadTypedGrid(UVdbVolumeStatic* VdbVolumeStatic, UVolumeTexture* VolumeTex, const FIntVector& IndexSize, const FIntVector& IndexMin, const FIntVector& TexSize)
{
	const nanovdb::NanoGrid<T>* Grid = VdbVolumeStatic->GetNanoGrid<T>();
	if (!Grid)
		return false;

	float Background = Grid->tree().background();
	float Maximum = Grid->tree().root().maximum();
	float Minimum = Grid->tree().root().minimum();
	UE_LOG(LogVdbToVolume, Log, TEXT("\tMinimum value %f, Maximum value %f, Background %f."), Minimum, Maximum, Background);

	auto Acc = Grid->getAccessor(); // create an accessor for fast access to multiple values

	uint8* MipData = reinterpret_cast<uint8*>(VolumeTex->Source.LockMip(0));
	for (int32 Z = 0; Z < IndexSize.Z; ++Z)
	{
		for (int32 Y = 0; Y < IndexSize.Y; ++Y)
		{
			uint8* Row = &MipData[Y * TexSize.X + Z * TexSize.X * TexSize.Y];
			for (int32 X = 0; X < IndexSize.X; ++X)
			{
				nanovdb::Coord xyz(X + IndexMin.X, Y + IndexMin.Y, Z + IndexMin.Z);
				float Value = Acc.getValue(xyz) / Maximum;
				Value = FMath::Max(0.f, FMath::Min(1.f, Value)); // Clamp values to 0-1
				Row[X] = uint8(Value * 255);
			}
		}
	}
	VolumeTex->Source.UnlockMip(0);

	return true;
}

template<typename T>
bool ReadVectorGrid(UVdbVolumeStatic* VdbVolumeStatic, UVolumeTexture* VolumeTex, const FIntVector& IndexSize, const FIntVector& IndexMin, const FIntVector& TexSize)
{
	const nanovdb::NanoGrid<T>* Grid = VdbVolumeStatic->GetNanoGrid<T>();
	if (!Grid)
		return false;

	auto Acc = Grid->getAccessor(); // create an accessor for fast access to multiple values

	FFloat16Color* MipData = reinterpret_cast<FFloat16Color*>(VolumeTex->Source.LockMip(0));
	for (int32 Z = 0; Z < IndexSize.Z; ++Z)
	{
		for (int32 Y = 0; Y < IndexSize.Y; ++Y)
		{
			FFloat16Color* Row = &MipData[Y * TexSize.X + Z * TexSize.X * TexSize.Y];
			for (int32 X = 0; X < IndexSize.X; ++X)
			{
				nanovdb::Coord xyz(X + IndexMin.X, Y + IndexMin.Y, Z + IndexMin.Z);
				auto Value = Acc.getValue(xyz);

				if (typeid(T) == typeid(nanovdb::Vec3f))
				{
					Row[X] = FFloat16Color(FLinearColor(Value[0], Value[1], Value[2], 1.0f));
				}
				else if (typeid(T) == typeid(nanovdb::Vec4f))
				{
					Row[X] = FFloat16Color(FLinearColor(Value[0], Value[1], Value[2], Value[3]));
				}
			}
		}
	}
	VolumeTex->Source.UnlockMip(0);

	return true;
}

/**
* Simplistic way to convert a single NanoVDB grid to a volume texture.
* We only support 8 bits volume texture for now.
* NanoVDB values are normalized (divided by max value) and clamped to 0-1 , ignoring negative values.
* Works best for fog volumes (which usually don't have negative values). Doesn't work well with narrow-band level sets.
*/
UObject* UVdbToVolumeTextureFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UVolumeTexture* NewVolumeTexture = NewObject<UVolumeTexture>(InParent, Name, Flags);
	NewVolumeTexture->MipGenSettings = TMGS_FromTextureGroup;
	NewVolumeTexture->NeverStream = true;
	NewVolumeTexture->CompressionNone = false;

	if (InitialVdbVolume && InitialVdbVolume->IsValid())
	{
		const nanovdb::GridMetaData* MetaData = InitialVdbVolume->GetMetaData();
		const nanovdb::BBox<nanovdb::Vec3R>& IndexBBox = MetaData->indexBBox();

		FIntVector IndexMin = FIntVector(IndexBBox.min()[0], IndexBBox.min()[1], IndexBBox.min()[2]);
		FIntVector IndexMax = FIntVector(IndexBBox.max()[0], IndexBBox.max()[1], IndexBBox.max()[2]);
		FIntVector IndexSize = IndexMax - IndexMin;
		const bool PowerOfTwo = false;
		FIntVector TexSize(
			PowerOfTwo ? FMath::RoundUpToPowerOfTwo(IndexSize.X) : IndexSize.X,
			PowerOfTwo ? FMath::RoundUpToPowerOfTwo(IndexSize.Y) : IndexSize.Y,
			PowerOfTwo ? FMath::RoundUpToPowerOfTwo(IndexSize.Z) : IndexSize.Z);

		UE_LOG(LogVdbToVolume, Log, 
			TEXT("Converting VDB grid \"%s\" (type: %s, class: %s) to texture volume (%dx%dx%d)."), 
			*FString(MetaData->shortGridName()), 
			*FString(nanovdb::toStr(MetaData->gridType())), 
			*FString(nanovdb::toStr(MetaData->gridClass())),
			TexSize.X, TexSize.Y, TexSize.Z);

		if (TexSize.X > 512 || TexSize.Y > 512 || TexSize.Z > 512)
		{
			UE_LOG(LogVdbToVolume, Warning, TEXT("Trying to convert a big volume. This process might be slow or even crash."));
		}

		NewVolumeTexture->SRGB = false;
		NewVolumeTexture->MipGenSettings = TMGS_NoMipmaps;
		NewVolumeTexture->CompressionNone = true;
		if (MetaData->gridType() == nanovdb::GridType::Vec3f ||
			MetaData->gridType() == nanovdb::GridType::Vec4f)
		{
			NewVolumeTexture->Source.Init(TexSize.X, TexSize.Y, TexSize.Z, 1, TSF_RGBA16F, nullptr);
		}
		else
		{
			NewVolumeTexture->Source.Init(TexSize.X, TexSize.Y, TexSize.Z, 1, TSF_G8, nullptr);
		}

		bool Success = false;
		
		switch(MetaData->gridType())
			{
		case nanovdb::GridType::Float:
			Success = ReadTypedGrid<float>(InitialVdbVolume, NewVolumeTexture, IndexSize, IndexMin, TexSize);
			break;
		case nanovdb::GridType::Fp4:
			Success = ReadTypedGrid<nanovdb::Fp4>(InitialVdbVolume, NewVolumeTexture, IndexSize, IndexMin, TexSize);
			break;
		case nanovdb::GridType::Fp8:
			Success = ReadTypedGrid<nanovdb::Fp8>(InitialVdbVolume, NewVolumeTexture, IndexSize, IndexMin, TexSize);
			break;
		case nanovdb::GridType::Fp16:
			Success = ReadTypedGrid<nanovdb::Fp16>(InitialVdbVolume, NewVolumeTexture, IndexSize, IndexMin, TexSize);
			break;
		case nanovdb::GridType::FpN:
			Success = ReadTypedGrid<nanovdb::FpN>(InitialVdbVolume, NewVolumeTexture, IndexSize, IndexMin, TexSize);
			break;
		case nanovdb::GridType::Vec3f:
			Success = ReadVectorGrid<nanovdb::Vec3f>(InitialVdbVolume, NewVolumeTexture, IndexSize, IndexMin, TexSize);
			break;
		case nanovdb::GridType::Vec4f:
			Success = ReadVectorGrid<nanovdb::Vec4f>(InitialVdbVolume, NewVolumeTexture, IndexSize, IndexMin, TexSize);
			break;
		default:
			break;
			}

		if (!Success)
		{
			UE_LOG(LogVdbToVolume, Error, TEXT("Could not read NanoVDB Grid. Conversion to volume texture failed."));
		}

		NewVolumeTexture->UpdateResource();
	}
	else
	{
		// Initialize the texture with a single texel of data
		const uint8 Texel[] = { 0, 0, 0, 255 };
		NewVolumeTexture->Source.Init(1, 1, 1, 1, TSF_BGRA8, Texel);
		NewVolumeTexture->UpdateResource();
	}

	return NewVolumeTexture;
}

#undef LOCTEXT_NAMESPACE
