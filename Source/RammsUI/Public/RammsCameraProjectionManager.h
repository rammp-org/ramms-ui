// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IRammsCameraProvider.h"
#include "RammsCameraProjectionManager.generated.h"

class URammsCameraProjectorComponent;

/**
 * Manages camera-feed projectors driven by one or more IRammsCameraProviders.
 *
 * Attach this component to any actor. When camera providers are discovered
 * (or assigned), the manager auto-creates URammsCameraProjectorComponent
 * children for each color stream and keeps their textures and transforms
 * up to date. Depth streams are automatically linked to the matching
 * color projector (via GroupID) for PGM use.
 *
 * Supports multiple providers simultaneously — e.g. an actor-based provider
 * for in-scene cameras AND a component-based provider for streamed cameras.
 *
 * Camera extrinsics (world transforms) can be supplied via:
 *   - FRammsCameraStreamInfo::Extrinsic (set bHasExtrinsic=true)
 *   - UpdateStreamExtrinsic() on the provider (auto-updates projectors)
 *   - SetProjectorTransform() on this manager (manual override)
 */
UCLASS(ClassGroup = (Ramms), meta = (BlueprintSpawnableComponent))
class RAMMSUI_API URammsCameraProjectionManager : public UActorComponent
{
	GENERATED_BODY()

public:
	URammsCameraProjectionManager();

	// ── Configuration ──────────────────────────────────

	/** Projection material shared by all projectors (Deferred Decal) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection")
	TObjectPtr<UMaterialInterface> ProjectionMaterial;

	/** Default stencil value for target surface filtering */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection")
	int32 DefaultTargetStencil = 200;

	/** Default edge fade width */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float DefaultFadeWidth = 0.05f;

	/** Default maximum projection distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection", meta = (ClampMin = "1.0"))
	float DefaultMaxDistance = 5000.0f;

	/**
	 * Multiplier applied to each projector's decal size for frustum culling bounds.
	 * Increase if decals disappear at oblique camera angles (e.g. on ARM64/Vulkan).
	 * The projection material masks to the correct frustum, so oversized bounds
	 * only affect culling conservatism.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection", meta = (ClampMin = "1.0", ClampMax = "10.0"))
	float DefaultDecalBoundsInflation = 1.5f;

	/** Auto-create projectors when streams become available */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection")
	bool bAutoCreateProjectors = true;

	/** Stream IDs to never create projectors for (e.g. "stream/100"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Filtering")
	TArray<FString> ExcludeStreamIDs;

	// ── PGM Configuration ──────────────────────────────

	/** Enable/disable Projective Grid Mesh (3D Point Cloud/Mesh) globally */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|PGM")
	bool bEnablePGM = false;

	/** GPU-accelerated PGM (WPO-based, no CPU readback). Requires a WPO material using RammsPGM.ush. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|PGM", meta = (EditCondition = "bEnablePGM"))
	bool bGPUAccelerated = true;

	/** Material used for PGM rendering.
	 *  GPU mode: must include RammsPGM.ush and use WPO for deprojection.
	 *  CPU mode: should use Vertex Color node for per-vertex colors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|PGM", meta = (EditCondition = "bEnablePGM"))
	TObjectPtr<UMaterialInterface> PGMMaterial;

	/** Maximum allowed edge length between vertices to form a face (in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|PGM", meta = (EditCondition = "bEnablePGM"))
	float MaxEdgeStretchCM = 50.0f;

	/** Multiplier to convert raw depth texture values to Centimeters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|PGM", meta = (EditCondition = "bEnablePGM"))
	float DepthScaleToCM = 1.0f;

	/** Minimum depth to consider valid (in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|PGM", meta = (EditCondition = "bEnablePGM"))
	float MinDepthCM = 10.0f;

	/** Maximum depth to consider valid (in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|PGM", meta = (EditCondition = "bEnablePGM"))
	float MaxDepthCM = 1000.0f;

	/** Decimation factor (1 = full res, 2 = half res, 4 = quarter res) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|PGM", meta = (EditCondition = "bEnablePGM", ClampMin = "1"))
	int32 Decimation = 4;

	/** Offset applied to RGB sampling due to sensor displacement (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|PGM", meta = (EditCondition = "bEnablePGM"))
	float SensorBaselineY = 0.0f;

	/** Sync threshold between RGB and Depth frames (milliseconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|PGM", meta = (EditCondition = "bEnablePGM"))
	float SyncThresholdMS = 100.0f;

	/** Enable custom depth/stencil rendering on PGM meshes (for post-process effects) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|PGM", meta = (EditCondition = "bEnablePGM"))
	bool bPGMRenderCustomDepth = false;

	/** Custom stencil value written by PGM meshes (0-255, used by post-process materials) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|PGM", meta = (EditCondition = "bEnablePGM && bPGMRenderCustomDepth", ClampMin = "0", ClampMax = "255", UIMin = "0", UIMax = "255"))
	int32 PGMCustomStencilValue = 1;

	// ── Blueprint API ──────────────────────────────────

	/** Connect to a camera provider — starts listening for streams */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void SetCameraProvider(TScriptInterface<IRammsCameraProvider> Provider);

