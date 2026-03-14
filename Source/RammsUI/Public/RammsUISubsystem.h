// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/IRammsRobotController.h"
#include "RammsUIEventTypes.h"
#include "RammsUISubsystem.generated.h"

/**
 * World subsystem providing two services for the RammsUI widget layer:
 *
 * 1. **Robot Controller Registry** — Actors implementing IRammsRobotController
 *    register/unregister here.  Widgets call FindRobotController() to auto-
 *    discover the actor they should send commands to.
 *
 * 2. **UI Event Bus** — Visualization and UI state events that cross the
 *    widget/actor boundary.  Any widget or actor can broadcast or subscribe.
 *
 * ## Access from C++
 *
 *   URammsUISubsystem* Sub = GetWorld()->GetSubsystem<URammsUISubsystem>();
 *
 * ## Subscribing to events (in any actor or widget)
 *
 *   Sub->OnTaskAction.AddDynamic(this, &AMyActor::HandleTaskAction);
 *   Sub->OnToolbarItemClicked.AddDynamic(this, &AMyActor::HandleToolbar);
 *   Sub->OnVisualizationChanged.AddDynamic(this, &AMyActor::HandleVizChange);
 *
 * ## Widget auto-discovery flow
 *
 * 1. Robot actor implements IRammsRobotController and registers in BeginPlay.
 * 2. Widget sets bAutoFindRobotController=true (done by default for control widgets).
 * 3. Widget::NativeConstruct() calls ResolveController().
 * 4. ResolveController() checks TargetRobotOverride, then queries this subsystem.
 * 5. On success, Widget::OnRobotControllerResolved() is called.
 */
UCLASS()
class RAMMSUI_API URammsUISubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ── Lifecycle ─────────────────────────────────────────────────

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Robot Controller Registry ─────────────────────────────────

	/**
	 * Register an actor as a robot controller.
	 * Call from BeginPlay on any actor implementing IRammsRobotController.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Controllers")
	void RegisterRobotController(AActor* Actor);

	/**
	 * Unregister a robot controller.
	 * Call from EndPlay.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Controllers")
	void UnregisterRobotController(AActor* Actor);

	/**
	 * Find the first registered robot controller.
	 * Returns nullptr if none registered.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Controllers")
	AActor* FindRobotController() const;

	/**
	 * Find a robot controller by name (IRammsRobotController::GetRobotName).
	 * Returns nullptr if not found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Controllers")
	AActor* FindRobotControllerByName(const FString& RobotName) const;

	/**
	 * Get all registered robot controllers.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Controllers")
	TArray<AActor*> GetAllRobotControllers() const;

	/**
	 * Get the number of registered robot controllers.
	 */
	UFUNCTION(BlueprintPure, Category = "Ramms|Controllers")
	int32 GetRobotControllerCount() const { return RegisteredControllers.Num(); }

	// ── Controller Registry Delegates ─────────────────────────────

	/** Broadcast when a controller is registered or unregistered */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnControllerRegistryChanged, AActor*, Actor, bool, bRegistered);
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Controllers")
	FOnControllerRegistryChanged OnControllerRegistryChanged;

	// ── UI Event Bus: Task Actions ────────────────────────────────

	/** Broadcast a task action (Exit, Cancel) to all listeners */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events")
	void BroadcastTaskAction(ERammsTaskAction Action);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaskActionBroadcast, ERammsTaskAction, Action);
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Events")
	FOnTaskActionBroadcast OnTaskAction;

	// ── UI Event Bus: Toolbar ─────────────────────────────────────

	/** Broadcast a toolbar item click to all listeners */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events")
	void BroadcastToolbarItemClicked(FName ItemID);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnToolbarItemBroadcast, FName, ItemID);
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Events")
	FOnToolbarItemBroadcast OnToolbarItemClicked;

	/** Broadcast a toolbar toggle state change */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events")
	void BroadcastToolbarItemToggled(FName ItemID, bool bIsActive);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnToolbarToggleBroadcast, FName, ItemID, bool, bIsActive);
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Events")
	FOnToolbarToggleBroadcast OnToolbarItemToggled;

	// ── UI Event Bus: Visualization (Typed) ──────────────────────

	/** Toggle a camera/viewport overlay */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events|Visualization")
	void BroadcastOverlayToggle(ERammsOverlayType Overlay, bool bVisible);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOverlayToggle, ERammsOverlayType, Overlay, bool, bVisible);
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Events|Visualization")
	FOnOverlayToggle OnOverlayToggle;

	/** Toggle a 3D visualization layer */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events|Visualization")
	void BroadcastVisualizationLayerToggle(ERammsVisualizationLayer Layer, bool bEnabled);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVisualizationLayerToggle, ERammsVisualizationLayer, Layer, bool, bEnabled);
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Events|Visualization")
	FOnVisualizationLayerToggle OnVisualizationLayerToggle;

	/** Highlight a part of the robot */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events|Visualization")
	void BroadcastHighlight(ERammsHighlightTarget Target, bool bHighlighted, FLinearColor Color = FLinearColor::Yellow);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHighlight, ERammsHighlightTarget, Target, bool, bHighlighted, FLinearColor, Color);
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Events|Visualization")
	FOnHighlight OnHighlight;

	// ── UI Event Bus: Custom Events (Blueprint-extensible) ───────

	/**
	 * Broadcast a custom typed event for Blueprint-defined event types.
	 * Use FRammsUIEvent::EventName as the discriminator.
	 * Subscribers filter on EventName to handle only events they care about.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events|Custom")
	void BroadcastCustomUIEvent(const FRammsUIEvent& Event);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCustomUIEvent, const FRammsUIEvent&, Event);
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Events|Custom")
	FOnCustomUIEvent OnCustomUIEvent;

private:
	/** Registered robot controllers (weak references to avoid preventing GC) */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> RegisteredControllers;

	/** Remove stale (destroyed) entries from the registry */
	void CleanupStaleControllers();
};
