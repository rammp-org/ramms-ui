// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RammsOrbitCameraComponent.generated.h"

class URammsTouchInputComponent;
class UCameraComponent;

/**
 * Orbital camera controller that rotates around a focus point.
 *
 * Attach to any actor that has a UCameraComponent. The orbit component
 * controls the camera's transform by maintaining yaw, pitch, and distance
 * relative to a configurable focus point.
 *
 * Input is driven by URammsTouchInputComponent — bind automatically via
 * bAutoBindTouchInput, or manually call SetTouchInputComponent().
 *
 * Focus point is either:
 *   - A target actor (FocusActor), using its root component origin
 *   - A manual world-space position (FocusPoint)
 *   - FocusActor takes priority when set and valid
 *
 * Supports:
 *   - Orbit rotation (yaw/pitch) with configurable limits
 *   - Zoom (distance from focus) with min/max bounds
 *   - Pan offset (shifts the focus point laterally)
 *   - Smooth interpolation and optional inertia/momentum
 */
UCLASS(ClassGroup = (Ramms), meta = (BlueprintSpawnableComponent))
class RAMMSUI_API URammsOrbitCameraComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	URammsOrbitCameraComponent();

	// ── Focus Target ──────────────────────────────────────────────

	/** Actor to orbit around.  Takes priority over FocusPoint when valid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|Focus")
	TWeakObjectPtr<AActor> FocusActor;

	/** Manual world-space focus point (used when FocusActor is null). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|Focus")
	FVector FocusPoint = FVector::ZeroVector;

	/** Offset applied to the focus point in the camera's local right/up plane.
	 *  Useful for framing the subject off-center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|Focus")
	FVector FocusOffset = FVector::ZeroVector;

	// ── Orbit State ───────────────────────────────────────────────

	/** Current orbit yaw in degrees (0 = forward along world X). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|State")
	float OrbitYaw = 0.0f;

	/** Current orbit pitch in degrees (positive = looking down). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|State")
	float OrbitPitch = -30.0f;

	/** Current distance from the focus point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|State", meta = (ClampMin = "1.0"))
	float OrbitDistance = 500.0f;

	// ── Constraints ───────────────────────────────────────────────

	/** Minimum orbit distance (zoom-in limit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|Constraints", meta = (ClampMin = "0.1"))
	float MinDistance = 50.0f;

	/** Maximum orbit distance (zoom-out limit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|Constraints", meta = (ClampMin = "1.0"))
	float MaxDistance = 5000.0f;

	/** Minimum pitch in degrees (looking up limit, typically negative). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|Constraints")
	float MinPitch = -89.0f;

	/** Maximum pitch in degrees (looking down limit, typically positive). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|Constraints")
	float MaxPitch = 89.0f;

	/** When true, yaw wraps around 360°. When false, yaw is unconstrained. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|Constraints")
	bool bWrapYaw = true;

	// ── Smoothing ─────────────────────────────────────────────────

	/** Enable smooth interpolation toward target orbit values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|Smoothing")
	bool bEnableSmoothing = true;

	/** Interpolation speed (higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|Smoothing", meta = (ClampMin = "1.0", EditCondition = "bEnableSmoothing"))
	float SmoothingSpeed = 10.0f;

	/** Enable momentum/inertia after releasing a gesture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|Smoothing")
	bool bEnableInertia = true;

	/** Rate at which inertia decays (per second). 0 = instant stop, 10 = very smooth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|Smoothing", meta = (ClampMin = "0.0", ClampMax = "20.0", EditCondition = "bEnableInertia"))
	float InertiaDecayRate = 5.0f;

	// ── Input Binding ─────────────────────────────────────────────

	/** Automatically find and bind to a URammsTouchInputComponent on the same actor or its controller. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|Input")
	bool bAutoBindTouchInput = true;

	/** Enable orbit rotation via touch/mouse drag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|Input")
	bool bEnableRotation = true;

	/** Enable zoom via pinch/mouse wheel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|Input")
	bool bEnableZoom = true;

	/** Enable pan via two-finger drag / middle mouse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit|Input")
	bool bEnablePan = true;

	// ── Public API ────────────────────────────────────────────────

	/** Set the focus target actor.  Pass nullptr to use FocusPoint instead. */
	UFUNCTION(BlueprintCallable, Category = "Orbit")
	void SetFocusActor(AActor* NewFocusActor);

	/** Set the manual focus point (only used when FocusActor is null). */
	UFUNCTION(BlueprintCallable, Category = "Orbit")
	void SetFocusPoint(FVector NewFocusPoint);

	/** Set orbit yaw/pitch/distance directly (bypasses smoothing). */
	UFUNCTION(BlueprintCallable, Category = "Orbit")
	void SetOrbitState(float Yaw, float Pitch, float Distance);

	/** Get the current effective focus point (accounts for FocusActor). */
	UFUNCTION(BlueprintPure, Category = "Orbit")
	FVector GetEffectiveFocusPoint() const;

	/** Manually bind a specific touch input component. */
	UFUNCTION(BlueprintCallable, Category = "Orbit|Input")
	void SetTouchInputComponent(URammsTouchInputComponent* InTouchInput);

	/** Reset orbit to initial values. */
	UFUNCTION(BlueprintCallable, Category = "Orbit")
	void ResetOrbit();

	// ── Lifecycle ─────────────────────────────────────────────────

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	// ── Input handlers ────────────────────────────────────────────

	UFUNCTION()
	void HandleDragRotate(FVector2D DeltaDegrees);

	UFUNCTION()
	void HandlePinchZoom(float ZoomDelta);

	UFUNCTION()
	void HandleTwoFingerPan(FVector2D PanDelta);

	void BindTouchInput();
	void UnbindTouchInput();

	// ── Internal state ────────────────────────────────────────────

	UPROPERTY()
	TObjectPtr<URammsTouchInputComponent> BoundTouchInput;

	// Target values for smooth interpolation
	float	TargetYaw = 0.0f;
	float	TargetPitch = -30.0f;
	float	TargetDistance = 500.0f;
	FVector TargetFocusOffset = FVector::ZeroVector;

	// Inertia velocities
	FVector2D RotationVelocity = FVector2D::ZeroVector;
	float	  ZoomVelocity = 0.0f;
	FVector2D PanVelocity = FVector2D::ZeroVector;

	// Initial values for reset
	float	InitialYaw = 0.0f;
	float	InitialPitch = -30.0f;
	float	InitialDistance = 500.0f;
	FVector InitialFocusOffset = FVector::ZeroVector;

	/** Apply constraints to target values. */
	void ApplyConstraints();

	/** Compute and set the camera transform based on current orbit state. */
	void UpdateCameraTransform();
};
