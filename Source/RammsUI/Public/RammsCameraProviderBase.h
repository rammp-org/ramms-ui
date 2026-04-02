// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PixelFormat.h"
#include "Interfaces/IRammsCameraProvider.h"
#include "RammsCameraProviderBase.generated.h"

/**
 * Internal tracking for an active camera stream.
 */
USTRUCT()
struct FRammsCameraStreamState
{
	GENERATED_BODY()

	UPROPERTY()
	FRammsCameraStreamInfo Info;

	UPROPERTY()
	TObjectPtr<UTexture> Texture = nullptr;

	UPROPERTY()
	bool bActive = false;

	UPROPERTY()
	int64 LastTimestamp = 0;

	UPROPERTY()
	float ActualFrameRate = 0.0f;

	// Frame rate tracking
	int32  FrameCount = 0;
	double FrameRateTimer = 0.0;

	/** CPU-side copy of the latest frame's raw pixel data.
	 *  Kept alongside the GPU texture so PGM and other CPU consumers
	 *  can read pixel values without GPU readback. */
	TArray<uint8> RawFrameData;

	/** Pixel format of RawFrameData (PF_B8G8R8A8, PF_R32_FLOAT, etc.). */
	EPixelFormat FramePixelFormat = PF_Unknown;
};

/**
 * Blueprint-friendly base class for camera stream providers.
 *
 * Implements IRammsCameraProvider with internal stream management.
 * Subclass in Blueprint or C++ to create custom camera sources.
 *
 * Usage in Blueprint:
 *   1. Create a Blueprint child of this class
 *   2. Call RegisterStream() to define available streams
 *   3. Call BroadcastFrame() when new frame data is available
 *   4. Override OnStreamStartRequested/OnStreamStopRequested for custom behavior
 */
UCLASS(Blueprintable, BlueprintType)
class RAMMSUI_API ARammsCameraProviderBase : public AActor, public IRammsCameraProvider
{
	GENERATED_BODY()

public:
	ARammsCameraProviderBase();

	// --- Blueprint API ---

	/** Register a new camera stream. Call during BeginPlay or when streams become available.
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

	/** Update the extrinsic (world-space pose) for a stream. Fires OnCameraStreamStatus so consumers can refresh. */
	UFUNCTION(BlueprintCallable, Category = "Camera Provider")
	void UpdateStreamExtrinsic(const FString& StreamID, const FTransform& WorldTransform);

	// --- Blueprint Events (override in Blueprint) ---

	/** Called when a consumer requests a stream to start. Override to begin producing frames. Return true if stream started successfully. */
	UFUNCTION(BlueprintNativeEvent, Category = "Camera Provider")
	bool OnStreamStartRequested(const FString& StreamID);

	/** Called when a consumer requests a stream to stop. Override to stop producing frames. */
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
	virtual void Tick(float DeltaTime) override;

	UPROPERTY()
	TMap<FString, FRammsCameraStreamState> Streams;

private:
	FOnCameraFrameReady		  CameraFrameReadyDelegate;
	FOnCameraStreamStatus	  CameraStreamStatusDelegate;
	FOnCameraExtrinsicUpdated CameraExtrinsicUpdatedDelegate;
};
