// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsSurfaceJoystick.h"
#include "RammsControlSink.h"
#include "RammsUISubsystem.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"

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
		if (URammsUISubsystem* UI = World->GetSubsystem<URammsUISubsystem>())
		{
			UObject* Found = UI->FindControlSurface();
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
