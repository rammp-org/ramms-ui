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
 * children for each non-depth stream and keeps their textures and transforms
 * up to date.
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

	/** Auto-create projectors when streams become available */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection")
	bool bAutoCreateProjectors = true;

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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY()
	TMap<FString, TObjectPtr<URammsCameraProjectorComponent>> Projectors;

private:
	/** Per-provider binding state. */
	struct FProviderBinding
	{
		TWeakObjectPtr<UObject> Object;
		FDelegateHandle FrameReadyHandle;
		FDelegateHandle StreamStatusHandle;
		FDelegateHandle ExtrinsicUpdatedHandle;
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

	void OnCameraFrameReady(const FString& StreamID, UTexture* Texture, int64 Timestamp);
	void OnCameraStreamStatus(const FString& StreamID, bool bActive);
	void OnCameraExtrinsicUpdated(const FString& StreamID, const FTransform& WorldTransform);
};
