// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsTouchInputComponent.h"
#include "GameFramework/PlayerController.h"

URammsTouchInputComponent::URammsTouchInputComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// ── Lifecycle ─────────────────────────────────────────────────────

void URammsTouchInputComponent::BeginPlay()
{
	Super::BeginPlay();

	// Zero-initialize finger state
	for (int32 i = 0; i < MaxTouches; ++i)
	{
		Fingers[i] = FFingerState();
	}
}

void URammsTouchInputComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnabled)
		return;

	APlayerController* PC = GetPlayerController();
	if (!PC)
		return;

	ProcessTouchInput(PC);
	ProcessMouseInput(PC, DeltaTime);
}

// ── Public API ────────────────────────────────────────────────────

void URammsTouchInputComponent::SetEnabled(bool bNewEnabled)
{
	bEnabled = bNewEnabled;
	if (!bEnabled)
	{
		// Reset all state
		for (int32 i = 0; i < MaxTouches; ++i)
		{
			Fingers[i] = FFingerState();
		}
		ActiveFingerCount = 0;
		bPinchActive = false;
		bMouseDragActive = false;
		bMouseMiddleDragActive = false;
	}
}

bool URammsTouchInputComponent::IsGestureActive() const
{
	return ActiveFingerCount > 0 || bMouseDragActive || bMouseMiddleDragActive;
}

// ── Touch Processing ──────────────────────────────────────────────

void URammsTouchInputComponent::ProcessTouchInput(APlayerController* PC)
{
	// Poll current touch state from the PlayerController
	int32 NewActiveCount = 0;

	for (int32 i = 0; i < MaxTouches; ++i)
	{
		float X = 0.f, Y = 0.f;
		bool  bIsCurrentlyTouched = false;

		PC->GetInputTouchState(static_cast<ETouchIndex::Type>(i), X, Y, bIsCurrentlyTouched);

		FVector2D CurrentPos(X, Y);

		if (bIsCurrentlyTouched)
		{
			++NewActiveCount;

			if (!Fingers[i].bDown)
			{
				// Touch just started
				Fingers[i].bDown = true;
				Fingers[i].Position = CurrentPos;
				Fingers[i].PreviousPosition = CurrentPos;
				Fingers[i].StartPosition = CurrentPos;
				Fingers[i].bPastDeadZone = false;
			}
			else
			{
				// Touch continued — update positions
				Fingers[i].PreviousPosition = Fingers[i].Position;
				Fingers[i].Position = CurrentPos;

				// Check dead zone
				if (!Fingers[i].bPastDeadZone)
				{
					float Dist = FVector2D::Distance(Fingers[i].Position, Fingers[i].StartPosition);
					if (Dist >= DeadZone)
					{
						Fingers[i].bPastDeadZone = true;
						// Snap previous to current to avoid a jump
						Fingers[i].PreviousPosition = Fingers[i].Position;
					}
				}
			}
		}
		else
		{
			if (Fingers[i].bDown)
			{
				// Touch ended
				Fingers[i].bDown = false;
				Fingers[i].bPastDeadZone = false;
			}
		}
	}

	int32 PrevActiveCount = ActiveFingerCount;
	ActiveFingerCount = NewActiveCount;

	// Reset pinch when finger count changes
	if (NewActiveCount != PrevActiveCount)
	{
		bPinchActive = false;
	}

	// ── Gesture dispatch ──────────────────────────────────────

	if (NewActiveCount == 1 && bEnableDragRotate)
	{
		// Single-finger drag → rotation
		for (int32 i = 0; i < MaxTouches; ++i)
		{
			if (Fingers[i].bDown && Fingers[i].bPastDeadZone)
			{
				FVector2D Delta = Fingers[i].Position - Fingers[i].PreviousPosition;
				if (!Delta.IsNearlyZero())
				{
					FVector2D DeltaDegrees(Delta.X * RotateSensitivity, Delta.Y * RotateSensitivity);
					OnDragRotate.Broadcast(DeltaDegrees);
				}
				break;
			}
		}
	}
	else if (NewActiveCount >= 2)
	{
		// Find the first two active fingers
		int32 F0 = INDEX_NONE, F1 = INDEX_NONE;
		for (int32 i = 0; i < MaxTouches; ++i)
		{
			if (Fingers[i].bDown)
			{
				if (F0 == INDEX_NONE)
					F0 = i;
				else if (F1 == INDEX_NONE)
				{
					F1 = i;
					break;
				}
			}
		}

		if (F0 != INDEX_NONE && F1 != INDEX_NONE)
		{
			FVector2D P0 = Fingers[F0].Position;
			FVector2D P1 = Fingers[F1].Position;
			FVector2D Prev0 = Fingers[F0].PreviousPosition;
			FVector2D Prev1 = Fingers[F1].PreviousPosition;

			float CurrentDist = FVector2D::Distance(P0, P1);
			float PrevDist = FVector2D::Distance(Prev0, Prev1);

			// Initialize pinch tracking when two fingers first appear
			if (!bPinchActive)
			{
				PreviousPinchDistance = CurrentDist;
				bPinchActive = true;
			}

			// Pinch zoom — change in distance between fingers
			if (bEnablePinchZoom)
			{
				float DistDelta = CurrentDist - PreviousPinchDistance;
				if (FMath::Abs(DistDelta) > 0.5f)
				{
					OnPinchZoom.Broadcast(DistDelta * PinchZoomSensitivity);
				}
			}
			PreviousPinchDistance = CurrentDist;

			// Two-finger pan — average movement of both fingers
			if (bEnableTwoFingerPan)
			{
				FVector2D AvgDelta = ((P0 - Prev0) + (P1 - Prev1)) * 0.5f;
				if (!AvgDelta.IsNearlyZero(0.5f))
				{
					OnTwoFingerPan.Broadcast(AvgDelta * PanSensitivity);
				}
			}
		}
	}
}

