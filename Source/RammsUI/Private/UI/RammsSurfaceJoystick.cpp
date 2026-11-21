// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsSurfaceJoystick.h"
#include "RammsControlSink.h"
#include "RammsControlSurfaceRegistry.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "UI/RammsUIStyle.h"

namespace
{
	/** How fast the thumb follows a mirrored command (FInterpTo speed, 1/s): there in about a fifth of a second. */
	const float MirrorInterpSpeed = 16.0f;
} // namespace

URammsSurfaceJoystick::URammsSurfaceJoystick(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The base joystick also drives a legacy IRammsRobotController it finds
	// by itself; this one talks to the control sink only.
	bAutoFindRobotController = false;
}

void URammsSurfaceJoystick::NativeConstruct()
{
	Super::NativeConstruct();
	OnValueChanged.AddUniqueDynamic(this, &URammsSurfaceJoystick::HandleValue);
	OnReleased.AddUniqueDynamic(this, &URammsSurfaceJoystick::HandleReleased);
}

void URammsSurfaceJoystick::NativeDestruct()
{
	Release();
	Super::NativeDestruct();
}

void URammsSurfaceJoystick::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TickMirror(InDeltaTime);
}

void URammsSurfaceJoystick::TickMirror(float DeltaSeconds)
{
	// The thumb is the user's while they hold it, and stays where they left it on a stick that does not
	// centre itself: it shows their input then, not anyone else's.
	if (bIsActive || !CurrentValue.IsNearlyZero())
	{
		MirrorShown = FVector2D::ZeroVector;
		return;
	}
	FVector2D Wanted = FVector2D::ZeroVector;
	if (bMirrorCommand || URammsUIStyle::UsesSimLook(Style))
	{
		// The commanded target, not the readback: what the keys, a gamepad or a script are asking of the
		// axes now. It springs back with them when the source lets go or times out.
		if (UObject* Sink = ResolveSink())
		{
			float X = 0.0f;
			float Y = 0.0f;
			if (!ControlIdX.IsNone() && IRammsControlSink::Execute_GetAxisTarget(Sink, ControlIdX, X))
			{
				Wanted.X = FMath::Clamp(X, -1.0f, 1.0f);
			}
			if (!ControlIdY.IsNone() && IRammsControlSink::Execute_GetAxisTarget(Sink, ControlIdY, Y))
			{
				Wanted.Y = FMath::Clamp(Y, -1.0f, 1.0f);
			}
			if (Wanted.SizeSquared() > 1.0)
			{
				Wanted = Wanted.GetSafeNormal(); // the base is round: the thumb stays inside it
			}
		}
	}
	// Eased: keys go from nothing to full at once, and a thumb that jumped would read as a glitch.
	FVector2D Next = FMath::Vector2DInterpTo(MirrorShown, Wanted, FMath::Clamp(DeltaSeconds, 0.0f, 0.1f), MirrorInterpSpeed);
	if ((Next - Wanted).SizeSquared() < 1.0e-6)
	{
		Next = Wanted;
	}
	if (Next.Equals(MirrorShown, 0.0))
	{
		return; // settled: nothing to redraw
	}
	MirrorShown = Next;
	const double Reach = FMath::Max(JoystickRadius - ThumbRadius, 0.0f);
	SetThumbOffset(FVector2D(Next.X, bInvertY ? -Next.Y : Next.Y) * Reach);
}

void URammsSurfaceJoystick::SetTarget(UObject* Sink, FName IdX, FName IdY)
{
	Release();
	TargetSink = Sink;
	ControlIdX = IdX;
	ControlIdY = IdY;
}

UObject* URammsSurfaceJoystick::ResolveSink()
{
	if (TargetSink && TargetSink->GetClass()->ImplementsInterface(URammsControlSink::StaticClass()))
	{
		return TargetSink;
	}
	if (!bAutoFindSink)
	{
		return nullptr;
	}
	if (UWorld* World = GetWorld())
	{
		if (URammsControlSurfaceRegistry* Reg = World->GetSubsystem<URammsControlSurfaceRegistry>())
		{
			UObject* Found = Reg->FindControlSurface();
			if (Found && Found->GetClass()->ImplementsInterface(URammsControlSink::StaticClass()))
			{
				TargetSink = Found;
				return Found;
			}
		}
	}
	return nullptr;
}

void URammsSurfaceJoystick::Push(FVector2D Value)
{
	UObject* Sink = ResolveSink();
	if (!Sink)
	{
		return;
	}
	const float Y = static_cast<float>(bInvertY ? -Value.Y : Value.Y);
	if (!ControlIdY.IsNone())
	{
		IRammsControlSink::Execute_SetAxis(Sink, ControlIdY, Y, Source);
	}
	if (!ControlIdX.IsNone())
	{
		IRammsControlSink::Execute_SetAxis(Sink, ControlIdX, static_cast<float>(Value.X), Source);
	}
}

void URammsSurfaceJoystick::Release()
{
	UObject* Sink = TargetSink;
	if (!Sink || !Sink->GetClass()->ImplementsInterface(URammsControlSink::StaticClass()))
	{
		return;
	}
	if (!ControlIdY.IsNone())
	{
		IRammsControlSink::Execute_ReleaseAxis(Sink, ControlIdY, Source);
	}
	if (!ControlIdX.IsNone())
	{
		IRammsControlSink::Execute_ReleaseAxis(Sink, ControlIdX, Source);
	}
}

void URammsSurfaceJoystick::HandleValue(FVector2D Value)
{
	Push(Value);
}

void URammsSurfaceJoystick::HandleReleased()
{
	Release();
}

void URammsSurfaceJoystick::SimulateInput(FVector2D Value)
{
	Push(Value);
}

void URammsSurfaceJoystick::SimulateRelease()
{
	Release();
}

void URammsSurfaceJoystick::SetRadii(float InJoystickRadius, float InThumbRadius)
{
	JoystickRadius = FMath::Max(InJoystickRadius, 1.0f);
	ThumbRadius = FMath::Clamp(InThumbRadius, 1.0f, JoystickRadius);
	// The base builds its canvas at construction with the radii of that
	// moment; a later change must move the images too, or the widget's
	// desired size and its hit maths disagree with what is drawn.
	if (BackgroundImage)
	{
		if (UCanvasPanelSlot* BgSlot = Cast<UCanvasPanelSlot>(BackgroundImage->Slot))
		{
			BgSlot->SetPosition(FVector2D::ZeroVector);
			BgSlot->SetSize(FVector2D(JoystickRadius * 2.0f, JoystickRadius * 2.0f));
		}
	}
	if (ThumbImage)
	{
		if (UCanvasPanelSlot* ThumbSlot = Cast<UCanvasPanelSlot>(ThumbImage->Slot))
		{
			const float Offset = JoystickRadius - ThumbRadius;
			ThumbSlot->SetPosition(FVector2D(Offset, Offset));
			ThumbSlot->SetSize(FVector2D(ThumbRadius * 2.0f, ThumbRadius * 2.0f));
		}
	}
	JoystickCenter = FVector2D(JoystickRadius, JoystickRadius);
}

FReply URammsSurfaceJoystick::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return NativeOnMouseButtonDown(InGeometry, InGestureEvent);
}

FReply URammsSurfaceJoystick::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return NativeOnMouseMove(InGeometry, InGestureEvent);
}

FReply URammsSurfaceJoystick::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	// Also covers a cancelled touch: the base's release recentres and the
	// released delegate lets the axes go.
	const FReply Reply = NativeOnMouseButtonUp(InGeometry, InGestureEvent);
	Release();
	return Reply;
}
