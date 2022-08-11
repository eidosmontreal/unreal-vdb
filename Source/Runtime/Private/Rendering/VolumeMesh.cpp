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

#include "VolumeMesh.h"
#include "LocalVertexFactory.h"
#include "DynamicMeshBuilder.h"

FVolumeMeshVertexBuffer::FVolumeMeshVertexBuffer()
{
	FVector3f BboxMin(0, 0, 0);
	FVector3f BboxMax(1, 1, 1);

	TArray<FDynamicMeshVertex> Vertices;
	// back face
	Vertices.Add(FDynamicMeshVertex(FVector3f(BboxMin.X, BboxMin.Y, BboxMin.Z)));
	Vertices.Add(FDynamicMeshVertex(FVector3f(BboxMax.X, BboxMin.Y, BboxMin.Z)));
	Vertices.Add(FDynamicMeshVertex(FVector3f(BboxMin.X, BboxMax.Y, BboxMin.Z)));
	Vertices.Add(FDynamicMeshVertex(FVector3f(BboxMax.X, BboxMax.Y, BboxMin.Z)));
	// front face
	Vertices.Add(FDynamicMeshVertex(FVector3f(BboxMin.X, BboxMin.Y, BboxMax.Z)));
	Vertices.Add(FDynamicMeshVertex(FVector3f(BboxMax.X, BboxMin.Y, BboxMax.Z)));
	Vertices.Add(FDynamicMeshVertex(FVector3f(BboxMin.X, BboxMax.Y, BboxMax.Z)));
	Vertices.Add(FDynamicMeshVertex(FVector3f(BboxMax.X, BboxMax.Y, BboxMax.Z)));

	Buffers.PositionVertexBuffer.Init(Vertices.Num());
	Buffers.StaticMeshVertexBuffer.Init(Vertices.Num(), 1);
	Buffers.ColorVertexBuffer.Init(Vertices.Num());

	for (int32 i = 0; i < Vertices.Num(); i++)
	{
		const FDynamicMeshVertex& Vertex = Vertices[i];

		Buffers.PositionVertexBuffer.VertexPosition(i) = Vertex.Position;
		Buffers.StaticMeshVertexBuffer.SetVertexTangents(i, FVector3f(Vertex.TangentX.ToFVector()), Vertex.GetTangentY(), FVector3f(Vertex.TangentZ.ToFVector()));
		Buffers.StaticMeshVertexBuffer.SetVertexUV(i, 0, Vertex.TextureCoordinate[0]);
		Buffers.ColorVertexBuffer.VertexColor(i) = Vertex.Color;
	}

	TArray<uint32> Indices{
		// bottom face
		0, 1, 2,
		1, 3, 2,
		// right face
		1, 5, 3,
		3, 5, 7,
		// front face
		3, 7, 6,
		2, 3, 6,
		// left face
		2, 4, 0,
		2, 6, 4,
		// back face
		0, 4, 5,
		1, 0, 5,
		// top face
		5, 4, 6,
		5, 6, 7 };

	IndexBuffer.SetIndices(Indices, EIndexBufferStride::Force16Bit);

	NumPrimitives = Indices.Num() / 3;
	NumVertices = Vertices.Num();
}

void FVolumeMeshVertexBuffer::InitResource()
{
	Buffers.PositionVertexBuffer.InitResource();
	Buffers.StaticMeshVertexBuffer.InitResource();
	Buffers.ColorVertexBuffer.InitResource();
	IndexBuffer.InitResource();
}

void FVolumeMeshVertexBuffer::ReleaseResource()
{
	Buffers.PositionVertexBuffer.ReleaseRHI();
	Buffers.PositionVertexBuffer.ReleaseResource();
	Buffers.StaticMeshVertexBuffer.ReleaseRHI();
	Buffers.StaticMeshVertexBuffer.ReleaseResource();
	Buffers.ColorVertexBuffer.ReleaseRHI();
	Buffers.ColorVertexBuffer.ReleaseResource();
	IndexBuffer.ReleaseRHI();
	IndexBuffer.ReleaseResource();
}

void FVolumeMeshVertexFactory::Init(FVolumeMeshVertexBuffer* InVertexBuffer)
{
	VertexBuffer = InVertexBuffer;

	// Init buffers resources
	VertexBuffer->UpdateRHI();

	// Init vertex factory resources
	{
		FDataType VertexData;
		FStaticMeshVertexBuffers& Buffers = VertexBuffer->Buffers;
		Buffers.PositionVertexBuffer.BindPositionVertexBuffer(this, VertexData);
		Buffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(this, VertexData);
		Buffers.StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(this, VertexData);
		Buffers.StaticMeshVertexBuffer.BindLightMapVertexBuffer(this, VertexData, 0);
		Buffers.ColorVertexBuffer.BindColorVertexBuffer(this, VertexData);

		SetData(VertexData);

		InitResource();
	}
}

void FVolumeMeshVertexFactoryShaderParameters::GetElementShaderBindings(
	const class FSceneInterface* Scene,
	const FSceneView* View,
	const FMeshMaterialShader* Shader,
	const EVertexInputStreamType InputStreamType,
	ERHIFeatureLevel::Type FeatureLevel,
	const FVertexFactory* VertexFactory,
	const FMeshBatchElement& BatchElement,
	FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams
) const
{
	check(VertexFactory->GetType() == &FVolumeMeshVertexFactory::StaticType);
	
	FRHIUniformBuffer* VertexFactoryUniformBuffer = static_cast<FRHIUniformBuffer*>(BatchElement.VertexFactoryUserData);
	// FLocalVertexFactoryShaderParametersBase::GetElementShaderBindingsBase is not exposed, so we have to re-write it:
	const auto* LocalVertexFactory = static_cast<const FLocalVertexFactory*>(VertexFactory);
	if (LocalVertexFactory->SupportsManualVertexFetch(FeatureLevel) || UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel))
	{
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FLocalVertexFactoryUniformShaderParameters>(), VertexFactoryUniformBuffer);
	}

	const FVdbVertexFactoryUserDataWrapper* BatchUserData = static_cast<const FVdbVertexFactoryUserDataWrapper*>(BatchElement.UserData);
	check(BatchUserData);

	ShaderBindings.Add(IndexScale, BatchUserData->Data.IndexSize);
	ShaderBindings.Add(IndexTranslation, BatchUserData->Data.IndexMin);
	ShaderBindings.Add(IndexToLocal, BatchUserData->Data.IndexToLocal);
};

IMPLEMENT_TYPE_LAYOUT(FVolumeMeshVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FVolumeMeshVertexFactory, SF_Vertex, FVolumeMeshVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_TYPE(FVolumeMeshVertexFactory, "/Plugin/VdbVolume/Private/VolumeVertexFactory.ush",
	EVertexFactoryFlags::UsedWithMaterials |
	EVertexFactoryFlags::SupportsDynamicLighting |
	EVertexFactoryFlags::SupportsPositionOnly
);