// ── Mouse Processing ──────────────────────────────────────────────

void URammsTouchInputComponent::ProcessMouseInput(APlayerController* PC, float DeltaTime)
{
	// Mouse wheel → zoom
	if (bEnableMouseWheelZoom)
	{
		float WheelDelta = 0.0f;
		PC->GetInputMouseDelta(WheelDelta, WheelDelta); // we actually need the wheel axis

		// UE doesn't expose wheel directly via GetInputMouseDelta.
		// Use the axis binding instead — check if the player is scrolling.
		float WheelAxis = PC->GetInputAxisKeyValue(EKeys::MouseWheelAxis);
		if (!FMath::IsNearlyZero(WheelAxis))
		{
			OnPinchZoom.Broadcast(WheelAxis * MouseWheelZoomSensitivity);
		}
	}

	// Get current mouse position
	float MouseX = 0.f, MouseY = 0.f;
	if (!PC->GetMousePosition(MouseX, MouseY))
	{
		// Mouse not available (e.g., touch-only device)
		bMouseDragActive = false;
		bMouseMiddleDragActive = false;
		return;
	}
	FVector2D CurrentMousePos(MouseX, MouseY);

	// LMB drag → rotate
	if (bEnableMouseDragRotate)
	{
		bool bLMBDown = PC->IsInputKeyDown(EKeys::LeftMouseButton);

		if (bLMBDown && !bMouseDragActive)
		{
			// Only start mouse drag if no touch is active (avoid conflict)
			if (ActiveFingerCount == 0)
			{
				bMouseDragActive = true;
				PreviousMousePos = CurrentMousePos;
			}
		}
		else if (!bLMBDown)
		{
			bMouseDragActive = false;
		}

		if (bMouseDragActive)
		{
			FVector2D Delta = CurrentMousePos - PreviousMousePos;
			if (!Delta.IsNearlyZero())
			{
				FVector2D DeltaDegrees(Delta.X * RotateSensitivity, Delta.Y * RotateSensitivity);
				OnDragRotate.Broadcast(DeltaDegrees);
			}
			PreviousMousePos = CurrentMousePos;
		}
	}

	// MMB drag → pan
	if (bEnableMouseDragPan)
	{
		bool bMMBDown = PC->IsInputKeyDown(EKeys::MiddleMouseButton);

		if (bMMBDown && !bMouseMiddleDragActive)
		{
			if (ActiveFingerCount == 0)
			{
				bMouseMiddleDragActive = true;
				PreviousMousePos = CurrentMousePos;
			}
		}
		else if (!bMMBDown)
		{
			bMouseMiddleDragActive = false;
		}

		if (bMouseMiddleDragActive)
		{
			FVector2D Delta = CurrentMousePos - PreviousMousePos;
			if (!Delta.IsNearlyZero())
			{
				OnTwoFingerPan.Broadcast(Delta * PanSensitivity);
			}
			PreviousMousePos = CurrentMousePos;
		}
	}
}

// ── Utilities ─────────────────────────────────────────────────────

APlayerController* URammsTouchInputComponent::GetPlayerController() const
{
	// If attached to a PlayerController, use it directly
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		return PC;
	}

	// If attached to a Pawn, get its controller
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Cast<APlayerController>(Pawn->GetController());
	}

	// Fallback: first local player controller
	if (UWorld* World = GetWorld())
	{
		return World->GetFirstPlayerController();
	}

	return nullptr;
}
