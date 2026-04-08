// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/IRammsRobotController.h"
#include "RammsUIEventTypes.h"
#include "RammsDetectionTypes.h"
#include "UI/RammsUIStyle.h"
#include "RammsUITransitionTypes.h"
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
 *   Sub->OnOverlayToggle.AddDynamic(this, &AMyActor::HandleOverlay);
 *   Sub->OnVisualizationLayerToggle.AddDynamic(this, &AMyActor::HandleVizLayer);
 *   Sub->OnHighlight.AddDynamic(this, &AMyActor::HandleHighlight);
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
	AActor* FindRobotController();

	/**
	 * Find a robot controller by name (IRammsRobotController::GetRobotName).
	 * Returns nullptr if not found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Controllers")
	AActor* FindRobotControllerByName(const FString& RobotName);

	/**
	 * Get all registered robot controllers.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Controllers")
	TArray<AActor*> GetAllRobotControllers();

	/**
	 * Get the number of currently valid registered robot controllers.
	 */
	UFUNCTION(BlueprintPure, Category = "Ramms|Controllers")
	int32 GetRobotControllerCount();

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

	// ── UI Event Bus: Arm Task ────────────────────────────────────

	/** Broadcast an arm task change to all listeners */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events")
	void BroadcastArmTaskChanged(ERammsArmTask NewTask, ERammsArmTask PreviousTask);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnArmTaskChanged, ERammsArmTask, NewTask, ERammsArmTask, PreviousTask);
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Events")
	FOnArmTaskChanged OnArmTaskChanged;

	// ── UI Event Bus: Robot State ────────────────────────────────

	/**
	 * Broadcast a robot state update to all listeners.
	 * Called by URammsRemoteBridge when state arrives via Remote Control.
	 * Also caches the state for late-joining widgets.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events")
	void BroadcastRobotStateChanged(const FRammsRobotState& State);

	/** Get the most recently broadcast robot state (Blueprint-safe, returns by value). */
	UFUNCTION(BlueprintPure, Category = "Ramms|Events")
	FRammsRobotState GetCachedRobotState() const { return CachedRobotState; }

	/** C++ helper — returns by const reference to avoid copy when Blueprint exposure isn't needed. */
	const FRammsRobotState& GetCachedRobotStateRef() const { return CachedRobotState; }

	/** Returns true if at least one robot state has been broadcast this session. */
	UFUNCTION(BlueprintPure, Category = "Ramms|Events")
	bool HasRobotState() const { return bHasRobotState; }

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRobotStateChanged, const FRammsRobotState&, State);
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Events")
	FOnRobotStateChanged OnRobotStateChanged;

	// ── Per-field Robot State Setters ────────────────────────────

	/** Update only the robot mode. Merges into cached state and broadcasts. */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events")
	void UpdateRobotMode(ERammsRobotMode Mode);

	/** Update only the battery level (0-1). Merges into cached state and broadcasts. */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events")
	void UpdateBatteryLevel(float Level);

	/** Update only the speed (forward velocity m/s). Merges into cached state and broadcasts. */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events")
	void UpdateSpeed(float SpeedMPS);

	/** Update only the linear velocity. Merges into cached state and broadcasts. */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events")
	void UpdateLinearVelocity(FVector Velocity);

	/** Update only the angular velocity. Merges into cached state and broadcasts. */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events")
	void UpdateAngularVelocity(FRotator Velocity);

	/** Update only the emergency stop flag. Sets mode to Emergency if active. Merges and broadcasts. */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events")
	void UpdateEmergencyStop(bool bActive);

	/** Update only the base pose. Merges into cached state and broadcasts. */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events")
	void UpdateBasePose(FTransform Pose);

	// ── UI Event Bus: Key-Value Property Store ──────────────────

	/**
	 * Set a named property value. Broadcasts OnPropertyChanged.
	 * Useful for prototyping / extensible data from the robot or external systems.
	 * Callable via Remote Control: PUT SetProperty(Key, Value).
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Properties")
	void SetProperty(FName Key, const FString& Value);

	/**
	 * Get a named property value. Returns empty string if not set.
	 */
	UFUNCTION(BlueprintPure, Category = "Ramms|Properties")
	FString GetProperty(FName Key) const;

	/**
	 * Get a named property as a float. Returns DefaultValue if not set or not parseable.
	 */
	UFUNCTION(BlueprintPure, Category = "Ramms|Properties")
	float GetPropertyAsFloat(FName Key, float DefaultValue = 0.0f) const;

	/**
	 * Get a named property as an integer. Returns DefaultValue if not set or not parseable.
	 */
	UFUNCTION(BlueprintPure, Category = "Ramms|Properties")
	int32 GetPropertyAsInt(FName Key, int32 DefaultValue = 0) const;

	/**
	 * Get a named property as a bool. Returns DefaultValue if not set.
	 * Truthy values: "true", "1", "yes" (case-insensitive).
	 */
	UFUNCTION(BlueprintPure, Category = "Ramms|Properties")
	bool GetPropertyAsBool(FName Key, bool DefaultValue = false) const;

	/**
	 * Get a named property as a byte. Returns DefaultValue if not set.
	 * Useful for reading enum values stored via SetPropertyAsByte.
	 */
	UFUNCTION(BlueprintPure, Category = "Ramms|Properties")
	uint8 GetPropertyAsByte(FName Key, uint8 DefaultValue = 0) const;

	/** Check if a property exists. */
	UFUNCTION(BlueprintPure, Category = "Ramms|Properties")
	bool HasProperty(FName Key) const;

	/** Remove a property. Broadcasts OnPropertyChanged with empty value. */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Properties")
	void RemoveProperty(FName Key);

	/** Get all property keys. */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Properties")
	TArray<FName> GetAllPropertyKeys() const;

	/**
	 * Set multiple properties at once. Broadcasts OnPropertyChanged for each.
	 * More efficient than calling SetProperty in a loop from Remote Control.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Properties")
	void SetProperties(const TMap<FName, FString>& Properties);

	// ── Typed Property Setters ───────────────────────────────────

	/** Set a property as a byte value. Enum pins auto-cast to uint8 in Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Properties")
	void SetPropertyAsByte(FName Key, uint8 Value);

	/** Set a property as a float value. */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Properties")
	void SetPropertyAsFloat(FName Key, float Value);

	/** Set a property as an integer value. */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Properties")
	void SetPropertyAsInt(FName Key, int32 Value);

	/** Set a property as a boolean value. */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Properties")
	void SetPropertyAsBool(FName Key, bool Value);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPropertyChanged, FName, Key, const FString&, Value);
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Properties")
	FOnPropertyChanged OnPropertyChanged;

	// ── UI Event Bus: Seat State ─────────────────────────────────

	/** Broadcast a seat state change to all listeners */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events")
	void BroadcastSeatStateChanged(ERammsSeatAxis Axis, float NewValue);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSeatStateChanged, ERammsSeatAxis, Axis, float, NewValue);
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Events")
	FOnSeatStateChanged OnSeatStateChanged;

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

	// ── UI Event Bus: Layout Transition Requests ────────────────

	/**
	 * Request a layout transition. Any widget can call this.
	 * The LayoutHost subscribes and performs the actual transition.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events|Layout")
	void BroadcastLayoutTransitionRequest(FName LayoutName, bool bAnimated = true,
		ERammsSlideDirection SlideDirection = ERammsSlideDirection::Auto);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLayoutTransitionRequest, FName, LayoutName, bool, bAnimated, ERammsSlideDirection, SlideDirection);
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Events|Layout")
	FOnLayoutTransitionRequest OnLayoutTransitionRequested;

	// ── UI Event Bus: Detection / Bounding Boxes ────────────────

	/**
	 * Broadcast a set of 2D detections (bounding boxes) from a named source.
	 * Any URammsBoundingBoxOverlay widgets subscribed to this source will update.
	 * @param SourceTag - Identifies the detection source (e.g., camera name, detector name)
	 * @param Boxes - Array of bounding boxes in normalized 0-1 image coordinates
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events|Detection")
	void BroadcastDetections(FName SourceTag, const TArray<FRammsBoundingBox>& Boxes);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDetectionsReceived, FName, SourceTag, const TArray<FRammsBoundingBox>&, Boxes);
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Events|Detection")
	FOnDetectionsReceived OnDetectionsReceived;

	/**
	 * Broadcast a clear signal for all detections from a named source.
	 * Subscribed overlays matching this source will clear their boxes.
	 * @param SourceTag - Source to clear (NAME_None clears all sources)
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events|Detection")
	void ClearDetections(FName SourceTag);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDetectionsCleared, FName, SourceTag);
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Events|Detection")
	FOnDetectionsCleared OnDetectionsCleared;

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

	/**
	 * Broadcast a custom event carrying a struct payload.
	 * Use "Make InstancedStruct" in Blueprint to create the payload.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events|Custom", meta = (DisplayName = "Broadcast Custom Event (Struct)"))
	void BroadcastCustomUIEventWithPayload(FName EventName, const FInstancedStruct& Payload);

	/**
	 * Broadcast a custom event carrying key-value properties.
	 * Subscribers read values via FRammsUIEvent::Properties or typed getters.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Events|Custom", meta = (DisplayName = "Broadcast Custom Event (Properties)"))
	void BroadcastCustomUIEventWithProperties(FName EventName, const TMap<FName, FString>& EventProperties);

	// ── Robot Command Dispatch ────────────────────────────────────
	//
	// Convenience methods that find the primary (first) registered
	// controller and call the corresponding IRammsRobotController
	// method. Widgets can call these instead of manually resolving
	// the controller.  Each also fires OnRobotCommandSent for logging /
	// telemetry / UI feedback.

	/**
	 * Send a named command to the primary robot controller.
	 * @return true if a controller was found AND accepted the command
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Commands", meta = (DisplayName = "Send Robot Command"))
	bool SendRobotCommand(FName CommandName);

	/**
	 * Send a named command with a byte/enum value to the primary robot controller.
	 * @return true if a controller was found AND accepted the command
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Commands", meta = (DisplayName = "Send Robot Command (Value)"))
	bool SendRobotCommandWithValue(FName CommandName, uint8 Value);

	/**
	 * Send a named command with a struct payload to the primary robot controller.
	 * Use "Make Instanced Struct" in Blueprint to wrap any USTRUCT.
	 * @return true if a controller was found AND accepted the command
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Commands", meta = (DisplayName = "Send Robot Command (Struct)"))
	bool SendRobotCommandWithPayload(FName CommandName, const FInstancedStruct& Payload);

	/**
	 * Broadcast a named command to ALL registered robot controllers.
	 * @return number of controllers that accepted the command
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Commands", meta = (DisplayName = "Broadcast Robot Command"))
	int32 BroadcastRobotCommand(FName CommandName);

	/**
	 * Broadcast a named command with a byte/enum value to ALL registered controllers.
	 * @return number of controllers that accepted the command
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Commands", meta = (DisplayName = "Broadcast Robot Command (Value)"))
	int32 BroadcastRobotCommandWithValue(FName CommandName, uint8 Value);

	/**
	 * Broadcast a named command with a struct payload to ALL registered controllers.
	 * @return number of controllers that accepted the command
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Commands", meta = (DisplayName = "Broadcast Robot Command (Struct)"))
	int32 BroadcastRobotCommandWithPayload(FName CommandName, const FInstancedStruct& Payload);

	/** Fired after any command is dispatched (for logging / UI feedback / telemetry) */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRobotCommandSent, FName, CommandName, bool, bAccepted);
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Commands")
	FOnRobotCommandSent OnRobotCommandSent;

	// ── Theme Management ─────────────────────────────────────────
	//
	// The subsystem owns the current UI theme.  Calling SetTheme()
	// stores the style, broadcasts OnThemeChanged, and propagates
	// the new style to all live URammsBaseWidget instances.

	/**
	 * Set the active UI theme. Pass nullptr to clear.
	 * Propagates to all live RammsBaseWidget instances.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Theme")
	void SetTheme(URammsUIStyle* NewStyle);

	/**
	 * Get the current active theme.
	 */
	UFUNCTION(BlueprintPure, Category = "Ramms|Theme")
	URammsUIStyle* GetTheme() const { return CurrentTheme; }

	/**
	 * Switch to the dark theme.
	 * Uses DefaultDarkTheme if set, otherwise falls back to built-in factory.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Theme")
	void SetDarkTheme();

	/**
	 * Switch to the light theme.
	 * Uses DefaultLightTheme if set, otherwise falls back to built-in factory.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Theme")
	void SetLightTheme();

	/**
	 * Optional user-created dark theme DataAsset.
	 * When set, SetDarkTheme() uses this instead of the built-in factory.
	 * Create a URammsUIStyle DataAsset in the editor and assign it here.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramms|Theme")
	TObjectPtr<URammsUIStyle> DefaultDarkTheme;

	/**
	 * Optional user-created light theme DataAsset.
	 * When set, SetLightTheme() uses this instead of the built-in factory.
	 * Create a URammsUIStyle DataAsset in the editor and assign it here.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramms|Theme")
	TObjectPtr<URammsUIStyle> DefaultLightTheme;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThemeChanged, URammsUIStyle*, NewStyle);

	/** Fired when the active UI theme changes */
	UPROPERTY(BlueprintAssignable, Category = "Ramms|Theme")
	FOnThemeChanged OnThemeChanged;

private:
	/** Registered robot controllers (weak references to avoid preventing GC) */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> RegisteredControllers;

	/** Cached robot state from most recent broadcast */
	FRammsRobotState CachedRobotState;
	bool			 bHasRobotState = false;

	/** Key-value property store */
	TMap<FName, FString> PropertyStore;

	/** Current active UI theme */
	UPROPERTY(Transient)
	TObjectPtr<URammsUIStyle> CurrentTheme;

	/** Cached built-in dark theme (created on first use) */
	UPROPERTY(Transient)
	TObjectPtr<URammsUIStyle> BuiltinDarkTheme;

	/** Cached built-in light theme (created on first use) */
	UPROPERTY(Transient)
	TObjectPtr<URammsUIStyle> BuiltinLightTheme;

	/** Remove stale (destroyed) entries from the registry */
	void CleanupStaleControllers();
};
