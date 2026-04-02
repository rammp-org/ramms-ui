// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IRammsCameraProvider.h"
#include "RammsCameraProviderBase.h"
#include "RammsCameraProviderComponent.generated.h"

/**
 * Component-based camera stream provider.
 *
 * Same functionality as ARammsCameraProviderBase but as an ActorComponent
 * so it can be added to any existing actor.
 *
 * Usage:
 *   1. Add this component to any actor
 *   2. Call RegisterStream() to define available streams
 *   3. Call BroadcastFrame() when new frame data is available
 *   4. Override OnStreamStartRequested/OnStreamStopRequested for custom behavior
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Ramms), meta = (BlueprintSpawnableComponent))
class RAMMSUI_API URammsCameraProviderComponent : public UActorComponent, public IRammsCameraProvider
{
	GENERATED_BODY()

public:
	URammsCameraProviderComponent();

	// --- Blueprint API ---

	/** Register a new camera stream.
	 *  @param bActivate  When true (default), the stream is activated immediately
	 *                    and OnCameraStreamStatus is broadcast so downstream
	 *                    consumers (e.g. ProjectionManager) learn about it. */
	UFUNCTION(BlueprintCallable, Category = "Camera Provider")
	void RegisterStream(const FRammsCameraStreamInfo& StreamInfo, bool bActivate = true);

	/** Unregister a camera stream. */
	UFUNCTION(BlueprintCallable, Category = "Camera Provider")
	void UnregisterStream(const FString& StreamID);

	/** Broadcast a new frame for a stream. Accepts UTexture2D or UTextureRenderTarget2D. */
	UFUNCTION(BlueprintCallable, Category = "Camera Provider")
	void BroadcastFrame(const FString& StreamID, UTexture* Texture);

	/** Broadcast raw pixel data for a stream. Creates the texture if needed. */
	UFUNCTION(BlueprintCallable, Category = "Camera Provider")
	void BroadcastFrameData(const FString& StreamID, const TArray<uint8>& PixelData, int32 Width, int32 Height);

	/** Broadcast a frame with both a GPU texture and a CPU-side raw data copy.
	 *  The raw data is stored so PGM / CPU consumers can read pixel values
	 *  without GPU readback.  Prefer this over BroadcastFrame when raw bytes
	 *  are available. */
	void BroadcastFrameWithRawData(const FString& StreamID, UTexture* Texture,
		TArray<uint8>&& RawData, EPixelFormat PixelFormat);

	/** Mark a stream as active/inactive and fire status delegate. */
	UFUNCTION(BlueprintCallable, Category = "Camera Provider")
	void SetStreamActive(const FString& StreamID, bool bActive);

	/** Update the extrinsic (world-space pose) for a stream. Fires OnCameraExtrinsicUpdated so consumers can refresh. */
	UFUNCTION(BlueprintCallable, Category = "Camera Provider")
	void UpdateStreamExtrinsic(const FString& StreamID, const FTransform& WorldTransform);

	// --- Blueprint Events ---

	/** Called when a consumer requests a stream to start. Override for custom behavior. */
	UFUNCTION(BlueprintNativeEvent, Category = "Camera Provider")
	bool OnStreamStartRequested(const FString& StreamID);

	/** Called when a consumer requests a stream to stop. Override for custom behavior. */
	UFUNCTION(BlueprintNativeEvent, Category = "Camera Provider")
	void OnStreamStopRequested(const FString& StreamID);

	// --- IRammsCameraProvider interface ---
	virtual TArray<FRammsCameraStreamInfo> GetAvailableStreams() override;
	virtual bool						   GetStreamInfo(const FString& StreamID, FRammsCameraStreamInfo& OutInfo) override;
	virtual bool						   StartStream(const FString& StreamID) override;
	virtual void						   StopStream(const FString& StreamID) override;
	virtual bool						   IsStreamActive(const FString& StreamID) const override;
	virtual UTexture*					   GetStreamTexture(const FString& StreamID) override;
	virtual int64						   GetLastFrameTimestamp(const FString& StreamID) const override;
	virtual float						   GetActualFrameRate(const FString& StreamID) const override;
	virtual FOnCameraFrameReady&		   OnCameraFrameReady() override { return CameraFrameReadyDelegate; }
	virtual FOnCameraStreamStatus&		   OnCameraStreamStatus() override { return CameraStreamStatusDelegate; }
	virtual FOnCameraExtrinsicUpdated&	   OnCameraExtrinsicUpdated() override { return CameraExtrinsicUpdatedDelegate; }
	virtual bool						   GetStreamRawData(const FString& StreamID, const TArray<uint8>*& OutData, EPixelFormat& OutFormat) const override;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY()
	TMap<FString, FRammsCameraStreamState> Streams;

private:
	FOnCameraFrameReady		  CameraFrameReadyDelegate;
	FOnCameraStreamStatus	  CameraStreamStatusDelegate;
	FOnCameraExtrinsicUpdated CameraExtrinsicUpdatedDelegate;
};
