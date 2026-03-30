// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RammsStreamProtocol.h"
#include "RammsStreamCameraBridge.generated.h"

class URammsStreamSinkComponent;
class URammsCameraProviderComponent;
class UTexture2D;
class FJsonObject;

/**
 * Bridges URammsStreamSinkComponent → URammsCameraProviderComponent.
 *
 * When placed on the same actor as a StreamSink and a CameraProvider,
 * this component automatically forwards received stream textures into
 * the camera provider system so they appear in the UI camera widgets.
 *
 * If a CameraProvider component is not found, one is auto-created.
 * Streams are auto-registered when the first frame arrives on a channel.
 * Extrinsics are updated per-frame when transform metadata is present.
 */
UCLASS(ClassGroup = (RAMMS), meta = (BlueprintSpawnableComponent))
class RAMMSUI_API URammsStreamCameraBridge : public UActorComponent
{
	GENERATED_BODY()

public:
	URammsStreamCameraBridge();

	/** Prefix for auto-generated stream IDs (e.g. "stream/0", "stream/1"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Streaming")
	FString StreamPrefix = TEXT("stream");

	/** When true, only forward frames categorised as Visual (RGB, depth, masks).
	 *  Data-only frames (motion vectors, point clouds, etc.) are silently
	 *  dropped.  Default is false — all frames are forwarded to the provider
	 *  so downstream consumers (projection manager, widgets) can filter
	 *  themselves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Streaming")
	bool bVisualOnly = false;

	/** Channels to explicitly exclude from forwarding to the camera provider.
	 *  Checked after the bVisualOnly filter. Empty = no exclusions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Streaming")
	TArray<int32> ExcludeChannels;

	/** Request that the bridge forwards CPU-side raw data alongside textures.
	 *  Call once per consumer that needs raw bytes (e.g. CPU PGM path).
	 *  Ref-counted: the bridge only queries sink raw data while count > 0. */
	UFUNCTION(BlueprintCallable, Category = "RAMMS|Streaming")
	void RequestRawDataForwarding();

	/** Release a previous raw-data request. When count reaches 0 the bridge
	 *  stops querying the sink for raw bytes, saving the per-frame CPU copy. */
	UFUNCTION(BlueprintCallable, Category = "RAMMS|Streaming")
	void ReleaseRawDataForwarding();

	/** True when at least one consumer has requested raw data forwarding. */
	bool NeedsRawData() const { return RawDataRequestCount > 0; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Ref-count of consumers that need CPU-side raw data. */
	int32 RawDataRequestCount = 0;
	UPROPERTY()
	URammsCameraProviderComponent* CameraProvider = nullptr;

	/** All sinks on this actor, queried per-channel for raw data. */
	UPROPERTY()
	TArray<TObjectPtr<URammsStreamSinkComponent>> BoundSinks;

	/** Track which channels have been registered as streams. */
	TSet<int32> RegisteredChannels;

	/** Bound to URammsStreamSinkComponent::OnFrameReceived. */
	UFUNCTION()
	void OnStreamFrameReceived(int32 ChannelID, UTexture2D* Texture, const FString& MetadataJson, ERammsStreamMessageType MessageType);

	/** Extract a transform from parsed JSON metadata. */
	bool ParseTransformFromMeta(const TSharedPtr<FJsonObject>& Meta, FTransform& OutTransform) const;
};
