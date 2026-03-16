// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IRammsCommandSink.generated.h"

/**
 * Delegate for command acknowledgement
 * @param CommandID - Unique command identifier (for tracking)
 * @param bSuccess - True if command was executed successfully
 * @param Message - Optional message (error description or confirmation)
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnCommandAck, const FString&, bool, const FString&);

UINTERFACE(MinimalAPI, Blueprintable)
class URammsCommandSink : public UInterface
{
	GENERATED_BODY()
};

/**
 * Abstract interface for sending commands to external systems
 * Implementations: RemoteControl plugin, Socket.IO adapter, ROS 2 bridge
 *
 * This interface allows UI components and game logic to send commands
 * without knowing the underlying transport mechanism.
 */
class RAMMSUI_API IRammsCommandSink
{
	GENERATED_BODY()

public:
	/**
	 * Send a movement command
	 * @param LinearVelocity - Linear velocity (X: forward/back, Y: left/right, Z: up/down) in m/s
	 * @param AngularVelocity - Angular velocity (Roll, Pitch, Yaw) in rad/s
	 * @return Command ID for tracking acknowledgement
	 */
	virtual FString SendMovementCommand(const FVector& LinearVelocity, const FRotator& AngularVelocity) = 0;

	/**
	 * Send joint position command for arm
	 * @param JointNames - Joint names (e.g., ["j0", "j1", "j2", "j3", "j4", "j5"])
	 * @param Positions - Target joint positions in radians
	 * @param bBlocking - If true, waits for command to complete
	 * @return Command ID for tracking acknowledgement
	 */
	virtual FString SendJointCommand(const TArray<FString>& JointNames, const TArray<float>& Positions, bool bBlocking = false) = 0;

	/**
	 * Send a mode change command
	 * @param ModeName - Mode identifier (e.g., "manual", "autonomous", "standby")
	 * @return Command ID for tracking acknowledgement
	 */
	virtual FString SendModeCommand(const FString& ModeName) = 0;

	/**
	 * Send a generic command with JSON parameters
	 * @param CommandType - Command type identifier
	 * @param ParamsJSON - Parameters as JSON string
	 * @return Command ID for tracking acknowledgement
	 */
	virtual FString SendGenericCommand(const FString& CommandType, const FString& ParamsJSON) = 0;

	/**
	 * Emergency stop - highest priority command
	 * @return Command ID for tracking acknowledgement
	 */
	virtual FString SendEmergencyStop() = 0;

	/**
	 * Request system state update (one-time query)
	 */
	virtual void RequestStateUpdate() = 0;

	/**
	 * Check if command sink is ready to accept commands
	 */
	virtual bool IsReady() const = 0;

	/**
	 * Get the type of this command sink (e.g., "RemoteControl", "SocketIO", "ROS2")
	 */
	virtual FString GetSinkType() const = 0;

	// Delegate accessor
	virtual FOnCommandAck& OnCommandAck() = 0;
};
