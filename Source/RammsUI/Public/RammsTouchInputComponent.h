// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RammsTouchInputComponent.generated.h"

/**
 * Cross-platform touch/mouse gesture processor.
 *
 * Polls APlayerController touch state each tick and detects:
 *   - 1-finger drag  → OnDragRotate  (yaw/pitch delta in degrees)
 *   - 2-finger pinch → OnPinchZoom   (signed distance delta, positive = zoom in)
 *   - 2-finger drag  → OnTwoFingerPan(screen-space delta)
 *   - Mouse wheel     → OnPinchZoom  (for desktop parity)
 *   - Mouse drag      → OnDragRotate (LMB) / OnTwoFingerPan (MMB)
 *
 * Attach to any Pawn or PlayerController. Other components (e.g.
 * URammsOrbitCameraComponent) bind to the delegates to respond.
 *
 * Works on Windows, Android, iOS, and Linux via UE's unified input layer.
 */
UCLASS(ClassGroup = (Ramms), meta = (BlueprintSpawnableComponent))
class RAMMSUI_API URammsTouchInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URammsTouchInputComponent();

	// ── Configuration ─────────────────────────────────────────────

	/** Enable single-finger drag → rotation gesture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gestures")
	bool bEnableDragRotate = true;

	/** Enable two-finger pinch → zoom gesture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gestures")
	bool bEnablePinchZoom = true;

	/** Enable two-finger drag → pan gesture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gestures")
	bool bEnableTwoFingerPan = true;

	/** Enable mouse wheel → zoom (desktop platforms). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gestures|Mouse")
	bool bEnableMouseWheelZoom = true;

	/** Enable mouse LMB drag → rotate (desktop platforms). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gestures|Mouse")
	bool bEnableMouseDragRotate = true;

	/** Enable mouse MMB drag → pan (desktop platforms). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gestures|Mouse")
	bool bEnableMouseDragPan = true;

	/** Rotation sensitivity multiplier (degrees per pixel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensitivity", meta = (ClampMin = "0.01"))
	float RotateSensitivity = 0.25f;

	/** Zoom sensitivity multiplier for touch pinch (units per pixel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensitivity", meta = (ClampMin = "0.01"))
	float PinchZoomSensitivity = 1.0f;

	/** Zoom sensitivity multiplier for mouse wheel (units per wheel notch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensitivity", meta = (ClampMin = "0.01"))
	float MouseWheelZoomSensitivity = 50.0f;

	/** Pan sensitivity multiplier (units per pixel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensitivity", meta = (ClampMin = "0.01"))
	float PanSensitivity = 1.0f;

	/** Minimum finger movement (pixels) before a gesture is recognized. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensitivity", meta = (ClampMin = "0"))
	float DeadZone = 4.0f;

	// ── Delegates ─────────────────────────────────────────────────

	/** Fired each frame during a rotation drag.  Delta is in degrees (X=Yaw, Y=Pitch). */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDragRotate, FVector2D, DeltaDegrees);
	UPROPERTY(BlueprintAssignable, Category = "Gestures")
	FOnDragRotate OnDragRotate;

	/** Fired each frame during a pinch or mouse wheel.  Positive = zoom in. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPinchZoom, float, ZoomDelta);
	UPROPERTY(BlueprintAssignable, Category = "Gestures")
	FOnPinchZoom OnPinchZoom;

	/** Fired each frame during a two-finger / middle-mouse pan.  Delta in screen pixels. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTwoFingerPan, FVector2D, PanDelta);
	UPROPERTY(BlueprintAssignable, Category = "Gestures")
	FOnTwoFingerPan OnTwoFingerPan;

	// ── Public API ────────────────────────────────────────────────

	/** Globally enable or disable all gesture processing. */
	UFUNCTION(BlueprintCallable, Category = "Gestures")
	void SetEnabled(bool bNewEnabled);

	UFUNCTION(BlueprintPure, Category = "Gestures")
	bool IsEnabled() const { return bEnabled; }

	/** Returns true while the user is actively performing any gesture. */
	UFUNCTION(BlueprintPure, Category = "Gestures")
	bool IsGestureActive() const;

	// ── Lifecycle ─────────────────────────────────────────────────

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	// ── Internal state ────────────────────────────────────────────

	bool bEnabled = true;

	/** Per-finger tracking. */
	static constexpr int32 MaxTouches = 10;

	struct FFingerState
	{
		bool	  bDown = false;
		FVector2D Position = FVector2D::ZeroVector;
		FVector2D PreviousPosition = FVector2D::ZeroVector;
		FVector2D StartPosition = FVector2D::ZeroVector;
		bool	  bPastDeadZone = false;
	};
	FFingerState Fingers[MaxTouches];

	/** Number of fingers currently down. */
	int32 ActiveFingerCount = 0;

	/** Pinch state. */
	float PreviousPinchDistance = 0.0f;
	bool  bPinchActive = false;

	/** Mouse state. */
	FVector2D PreviousMousePos = FVector2D::ZeroVector;
	bool	  bMouseDragActive = false;
	bool	  bMouseMiddleDragActive = false;
	float	  AccumulatedWheelDelta = 0.0f;

	// ── Processing helpers ────────────────────────────────────────

	void ProcessTouchInput(APlayerController* PC);
	void ProcessMouseInput(APlayerController* PC, float DeltaTime);

	APlayerController* GetPlayerController() const;
};
