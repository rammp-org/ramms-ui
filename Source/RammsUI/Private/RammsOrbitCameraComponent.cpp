// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsOrbitCameraComponent.h"
#include "RammsTouchInputComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

URammsOrbitCameraComponent::URammsOrbitCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

// ── Lifecycle ─────────────────────────────────────────────────────

void URammsOrbitCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	// Snapshot initial values for ResetOrbit()
	InitialYaw = OrbitYaw;
	InitialPitch = OrbitPitch;
	InitialDistance = OrbitDistance;
	InitialFocusOffset = FocusOffset;

	// Sync targets to current state
	TargetYaw = OrbitYaw;
	TargetPitch = OrbitPitch;
	TargetDistance = OrbitDistance;
	TargetFocusOffset = FocusOffset;

	if (bAutoBindTouchInput)
	{
		BindTouchInput();
	}

	// Apply initial transform
	ApplyConstraints();
	UpdateCameraTransform();
}

void URammsOrbitCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Apply inertia (decay velocities and add to targets)
	if (bEnableInertia)
	{
		float DecayFactor = FMath::Exp(-InertiaDecayRate * DeltaTime);

		if (!RotationVelocity.IsNearlyZero(0.01f))
		{
			TargetYaw += RotationVelocity.X * DeltaTime;
			TargetPitch += RotationVelocity.Y * DeltaTime;
			RotationVelocity *= DecayFactor;
		}
		else
		{
			RotationVelocity = FVector2D::ZeroVector;
		}

		if (!FMath::IsNearlyZero(ZoomVelocity, 0.1f))
		{
			TargetDistance += ZoomVelocity * DeltaTime;
			ZoomVelocity *= DecayFactor;
		}
		else
		{
			ZoomVelocity = 0.0f;
		}

		if (!PanVelocity.IsNearlyZero(0.01f))
		{
			// Convert screen-space pan velocity to world-space offset
			FRotator OrbitRotation(OrbitPitch, OrbitYaw, 0.0f);
			FVector	 Right = FRotationMatrix(OrbitRotation).GetUnitAxis(EAxis::Y);
			FVector	 Up = FRotationMatrix(OrbitRotation).GetUnitAxis(EAxis::Z);

			TargetFocusOffset += Right * PanVelocity.X * DeltaTime;
			TargetFocusOffset += Up * -PanVelocity.Y * DeltaTime;
			PanVelocity *= DecayFactor;
		}
		else
		{
			PanVelocity = FVector2D::ZeroVector;
		}
	}

	ApplyConstraints();

	// Smooth interpolation toward targets
	if (bEnableSmoothing)
	{
		float InterpAlpha = FMath::Clamp(SmoothingSpeed * DeltaTime, 0.0f, 1.0f);
		OrbitYaw = FMath::Lerp(OrbitYaw, TargetYaw, InterpAlpha);
		OrbitPitch = FMath::Lerp(OrbitPitch, TargetPitch, InterpAlpha);
		OrbitDistance = FMath::Lerp(OrbitDistance, TargetDistance, InterpAlpha);
		FocusOffset = FMath::Lerp(FocusOffset, TargetFocusOffset, InterpAlpha);
	}
	else
	{
		OrbitYaw = TargetYaw;
		OrbitPitch = TargetPitch;
		OrbitDistance = TargetDistance;
		FocusOffset = TargetFocusOffset;
	}

	UpdateCameraTransform();
}

// ── Public API ────────────────────────────────────────────────────

void URammsOrbitCameraComponent::SetFocusActor(AActor* NewFocusActor)
{
	FocusActor = NewFocusActor;
}

void URammsOrbitCameraComponent::SetFocusPoint(FVector NewFocusPoint)
{
	FocusPoint = NewFocusPoint;
}

void URammsOrbitCameraComponent::SetOrbitState(float Yaw, float Pitch, float Distance)
{
	OrbitYaw = Yaw;
	OrbitPitch = Pitch;
	OrbitDistance = Distance;
	TargetYaw = Yaw;
	TargetPitch = Pitch;
	TargetDistance = Distance;

	// Clear inertia
	RotationVelocity = FVector2D::ZeroVector;
	ZoomVelocity = 0.0f;

	ApplyConstraints();
	UpdateCameraTransform();
}

FVector URammsOrbitCameraComponent::GetEffectiveFocusPoint() const
{
	FVector Base = FocusPoint;
	if (FocusActor.IsValid())
	{
		Base = FocusActor->GetActorLocation();
	}
	return Base + FocusOffset;
}

void URammsOrbitCameraComponent::SetTouchInputComponent(URammsTouchInputComponent* InTouchInput)
{
	UnbindTouchInput();
	BoundTouchInput = InTouchInput;
	if (BoundTouchInput)
	{
		if (bEnableRotation)
			BoundTouchInput->OnDragRotate.AddUniqueDynamic(this, &URammsOrbitCameraComponent::HandleDragRotate);
		if (bEnableZoom)
			BoundTouchInput->OnPinchZoom.AddUniqueDynamic(this, &URammsOrbitCameraComponent::HandlePinchZoom);
		if (bEnablePan)
			BoundTouchInput->OnTwoFingerPan.AddUniqueDynamic(this, &URammsOrbitCameraComponent::HandleTwoFingerPan);
	}
}

