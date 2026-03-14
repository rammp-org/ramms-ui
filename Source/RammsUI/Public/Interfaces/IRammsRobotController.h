// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interfaces/IRammsStateProvider.h"
#include "UI/RammsArmController.h"
#include "UI/RammsMebotController.h"
#include "UI/RammsTaskWidget.h"
#include "IRammsRobotController.generated.h"

UINTERFACE(MinimalAPI, Blueprintable, meta = (DisplayName = "Ramms Robot Controller"))
class URammsRobotController : public UInterface
{
	GENERATED_BODY()
};

/**
 * High-level interface for actors that represent controllable robots.
 *
 * Fully implementable from both C++ and Blueprint.
 *
 * Widgets auto-discover actors implementing this interface via
 * URammsUISubsystem. Each widget calls only the methods relevant
 * to its function (arm widgets call arm methods, etc.).
 *
 * Implementations can delegate to lower-level interfaces
 * (IRammsCommandSink, IRammsStateProvider) internally.
 *
 * ## Registration (required for auto-discovery)
 *
 *   void AMyRobot::BeginPlay()
 *   {
 *       Super::BeginPlay();
 *       GetWorld()->GetSubsystem<URammsUISubsystem>()->RegisterRobotController(this);
 *   }
 *
 *   void AMyRobot::EndPlay(const EEndPlayReason::Type Reason)
 *   {
 *       GetWorld()->GetSubsystem<URammsUISubsystem>()->UnregisterRobotController(this);
 *       Super::EndPlay(Reason);
 *   }
 *
 * ## Implementing a subset
 *
 * Override only the methods your robot supports. All methods have
 * default no-op implementations (return false / do nothing).
 * In C++ override MethodName_Implementation(); in Blueprint override
 * the corresponding event/function node.
 *
 * ## Calling interface methods
 *
 * Always use the Execute_* statics so dispatch works for both C++
 * and Blueprint implementers:
 *
 *   IRammsRobotController::Execute_RequestArmAction(Actor, Action);
 *
 * ## State update notifications
 *
 * Subscribe to URammsUISubsystem delegates (OnControllerRegistryChanged,
 * OnTaskAction, etc.) for cross-actor/widget notifications.
 */
class RAMMSUI_API IRammsRobotController
{
	GENERATED_BODY()

public:
	// ── Identity ──────────────────────────────────────────────────

	/** Human-readable name for this robot (e.g., "MEBot Alpha") */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ramms|Robot")
	FString GetRobotName() const;

	/** Whether the controller is connected and ready to accept commands */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ramms|Robot")
	bool IsConnected() const;

	// ── Arm Control ───────────────────────────────────────────────

	/**
	 * Request an arm action (Home, Retract, etc.)
	 * @return true if the command was accepted
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ramms|Robot|Arm")
	bool RequestArmAction(ERammsArmAction Action);

	/**
	 * Get the current arm state
	 * @param OutState - populated with current joint positions, velocities, etc.
	 * @return true if arm state is available
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ramms|Robot|Arm")
	bool GetArmState(FRammsArmState& OutState) const;

	// ── MEBot / Chair Control ─────────────────────────────────────

	/**
	 * Request a MEBot driving mode change
	 * @return true if the command was accepted
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ramms|Robot|MEBot")
	bool RequestMebotMode(ERammsMebotMode Mode);

	/** Get the currently active MEBot mode */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ramms|Robot|MEBot")
	ERammsMebotMode GetCurrentMebotMode() const;

	// ── Movement ──────────────────────────────────────────────────

	/**
	 * Send joystick-style movement input
	 * @param JoystickValue - normalized (-1..1) per axis, X = forward/back, Y = left/right
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ramms|Robot|Movement")
	void SendMovementInput(FVector2D JoystickValue);

	// ── Task Control ──────────────────────────────────────────────

	/**
	 * Request a task-level action (Exit, Cancel)
	 * @return true if the command was accepted
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ramms|Robot|Task")
	bool RequestTaskAction(ERammsTaskAction Action);

	// ── Robot State ───────────────────────────────────────────────

	/**
	 * Get the current robot state snapshot
	 * @param OutState - populated with battery, speed, mode, e-stop, etc.
	 * @return true if state is available
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ramms|Robot|State")
	bool GetRobotState(FRammsRobotState& OutState) const;
};
