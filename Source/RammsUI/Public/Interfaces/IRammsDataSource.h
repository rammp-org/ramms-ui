// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IRammsDataSource.generated.h"

/**
 * Delegate for receiving camera frame data
 * @param StreamID - Identifier for the camera stream (e.g., "camera/wrist/color")
 * @param ImageData - Raw image data (JPEG, PNG, or raw pixels)
 * @param Width - Image width in pixels
 * @param Height - Image height in pixels
 * @param Format - Format string ("JPEG", "PNG", "BGRA8", etc.)
 * @param Timestamp - Frame timestamp in microseconds
 */
DECLARE_MULTICAST_DELEGATE_SixParams(FOnCameraFrame, const FString&, const TArray<uint8>&, int32, int32, const FString&, int64);

/**
 * Delegate for receiving robot state updates
 * @param StateData - JSON string containing state information
 * @param Timestamp - State timestamp in microseconds
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRobotState, const FString&, int64);

/**
 * Delegate for receiving joint state updates
 * @param JointNames - Array of joint names (e.g., ["j0", "j1", "j2", "j3", "j4", "j5"])
 * @param Positions - Joint positions in radians
 * @param Velocities - Joint velocities (optional, can be empty)
 * @param Efforts - Joint efforts/torques (optional, can be empty)
 * @param Timestamp - State timestamp in microseconds
 */
DECLARE_MULTICAST_DELEGATE_FiveParams(FOnJointState, const TArray<FString>&, const TArray<float>&, const TArray<float>&, const TArray<float>&, int64);

/**
 * Delegate for receiving transform updates
 * @param FrameID - Transform frame identifier
 * @param ParentFrameID - Parent frame identifier
 * @param Transform - Transform from parent to frame
 * @param Timestamp - Transform timestamp in microseconds
 */
DECLARE_MULTICAST_DELEGATE_FourParams(FOnTransform, const FString&, const FString&, const FTransform&, int64);

/**
 * Delegate for receiving generic sensor data
 * @param SensorID - Sensor identifier
 * @param DataJSON - Sensor data as JSON string
 * @param Timestamp - Data timestamp in microseconds
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSensorData, const FString&, const FString&, int64);

/**
 * Delegate for connection status changes
 * @param bConnected - True if connected, false if disconnected
 * @param Source - Data source identifier (e.g., "RemoteControl", "SocketIO", "ROS2")
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnConnectionStatus, bool, const FString&);

UINTERFACE(MinimalAPI, Blueprintable)
class URammsDataSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * Abstract interface for receiving data from external sources
 * Implementations: RemoteControl plugin, Socket.IO adapter, ROS 2 bridge
 * 
 * This interface allows UI components and game logic to receive data
 * without knowing the underlying transport mechanism.
 */
class RAMMSUI_API IRammsDataSource
{
	GENERATED_BODY()

public:
	/**
	 * Connect to the data source
	 * @return True if connection successful or already connected
	 */
	virtual bool Connect() = 0;

	/**
	 * Disconnect from the data source
	 */
	virtual void Disconnect() = 0;

	/**
	 * Check if currently connected
	 */
	virtual bool IsConnected() const = 0;

	/**
	 * Get the type of this data source (e.g., "RemoteControl", "SocketIO", "ROS2")
	 */
	virtual FString GetSourceType() const = 0;

	/**
	 * Subscribe to a specific data stream
	 * @param StreamID - Stream identifier (e.g., "camera/wrist/color", "state", "joints")
	 * @return True if subscription successful
	 */
	virtual bool Subscribe(const FString& StreamID) = 0;

	/**
	 * Unsubscribe from a data stream
	 * @param StreamID - Stream identifier
	 */
	virtual void Unsubscribe(const FString& StreamID) = 0;

	/**
	 * Get list of available streams (may require discovery/query)
	 * @return Array of stream identifiers
	 */
	virtual TArray<FString> GetAvailableStreams() = 0;

	// Delegate accessors
	virtual FOnCameraFrame& OnCameraFrame() = 0;
	virtual FOnRobotState& OnRobotState() = 0;
	virtual FOnJointState& OnJointState() = 0;
	virtual FOnTransform& OnTransform() = 0;
	virtual FOnSensorData& OnSensorData() = 0;
	virtual FOnConnectionStatus& OnConnectionStatus() = 0;
};
