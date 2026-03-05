// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IRammsStateProvider.generated.h"

/**
 * Robot operating mode
 */
UENUM(BlueprintType)
enum class ERammsRobotMode : uint8
{
	Unknown,
	Standby,
	Manual,
	Autonomous,
	Emergency
};

/**
 * Robot state snapshot
 */
USTRUCT(BlueprintType)
struct FRammsRobotState
{
	GENERATED_BODY()

	/** Current operating mode */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	ERammsRobotMode Mode = ERammsRobotMode::Unknown;

	/** Battery level (0.0 to 1.0) */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	float BatteryLevel = 0.0f;

	/** Linear velocity (m/s) */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	FVector LinearVelocity = FVector::ZeroVector;

	/** Angular velocity (rad/s) */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	FRotator AngularVelocity = FRotator::ZeroRotator;

	/** Base pose (position and orientation in world frame) */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	FTransform BasePose = FTransform::Identity;

	/** Timestamp in microseconds */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	int64 Timestamp = 0;

	/** Whether emergency stop is active */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bEmergencyStop = false;

	/** Additional state data as JSON (for extensibility) */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	FString ExtendedStateJSON;
};

/**
 * Arm joint state
 */
USTRUCT(BlueprintType)
struct FRammsArmState
{
	GENERATED_BODY()

	/** Joint names (e.g., ["j0", "j1", "j2", "j3", "j4", "j5"]) */
	UPROPERTY(BlueprintReadOnly, Category = "Arm")
	TArray<FString> JointNames;

	/** Joint positions in radians */
	UPROPERTY(BlueprintReadOnly, Category = "Arm")
	TArray<float> Positions;

	/** Joint velocities in rad/s (may be empty if not available) */
	UPROPERTY(BlueprintReadOnly, Category = "Arm")
	TArray<float> Velocities;

	/** Joint efforts/torques in Nm (may be empty if not available) */
	UPROPERTY(BlueprintReadOnly, Category = "Arm")
	TArray<float> Efforts;

	/** End effector pose (if computed) */
	UPROPERTY(BlueprintReadOnly, Category = "Arm")
	FTransform EndEffectorPose = FTransform::Identity;

	/** Timestamp in microseconds */
	UPROPERTY(BlueprintReadOnly, Category = "Arm")
	int64 Timestamp = 0;

	/** Additional arm state data as JSON (for extensibility) */
	UPROPERTY(BlueprintReadOnly, Category = "Arm")
	FString ExtendedStateJSON;
};

/**
 * Navigation/curb information
 */
USTRUCT(BlueprintType)
struct FRammsCurbInfo
{
	GENERATED_BODY()

	/** Curb detection points in robot frame */
	UPROPERTY(BlueprintReadOnly, Category = "Navigation")
	TArray<FVector> CurbPoints;

	/** Confidence (0.0 to 1.0) */
	UPROPERTY(BlueprintReadOnly, Category = "Navigation")
	float Confidence = 0.0f;

	/** Timestamp in microseconds */
	UPROPERTY(BlueprintReadOnly, Category = "Navigation")
	int64 Timestamp = 0;
};

/**
 * Delegate for robot state updates
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRobotStateUpdate, const FRammsRobotState&);

/**
 * Delegate for arm state updates
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnArmStateUpdate, const FRammsArmState&);

/**
 * Delegate for curb info updates
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCurbInfoUpdate, const FRammsCurbInfo&);

UINTERFACE(MinimalAPI, Blueprintable)
class URammsStateProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Abstract interface for receiving robot state information
 * Implementations: RemoteControl plugin, Socket.IO adapter, ROS 2 bridge
 * 
 * Provides structured access to robot state, arm state, and navigation data.
 */
class RAMMSUI_API IRammsStateProvider
{
	GENERATED_BODY()

public:
	/**
	 * Get the latest robot state
	 * @param OutState - Robot state snapshot
	 * @return True if state is available and valid
	 */
	virtual bool GetRobotState(FRammsRobotState& OutState) const = 0;

	/**
	 * Get the latest arm state
	 * @param OutState - Arm state snapshot
	 * @return True if state is available and valid
	 */
	virtual bool GetArmState(FRammsArmState& OutState) const = 0;

	/**
	 * Get the latest curb information
	 * @param OutInfo - Curb info
	 * @return True if info is available and valid
	 */
	virtual bool GetCurbInfo(FRammsCurbInfo& OutInfo) const = 0;

	/**
	 * Request immediate state update (one-time query)
	 */
	virtual void RequestStateUpdate() = 0;

	/**
	 * Get age of last robot state update (seconds)
	 */
	virtual float GetRobotStateAge() const = 0;

	/**
	 * Get age of last arm state update (seconds)
	 */
	virtual float GetArmStateAge() const = 0;

	// Delegate accessors
	virtual FOnRobotStateUpdate& OnRobotStateUpdate() = 0;
	virtual FOnArmStateUpdate& OnArmStateUpdate() = 0;
	virtual FOnCurbInfoUpdate& OnCurbInfoUpdate() = 0;
};
