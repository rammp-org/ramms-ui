// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Interfaces/IRammsCameraProvider.h"
#include "RammsCameraProjectorComponent.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class UTextureRenderTarget2D;

/**
 * Projects a single camera feed onto scene surfaces using a deferred decal
 * and/or a 3D Projective Grid Mesh (PGM).
 *
 * Place this component at the camera's world pose (extrinsic).
 *
 * The component creates a UDecalComponent internally and drives a projection
 * material with the camera's intrinsic parameters.
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Intrinsics", meta = (ClampMin = "1", UIMin = "1"))
	int32 ImageWidth = 640;

	/** Image height in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Intrinsics", meta = (ClampMin = "1", UIMin = "1"))
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

	// ── PGM Configuration ──────────────────────────────

	/** Enable/disable Projective Grid Mesh (3D Point Cloud/Mesh) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM")
	bool bEnablePGM = false;

	/** Material used for PGM rendering (requires Vertex Color node) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM", meta = (EditCondition = "bEnablePGM"))
	TObjectPtr<UMaterialInterface> PGMMaterial;

	/** Maximum allowed edge length between vertices to form a face (in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM", meta = (EditCondition = "bEnablePGM"))
	float MaxEdgeStretchCM = 50.0f;

	/** Multiplier to convert raw depth texture values to Centimeters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM", meta = (EditCondition = "bEnablePGM"))
	float DepthScaleToCM = 1.0f;

	/** Minimum depth to consider valid (in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM", meta = (EditCondition = "bEnablePGM"))
	float MinDepthCM = 10.0f;

	/** Maximum depth to consider valid (in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM", meta = (EditCondition = "bEnablePGM"))
	float MaxDepthCM = 1000.0f;

	/** Decimation factor (1 = full res, 2 = half res, 4 = quarter res) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM", meta = (EditCondition = "bEnablePGM", ClampMin = "1"))
	int32 Decimation = 4;

	/** Offset applied to RGB sampling due to sensor displacement (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM", meta = (EditCondition = "bEnablePGM"))
	float SensorBaselineY = 0.0f;

	/** Sync threshold between RGB and Depth frames (milliseconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM", meta = (EditCondition = "bEnablePGM"))
	float SyncThresholdMS = 100.0f;

	/** Enable custom depth/stencil rendering on the PGM mesh (for post-process effects) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM", meta = (EditCondition = "bEnablePGM"))
	bool bPGMRenderCustomDepth = false;

	/** Custom stencil value written by the PGM mesh (0-255, used by post-process materials) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM", meta = (EditCondition = "bEnablePGM && bPGMRenderCustomDepth", ClampMin = "0", ClampMax = "255", UIMin = "0", UIMax = "255"))
	int32 PGMCustomStencilValue = 1;

	/** The ID of the corresponding depth stream */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM")
	FString DepthStreamID;

	// ── Blueprint API ──────────────────────────────────

	/** Set the camera texture to project */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void SetCameraTexture(UTexture* Texture);

	/** Set the depth texture for PGM generation */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void SetDepthTexture(UTexture* Texture, int64 Timestamp);

	/** Updated camera texture set with timestamp for sync */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void SetColorTexture(UTexture* Texture, int64 Timestamp);

	/** Set color texture with CPU-side raw data for PGM.
	 *  Accepts a non-owning view; the projector copies internally. */
	void SetColorTextureWithData(UTexture* Texture, int64 Timestamp,
		TConstArrayView<uint8> RawData, EPixelFormat Format, int32 Width, int32 Height);

	/** Set depth texture with CPU-side raw data for PGM.
	 *  Accepts a non-owning view; the projector copies internally. */
	void SetDepthTextureWithData(UTexture* Texture, int64 Timestamp,
		TConstArrayView<uint8> RawData, EPixelFormat Format, int32 Width, int32 Height);

	/** Populate intrinsics from a camera stream info struct */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void SetIntrinsicsFromStreamInfo(const FRammsCameraStreamInfo& StreamInfo);

	/** Move the projector to a new world pose (camera extrinsic) */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void SetCameraTransform(const FTransform& WorldTransform);

	/** Show or hide the projection */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void SetProjectionEnabled(bool bEnabled);

	/** Enable or disable the PGM mesh at runtime */
	UFUNCTION(BlueprintCallable, Category = "PGM")
	void SetPGMEnabled(bool bEnabled);

	/** Set whether the PGM mesh renders to custom depth/stencil, and the stencil value (at runtime) */
	UFUNCTION(BlueprintCallable, Category = "PGM")
	void SetPGMCustomDepthStencil(bool bEnable, int32 StencilValue = 1);

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

	/** Dynamic material instance for the PGM mesh (created from PGMMaterial) */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PGMMaterialInstance;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> ProcMeshComponent;

private:
	void EnsureDecalCreated();
	void EnsurePGMCreated();
	void UpdateDecalSize();
	void UpdateMaterialParameters();
	void UpdateCameraTransformParameters();

	void UpdatePGM();
	void MaybeUpdatePGM();

	/** GPU readback fallback: extract pixel data from a texture when no raw data was provided. */
	bool TryReadTextureToRawData(UTexture* Texture,
		TArray<uint8>& OutRawData, EPixelFormat& OutFormat,
		int32& OutWidth, int32& OutHeight);

	// State trackers
	TObjectPtr<UTexture> CurrentColorTexture;
	TObjectPtr<UTexture> CurrentDepthTexture;
	int64				 LastColorTimestamp = 0;
	int64				 LastDepthTimestamp = 0;
	int64				 LastPGMTimestamp = 0;

	// CPU-side raw frame data for PGM (avoids GPU readback)
	TArray<uint8> ColorRawData;
	EPixelFormat  ColorPixelFormat = PF_Unknown;
	int32		  ColorFrameWidth = 0;
	int32		  ColorFrameHeight = 0;

	TArray<uint8> DepthRawData;
	EPixelFormat  DepthPixelFormat = PF_Unknown;
	int32		  DepthFrameWidth = 0;
	int32		  DepthFrameHeight = 0;
};
