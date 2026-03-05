// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Interfaces/IRammsCameraProvider.h"
#include "RammsCameraProjectorComponent.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;

/**
 * Projects a single camera feed onto scene surfaces using a deferred decal.
 *
 * Place this component at the camera's world pose (extrinsic). The component
 * creates a UDecalComponent internally and drives a projection material with
 * the camera's intrinsic parameters.
 *
 * The projection material should include RammsProjection.ush and expose these
 * material parameters:
 *   - CameraTexture  (Texture)    — the camera image
 *   - Intrinsics     (Vector)     — float4(fx, fy, cx, cy)
 *   - ImageSize      (Vector)     — float4(width, height, 0, 0)
 *   - CameraWorldPos (Vector)     — camera world position
 *   - CameraForward  (Vector)     — camera forward axis (world space)
 *   - CameraRight    (Vector)     — camera right axis (world space)
 *   - CameraUp       (Vector)     — camera up axis (world space)
 *   - FadeWidth      (Scalar)     — edge fade [0..0.5]
 *   - TargetStencil  (Scalar)     — stencil value for surface masking
 */
UCLASS(ClassGroup = (Ramms), meta = (BlueprintSpawnableComponent))
class RAMMSUI_API URammsCameraProjectorComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	URammsCameraProjectorComponent();

	// ── Configuration ──────────────────────────────────

	/** Base projection material (Deferred Decal, Translucent blend mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection")
	TObjectPtr<UMaterialInterface> ProjectionMaterial;

	/** Focal length X in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Intrinsics")
	float FocalLengthX = 500.0f;

	/** Focal length Y in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Intrinsics")
	float FocalLengthY = 500.0f;

	/** Principal point X in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Intrinsics")
	float PrincipalPointX = 320.0f;

	/** Principal point Y in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Intrinsics")
	float PrincipalPointY = 240.0f;

	/** Image width in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Intrinsics")
	int32 ImageWidth = 640;

	/** Image height in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Intrinsics")
	int32 ImageHeight = 480;

	/** Edge fade width in normalized UV space (0 = hard, 0.1 = 10% fade) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float FadeWidth = 0.05f;

	/** Maximum projection distance from camera position */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection", meta = (ClampMin = "1.0"))
	float MaxProjectionDistance = 5000.0f;

	/** Stencil value that target surfaces must have (set 0 to disable filtering) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection")
	int32 TargetStencilValue = 200;

	// ── Blueprint API ──────────────────────────────────

	/** Set the camera texture to project */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void SetCameraTexture(UTexture* Texture);

	/** Populate intrinsics from a camera stream info struct */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void SetIntrinsicsFromStreamInfo(const FRammsCameraStreamInfo& StreamInfo);

	/** Move the projector to a new world pose (camera extrinsic) */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void SetCameraTransform(const FTransform& WorldTransform);

	/** Show or hide the projection */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void SetProjectionEnabled(bool bEnabled);

	/** Force-refresh all material parameters (call after changing properties at runtime) */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void RefreshMaterialParameters();

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY()
	TObjectPtr<UDecalComponent> DecalComponent;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance;

private:
	void EnsureDecalCreated();
	void UpdateDecalSize();
	void UpdateMaterialParameters();
	void UpdateCameraTransformParameters();
};
