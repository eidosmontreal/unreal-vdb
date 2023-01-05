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
#include "Components/PrimitiveComponent.h"

#include "VdbPrincipledComponent.generated.h"

class UVdbSequenceComponent;
class UVdbAssetComponent;

// If you do not care about Unreal features integration, I recommend using this "Principled" component.
// It allows you to experiment with OpenVDB / NanoVDB rendering, without having to worry 
// about most Unreal compatibilities. 
// 
// These NanoVDBs are rendered at the end of the graphics pipeline, just before the Post Processes. 
// 
// This cannot be used in production, this is only used for research and experimentation purposes. It will 
// probably be incompatible with a lot of other Unreal features (but we don't care).
// Also note that this component can hack into the Unreal's path-tracer, and render high quality images.
// We made the delibarate choice to only handle NanoVDB FogVolumes with this component, because they benefit most 
// from experimentation and path-tracers, and are still an active research area (offline and realtime).
UCLASS(Blueprintable, ClassGroup = Rendering, HideCategories = (Activation, Input, Physics, Materials, Collision, Lighting, LOD, HLOD, Mobile, Navigation, VirtualTexture), meta = (BlueprintSpawnableComponent))
class UVdbPrincipledComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	virtual ~UVdbPrincipledComponent();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set VdbAssetComponent"), Category = Volume)
	void SetVdbAssets(UVdbAssetComponent* Comp);

	UPROPERTY()
	TObjectPtr<class UTextureRenderTarget2D> RenderTarget; // Must be the same for all VdbPrincipledActors

	//----------------------------------------------------------------------------
	// Volume Attributes
	//----------------------------------------------------------------------------

	// Max number of ray bounces
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Attributes", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxRayDepth = 300;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Attributes", meta = (ClampMin = "1", UIMin = "1"))
	int32 SamplesPerPixel = 1;

	// Volume local step size
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Attributes", meta = (ClampMin = "0.001", UIMin = "0.001"))
	float StepSize = 8.0f;

	// Wether to allow colored transmittance during light scattering. More physically based but less artistic-friendly when enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Attributes")
	bool ColoredTransmittance = true;

	// Enable temporal noise (including sub-frame variation for movie render queue)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Attributes")
	bool TemporalNoise = true;

	// Voxel interpolation when sampling VDB data. "Trilinear" if true (EXPENSIVE), "Closest" if false. Enabled by default when using Path Tracing rendering.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Attributes", meta = (DisplayName = "Trilinear Interpolation (SLOW)"))
	bool TrilinearInterpolation = false;

	//-------------------------------------------------------------------------------
	//    Principled Volume Shader Options, inspired by these two sources:
	// https://docs.arnoldrenderer.com/display/A5AFMUG/Standard+Volume
	// https://docs.blender.org/manual/en/latest/render/shader_nodes/shader/volume_principled.html
	//----------------------------------------------------------------------------

	// Volume scattering color. This acts as a multiplier on the scatter color, to texture the color of the volume.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Shader")
	FLinearColor Color = FLinearColor(1.0, 1.0, 1.0, 1.0);

	// Density multiplier of the volume, modulating VdbPrincipal values 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Shader", meta = (ClampMin = "0.00001", UIMin = "0.00001"))
	float DensityMultiplier = 10.0;

	// Describes the probability of scattering (versus absorption) at a scattering event. Between 0 and 1.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Shader", meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float Albedo = 0.8;

	// Ambient contribution to be added to light scattering, usually needed to cheaply boost volume radiance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Shader", meta = (UIMin = "0.0"))
	float Ambient = 0.0;

	// Backward or forward scattering direction (aka directional bias).
	// The default value of zero gives isotropic scattering so that light is scattered evenly in all directions. 
	// Positive values bias the scattering effect forwards, in the direction of the light, while negative values bias the scattering backward, toward the light. 
	// This shader uses the Henyey-Greenstein phase function.
	// Note that values very close to 1.0 (above 0.95) or -1.0 (below - 0.95) will produce scattering that is so directional that it will not be very visible from most angles, so such values are not recommended.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Shader", meta = (ClampMin = "-1.0", UIMin = "-1.0", ClampMax = "1.0", UIMax = "1.0"))
	float Anisotropy = 0.0;

	// Amount of light to emit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Shader", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EmissionStrength = 0.0;

	// Emission color tint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Shader")
	FLinearColor EmissionColor = FLinearColor(1.0, 1.0, 1.0, 1.0);

	// Blackbody emission for fire. Set to 1 for physically accurate intensity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Shader|Blackbody", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BlackbodyIntensity = 1.0;

	// Color tint for blackbody emission.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Shader|Blackbody")
	FLinearColor BlackbodyTint = FLinearColor(1.0, 1.0, 1.0, 1.0);

	// Use physically based temperature-to-color values, or user-defined color curve.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Shader|Blackbody")
	bool PhysicallyBasedBlackbody = true;
	 
	// Temperature in kelvin for blackbody emission, higher values emit more.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Shader|Blackbody", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "6500", UIMax = "6500", EditCondition = "PhysicallyBasedBlackbody"))
	float Temperature = 1500.0;

	// Material is sampling the CurveAtlas only. Cf https://docs.unrealengine.com/5.1/en-US/curve-atlases-in-unreal-engine-materials/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Shader|Blackbody", meta = (EditCondition = "!PhysicallyBasedBlackbody"))
	TObjectPtr<class UCurveLinearColorAtlas> BlackBodyCurveAtlas = nullptr;

	// Select Curve from the Curve Atlas. If invalid or if selected curve doesn't belong to the Atlas above, material will default to physically based temperature to color.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Shader|Blackbody", meta = (EditCondition = "!PhysicallyBasedBlackbody"))
	TObjectPtr<class UCurveLinearColor> BlackBodyCurve = nullptr; // TODO: find a way to filter only curves from atlas in the editor dialog.

	// Temperature values should be between 0 and 1. If using color curve (aka color ramp), this can help boost Temperature values.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Shader|Blackbody", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "!PhysicallyBasedBlackbody"))
	float TemperatureMultiplier = 1.0;

	//----------------------------------------------------------------------------
	// Debug options (by order of priority)
	//----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Debug Display")
	bool UseDirectionalLight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Debug Display")
	bool UseEnvironmentLight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Debug Display")
	bool DisplayBounds = false;

	//----------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Volume")
	void SetStepSize(float NewValue) { SetAttribute(StepSize, NewValue); }

	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Volume")
	void SetColor(FLinearColor& NewValue) { SetAttribute(Color, NewValue); }

	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Volume")
	void SetDensityMultiplier(float NewValue) { SetAttribute(DensityMultiplier, NewValue); }

	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Volume")
	void SetAlbedo(float NewValue) { SetAttribute(Albedo, NewValue); }

	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Volume")
	void SetAnisotropy(float NewValue) { SetAttribute(Anisotropy, NewValue); }

	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Volume")
	void SetEmissionStrength(float NewValue) { SetAttribute(EmissionStrength, NewValue); }

	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Volume")
	void SetEmissionColor(FLinearColor& NewValue) { SetAttribute(EmissionColor, NewValue); }

	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Volume")
	void SetBlackbodyIntensity(float NewValue) { SetAttribute(BlackbodyIntensity, NewValue); }

	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Volume")
	void SetBlackbodyTint(FLinearColor& NewValue) { SetAttribute(BlackbodyTint, NewValue); }

	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Volume")
	void SetTemperature(float NewValue) { SetAttribute(Temperature, NewValue); }

	//----------------------------------------------------------------------------
	
	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const  override;
	//~ End USceneComponent Interface.

	//~ Begin UPrimitiveComponent Interface.
	virtual bool SupportsStaticLighting() const override { return false; }
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	void UpdateSceneProxy(uint32 FrameIndex);

private:

	template<typename T>
	void SetAttribute(T& Attribute, const T& NewValue);

	UVdbAssetComponent* VdbAssets;
};