void URammsOrbitCameraComponent::ResetOrbit()
{
	TargetYaw = InitialYaw;
	TargetPitch = InitialPitch;
	TargetDistance = InitialDistance;
	TargetFocusOffset = InitialFocusOffset;

	RotationVelocity = FVector2D::ZeroVector;
	ZoomVelocity = 0.0f;
	PanVelocity = FVector2D::ZeroVector;

	if (!bEnableSmoothing)
	{
		OrbitYaw = TargetYaw;
		OrbitPitch = TargetPitch;
		OrbitDistance = TargetDistance;
		FocusOffset = TargetFocusOffset;
		UpdateCameraTransform();
	}
}

// ── Input Handlers ────────────────────────────────────────────────

void URammsOrbitCameraComponent::HandleDragRotate(FVector2D DeltaDegrees)
{
	if (!bEnableRotation)
		return;

	TargetYaw += DeltaDegrees.X;
	TargetPitch += DeltaDegrees.Y;

	if (bEnableInertia)
	{
		// Track velocity for momentum after release
		RotationVelocity = DeltaDegrees / FMath::Max(GetWorld()->GetDeltaSeconds(), 0.001f);
	}
}

void URammsOrbitCameraComponent::HandlePinchZoom(float ZoomDelta)
{
	if (!bEnableZoom)
		return;

	// Negative delta = zoom in (decrease distance)
	TargetDistance -= ZoomDelta;

	if (bEnableInertia)
	{
		ZoomVelocity = -ZoomDelta / FMath::Max(GetWorld()->GetDeltaSeconds(), 0.001f);
	}
}

void URammsOrbitCameraComponent::HandleTwoFingerPan(FVector2D PanDelta)
{
	if (!bEnablePan)
		return;

	// Convert screen-space pan to world-space offset using current orbit orientation
	FRotator OrbitRotation(OrbitPitch, OrbitYaw, 0.0f);
	FVector	 Right = FRotationMatrix(OrbitRotation).GetUnitAxis(EAxis::Y);
	FVector	 Up = FRotationMatrix(OrbitRotation).GetUnitAxis(EAxis::Z);

	// Scale pan by distance so it feels consistent at different zoom levels
	float DistanceScale = OrbitDistance * 0.001f;
	TargetFocusOffset += Right * -PanDelta.X * DistanceScale;
	TargetFocusOffset += Up * PanDelta.Y * DistanceScale;

	if (bEnableInertia)
	{
		PanVelocity = PanDelta / FMath::Max(GetWorld()->GetDeltaSeconds(), 0.001f);
	}
}

// ── Input Binding ─────────────────────────────────────────────────

void URammsOrbitCameraComponent::BindTouchInput()
{
	// Search for a touch input component on the same actor
	URammsTouchInputComponent* TouchInput = nullptr;

	if (AActor* Owner = GetOwner())
	{
		TouchInput = Owner->FindComponentByClass<URammsTouchInputComponent>();
	}

	// Also check the player controller if we're on a pawn
	if (!TouchInput)
	{
		if (APawn* Pawn = Cast<APawn>(GetOwner()))
		{
			if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
			{
				TouchInput = PC->FindComponentByClass<URammsTouchInputComponent>();
			}
		}
	}

	if (TouchInput)
	{
		SetTouchInputComponent(TouchInput);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("URammsOrbitCameraComponent: No URammsTouchInputComponent found on '%s' or its controller. "
									  "Add one or call SetTouchInputComponent() manually."),
			GetOwner() ? *GetOwner()->GetName() : TEXT("(null)"));
	}
}

void URammsOrbitCameraComponent::UnbindTouchInput()
{
	if (BoundTouchInput)
	{
		BoundTouchInput->OnDragRotate.RemoveDynamic(this, &URammsOrbitCameraComponent::HandleDragRotate);
		BoundTouchInput->OnPinchZoom.RemoveDynamic(this, &URammsOrbitCameraComponent::HandlePinchZoom);
		BoundTouchInput->OnTwoFingerPan.RemoveDynamic(this, &URammsOrbitCameraComponent::HandleTwoFingerPan);
		BoundTouchInput = nullptr;
	}
}

// ── Internal ──────────────────────────────────────────────────────

void URammsOrbitCameraComponent::ApplyConstraints()
{
	TargetPitch = FMath::Clamp(TargetPitch, MinPitch, MaxPitch);
	TargetDistance = FMath::Clamp(TargetDistance, MinDistance, MaxDistance);

	if (bWrapYaw)
	{
		TargetYaw = FMath::Fmod(TargetYaw, 360.0f);
		if (TargetYaw < 0.0f)
			TargetYaw += 360.0f;
	}
}

void URammsOrbitCameraComponent::UpdateCameraTransform()
{
	FVector EffectiveFocus = GetEffectiveFocusPoint();

	// Compute camera position on the orbit sphere
	FRotator OrbitRotation(OrbitPitch, OrbitYaw, 0.0f);
	FVector	 OrbitDirection = OrbitRotation.Vector();
	FVector	 CameraPosition = EffectiveFocus - OrbitDirection * OrbitDistance;

	// Camera looks at the focus point
	FRotator LookAtRotation = (EffectiveFocus - CameraPosition).Rotation();

	// Apply to this component's transform (typically parent of a CameraComponent)
	SetWorldLocationAndRotation(CameraPosition, LookAtRotation);
}
