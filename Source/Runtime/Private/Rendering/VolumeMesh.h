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
#include "RenderResource.h"
#include "StaticMeshResources.h"
#include "MeshMaterialShader.h"

// Unit cube (0, 0, 0) -> (1, 1, 1), to be scaled when rendering using VolumeMeshVertexFactory
class FVolumeMeshVertexBuffer : public FRenderResource
{
public:
	FStaticMeshVertexBuffers Buffers;
	FRawStaticIndexBuffer IndexBuffer;
	uint32 NumPrimitives;
	uint32 NumVertices;

	FVolumeMeshVertexBuffer();

	virtual void InitResource() override;
	virtual void ReleaseResource() override;
};

class FVolumeMeshVertexFactory : public FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FVolumeMeshVertexFactory);
	
	typedef FLocalVertexFactory Super;

public:
	FVolumeMeshVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: Super(InFeatureLevel, "FVolumeMeshVertexFactory")
	{}

	~FVolumeMeshVertexFactory()
	{
		ReleaseResource();
	}

	void Init(FVolumeMeshVertexBuffer* InVertexBuffer);

	bool HasIncompatibleFeatureLevel(ERHIFeatureLevel::Type InFeatureLevel)
	{
		return InFeatureLevel != GetFeatureLevel();
	}

	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
	{
		bool Cond = 
			Super::ShouldCompilePermutation(Parameters) &&
			IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
			Parameters.MaterialParameters.MaterialDomain == MD_Volume &&
			IsPCPlatform(Parameters.Platform);

		return Cond || Parameters.MaterialParameters.bIsSpecialEngineMaterial;
	}

private:
	FVolumeMeshVertexBuffer* VertexBuffer;
};

struct FVolumeBatchElementUserData
{
	FVector3f IndexMin;
	FVector3f IndexSize;
	FMatrix44f IndexToLocal;
};

struct FVdbVertexFactoryUserDataWrapper : public FOneFrameResource
{
	FVolumeBatchElementUserData Data;
};

// Should derive from FLocalVertexFactoryShaderParametersBase instead, but it is not exposed
class FVolumeMeshVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FVolumeMeshVertexFactoryShaderParameters, NonVirtual);
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		IndexScale.Bind(ParameterMap, TEXT("IndexScale"));
		IndexTranslation.Bind(ParameterMap, TEXT("IndexTranslation"));
		IndexToLocal.Bind(ParameterMap, TEXT("IndexToLocal"));
	}

	void GetElementShaderBindings(
		const class FSceneInterface* Scene,
		const FSceneView* View,
		const FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams
	) const;

private:

	LAYOUT_FIELD(FShaderParameter, IndexScale);
	LAYOUT_FIELD(FShaderParameter, IndexTranslation);
	LAYOUT_FIELD(FShaderParameter, IndexToLocal);
};