	/** Add an additional provider (does not remove existing ones) */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void AddCameraProvider(TScriptInterface<IRammsCameraProvider> Provider);

	/** Disconnect from all camera providers */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void ClearCameraProvider();

	/** Manually add a projector for a specific stream */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	URammsCameraProjectorComponent* AddProjector(const FString& StreamID);

	/** Set the world transform (camera extrinsic) for a projector */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void SetProjectorTransform(const FString& StreamID, const FTransform& WorldTransform);

	/** Remove a projector */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void RemoveProjector(const FString& StreamID);

	/** Remove all projectors */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void RemoveAllProjectors();

	/** Get the projector component for a given stream (nullptr if none) */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	URammsCameraProjectorComponent* GetProjector(const FString& StreamID) const;

	/** Get all active stream IDs that have projectors */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	TArray<FString> GetProjectorStreamIDs() const;

	/** Enable or disable PGM on all projectors at runtime */
	UFUNCTION(BlueprintCallable, Category = "Projection|PGM")
	void SetPGMEnabled(bool bEnabled);

	/** Switch between GPU and CPU PGM at runtime. Propagates to all projectors
	 *  and updates raw-data forwarding on the bridge accordingly. */
	UFUNCTION(BlueprintCallable, Category = "Projection|PGM")
	void SetGPUAccelerated(bool bGPU);

	/** Set minimum valid depth (cm). Propagates to all projectors. */
	UFUNCTION(BlueprintCallable, Category = "Projection|PGM")
	void SetMinDepth(float InMinDepthCM);

	/** Set maximum valid depth (cm). Propagates to all projectors. */
	UFUNCTION(BlueprintCallable, Category = "Projection|PGM")
	void SetMaxDepth(float InMaxDepthCM);

	/** Set maximum edge stretch (cm). Propagates to all projectors. */
	UFUNCTION(BlueprintCallable, Category = "Projection|PGM")
	void SetMaxEdgeStretch(float InMaxEdgeStretchCM);

	/** Set depth scale to cm multiplier. Propagates to all projectors. */
	UFUNCTION(BlueprintCallable, Category = "Projection|PGM")
	void SetDepthScale(float InDepthScaleToCM);

	/** Set decimation factor. Propagates to all projectors and rebuilds PGM grids. */
	UFUNCTION(BlueprintCallable, Category = "Projection|PGM")
	void SetDecimation(int32 InDecimation);

	/** Set sensor baseline Y offset (cm). Propagates to all projectors. */
	UFUNCTION(BlueprintCallable, Category = "Projection|PGM")
	void SetSensorBaseline(float InBaselineY);

	/** Bulk-update depth configuration on all projectors, with one material refresh per projector.
	 *  Pass negative values to leave a parameter unchanged. */
	UFUNCTION(BlueprintCallable, Category = "Projection|PGM")
	void SetDepthConfig(float InMinDepthCM, float InMaxDepthCM, float InMaxEdgeStretchCM, float InDepthScaleToCM = -1.0f);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY()
	TMap<FString, TObjectPtr<URammsCameraProjectorComponent>> Projectors;

private:
	/** Per-provider binding state. */
	struct FProviderBinding
	{
		TWeakObjectPtr<UObject> Object;
		FDelegateHandle			FrameReadyHandle;
		FDelegateHandle			StreamStatusHandle;
		FDelegateHandle			ExtrinsicUpdatedHandle;
	};

	TArray<FProviderBinding> ProviderBindings;

	/** Get the interface pointer for a binding (nullptr if stale). */
	static IRammsCameraProvider* GetProviderFromBinding(const FProviderBinding& Binding);

	/** Find a valid provider that knows about a given StreamID. */
	IRammsCameraProvider* FindProviderForStream(const FString& StreamID) const;

	void BindProvider(UObject* Obj, IRammsCameraProvider* Iface);
	void UnbindAllProviders();
	bool IsProviderBound(UObject* Obj) const;
	void CreateProjectorsForProvider(IRammsCameraProvider* Iface);
	void DiscoverProviders();

	FTimerHandle DeferredDiscoveryHandle;

	/** True if we called RequestRawDataForwarding on the bridge (so we release on EndPlay). */
	bool bRequestedRawData = false;

	/** Requests or releases raw-data forwarding on the bridge to match current mode.
	 *  CPU PGM (bEnablePGM && !bGPUAccelerated) needs raw data; all other modes don't. */
	void UpdateRawDataRequest();

	void OnCameraFrameReady(const FString& StreamID, UTexture* Texture, int64 Timestamp);
	void OnCameraStreamStatus(const FString& StreamID, bool bActive);
	void OnCameraExtrinsicUpdated(const FString& StreamID, const FTransform& WorldTransform);

	/** Find the depth stream associated with a color stream.
	 *  Uses GroupID matching first, falls back to deprecated channel+100 convention. */
	FString FindDepthStreamForColor(const FString& ColorStreamID, IRammsCameraProvider* Provider) const;

	/** Find the color projector associated with a depth stream.
	 *  Uses GroupID matching first, falls back to deprecated channel+100 convention. */
	FString FindColorStreamForDepth(const FString& DepthStreamID, IRammsCameraProvider* Provider) const;
};
