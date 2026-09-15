// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsSurfaceJoystick.h"
#include "RammsControlSink.h"
#include "RammsUISubsystem.h"

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
