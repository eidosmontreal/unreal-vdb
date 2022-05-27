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

#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceVdb.generated.h"

USTRUCT()
struct FVdbRay
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, Category = "Ray")
	FVector3f Origin;

	UPROPERTY(EditAnywhere, Category = "Ray")
	float Tmin = 0.0001;

	UPROPERTY(EditAnywhere, Category = "Ray")
	FVector3f Direction;

	UPROPERTY(EditAnywhere, Category = "Ray")
	float Tmax = FLT_MAX;
};

USTRUCT()
struct FVdbLevelSetHit
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, Category = "LevelSetHit")
	float t;
	UPROPERTY(EditAnywhere, Category = "LevelSetHit")
	float v0;
	UPROPERTY(EditAnywhere, Category = "LevelSetHit")
	int i;
	UPROPERTY(EditAnywhere, Category = "LevelSetHit")
	int j;
	UPROPERTY(EditAnywhere, Category = "LevelSetHit")
	int k;
};

UCLASS(EditInlineNew, Category = "SparseVolumetrics", meta = (DisplayName = "Volume VDB"))
class VOLUMERUNTIME_API UNiagaraDataInterfaceVdb : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	DECLARE_NIAGARA_DI_PARAMETER();

	UPROPERTY(EditAnywhere, Category = "Source")
	TObjectPtr<class UVdbVolumeStatic> VdbVolumeStatic;

	//UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//UObject Interface End

	//UNiagaraDataInterface Interface
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	// Returns the DI's functions signatures 
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return Target == ENiagaraSimTarget::GPUComputeSim; }
	virtual bool RequiresDepthBuffer() const override { return false; }

#if WITH_EDITORONLY_DATA
	virtual void GetCommonHLSL(FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
#endif


protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	virtual void PushToRenderThreadImpl() override;
};
