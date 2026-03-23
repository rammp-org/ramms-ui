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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	URammsCameraProviderComponent* CameraProvider = nullptr;

	/** Track which channels have been registered as streams. */
	TSet<int32> RegisteredChannels;

	/** Bound to URammsStreamSinkComponent::OnFrameReceived. */
	UFUNCTION()
	void OnStreamFrameReceived(int32 ChannelID, UTexture2D* Texture, const FString& MetadataJson, ERammsStreamMessageType MessageType);

	/** Extract a transform from parsed JSON metadata. */
	bool ParseTransformFromMeta(const TSharedPtr<FJsonObject>& Meta, FTransform& OutTransform) const;
};
