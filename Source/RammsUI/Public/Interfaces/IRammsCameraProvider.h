// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Engine/Texture2D.h"
#include "PixelFormat.h"
#include "RammsStreamProtocol.h"
#include "IRammsCameraProvider.generated.h"

/** Role of a stream within a camera group (color, depth, mask, etc.). */
UENUM(BlueprintType)
enum class ERammsStreamRole : uint8
{
	/** RGB / RGBA colour image. */
	Color,
	/** Depth map (float32 cm, uint16 mm, etc.). */
	Depth,
	/** Segmentation / instance mask. */
	Mask,
	/** Infrared / thermal image. */
	Infrared,
	/** Unspecified / other auxiliary stream. */
	Other,
};

/** Depth data encoding on the wire / in the texture. */
UENUM(BlueprintType)
enum class ERammsDepthFormat : uint8
{
	/** Unknown / not a depth stream. */
	Unknown,
	/** 32-bit float, values in centimetres (PF_R32_FLOAT). */
	Float32CM,
	/** 16-bit unsigned int, values in millimetres (PF_G16). */
	Uint16MM,
};

/**
 * Camera stream information
 */
USTRUCT(BlueprintType)
struct FRammsCameraStreamInfo
{
	GENERATED_BODY()

	/** Unique stream identifier (e.g., "camera/wrist/color") */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	FString StreamID;

	/** Human-readable name (e.g., "Wrist Camera - Color") */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	FString DisplayName;

	/** Stream width in pixels */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	int32 Width = 0;

	/** Stream height in pixels */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	int32 Height = 0;

	/** Frame rate (frames per second) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	float FrameRate = 0.0f;

	/** Pixel format (e.g., "BGRA8", "RGB8", "Depth16") */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	FString PixelFormat;

	/** Whether this is a depth camera stream (legacy — prefer StreamRole) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	bool bIsDepth = false;

	/** Semantic role of this stream within its camera group. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	ERammsStreamRole StreamRole = ERammsStreamRole::Other;

	/** Camera group identifier — streams sharing a GroupID come from the same
	 *  physical camera (e.g., "wrist_camera"). Empty = ungrouped. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	FString GroupID;

	/** Depth data encoding (only meaningful when StreamRole == Depth). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	ERammsDepthFormat DepthFormat = ERammsDepthFormat::Unknown;

	/** High-level category: Visual streams are renderable, Data streams
	 *  carry auxiliary information (motion vectors, point clouds, etc.). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	ERammsFrameCategory FrameCategory = ERammsFrameCategory::Visual;

	/** Camera intrinsics (fx, fy, cx, cy) - empty if not available */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	TArray<float> Intrinsics;

	/** Camera extrinsic — world-space pose (position + orientation). Identity if not available. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	FTransform Extrinsic;

	/** Whether a valid extrinsic has been provided */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	bool bHasExtrinsic = false;

	/** Dynamic scalar material parameters forwarded from stream metadata.
	 *  Applied to rendering MIDs after static config params, allowing the
	 *  sender to drive shader behaviour per-stream (e.g. NumSegmentIDs). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	TMap<FName, float> MaterialScalarParams;
};

/**
 * Delegate for new camera frame available
 * @param StreamID - Stream identifier
 * @param Texture - UTexture ready for rendering (UTexture2D or UTextureRenderTarget2D)
 * @param Timestamp - Frame timestamp in microseconds
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnCameraFrameReady, const FString&, UTexture*, int64);

/**
 * Delegate for camera stream status change
 * @param StreamID - Stream identifier
 * @param bActive - True if stream is active, false if stopped
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCameraStreamStatus, const FString&, bool);

/**
 * Delegate for camera extrinsic (pose) update
 * @param StreamID - Stream identifier
 * @param WorldTransform - Updated world-space camera pose
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCameraExtrinsicUpdated, const FString&, const FTransform&);

UINTERFACE(MinimalAPI, Blueprintable)
class URammsCameraProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Abstract interface for providing camera streams
 * Implementations: RemoteControl plugin, Socket.IO adapter, ROS 2 bridge
 *
 * Handles automatic texture management, format conversion, and game thread updates.
 */
class RAMMSUI_API IRammsCameraProvider
{
	GENERATED_BODY()

public:
	/**
	 * Get list of available camera streams
	 * @return Array of stream information
	 */
	virtual TArray<FRammsCameraStreamInfo> GetAvailableStreams() = 0;

	/**
	 * Get information for a specific stream
	 * @param StreamID - Stream identifier
	 * @param OutInfo - Stream information (if found)
	 * @return True if stream exists
	 */
	virtual bool GetStreamInfo(const FString& StreamID, FRammsCameraStreamInfo& OutInfo) = 0;

	/**
	 * Lightweight accessor for per-stream dynamic material scalar params.
	 * Returns a pointer to the internal map (valid until next modification) or nullptr.
	 * Avoids the full FRammsCameraStreamInfo copy of GetStreamInfo().
	 */
	virtual const TMap<FName, float>* GetStreamMaterialParams(const FString& StreamID) const { return nullptr; }

	/**
	 * Lightweight accessor for per-stream depth format.
	 * Returns ERammsDepthFormat::Unknown if the stream doesn't exist or has no depth.
	 */
	virtual ERammsDepthFormat GetStreamDepthFormat(const FString& StreamID) const { return ERammsDepthFormat::Unknown; }

	/**
	 * Lightweight accessor for per-stream pixel format string.
	 */
	virtual FString GetStreamPixelFormat(const FString& StreamID) const { return FString(); }

	/**
	 * Start receiving a camera stream
	 * @param StreamID - Stream identifier
	 * @return True if stream started successfully
	 */
	virtual bool StartStream(const FString& StreamID) = 0;

	/**
	 * Stop receiving a camera stream
	 * @param StreamID - Stream identifier
	 */
	virtual void StopStream(const FString& StreamID) = 0;

	/**
	 * Check if stream is currently active
	 * @param StreamID - Stream identifier
	 */
	virtual bool IsStreamActive(const FString& StreamID) const = 0;

	/**
	 * Get the current texture for a stream
	 * @param StreamID - Stream identifier
	 * @return Texture (nullptr if stream not started or no frames received yet)
	 */
	virtual UTexture* GetStreamTexture(const FString& StreamID) = 0;

	/**
	 * Get the timestamp of the last received frame
	 * @param StreamID - Stream identifier
	 * @return Timestamp in microseconds (0 if no frames received)
	 */
	virtual int64 GetLastFrameTimestamp(const FString& StreamID) const = 0;

	/**
	 * Get the actual frame rate being received (may differ from advertised)
	 * @param StreamID - Stream identifier
	 * @return Frames per second (0 if stream not active)
	 */
	virtual float GetActualFrameRate(const FString& StreamID) const = 0;

	// Delegate accessors
	virtual FOnCameraFrameReady&	   OnCameraFrameReady() = 0;
	virtual FOnCameraStreamStatus&	   OnCameraStreamStatus() = 0;
	virtual FOnCameraExtrinsicUpdated& OnCameraExtrinsicUpdated() = 0;

	/**
	 * Get a pointer to the latest raw pixel data for a stream.
	 * @param StreamID - Stream identifier
	 * @param OutData - Pointer to internal raw data buffer (valid until next BroadcastFrame)
	 * @param OutFormat - Pixel format of the raw data
	 * @return True if raw data is available
	 */
	virtual bool GetStreamRawData(const FString& StreamID, const TArray<uint8>*& OutData, EPixelFormat& OutFormat) const
	{
		OutData = nullptr;
		OutFormat = PF_Unknown;
		return false;
	}
};
