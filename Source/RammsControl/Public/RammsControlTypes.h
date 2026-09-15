// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RammsControlTypes.generated.h"

/**
 * How a control's value is interpreted.
 *  - Continuous: a normalized command held while the input is active and
 *    expected to spring back to Default when released (drive axes, twist).
 *  - Position: a target the robot moves to and holds (a lift angle, a slide
 *    travel, an endpoint height). Persists until changed.
 *  - Velocity: a target rate, held until changed.
 *  - Action: momentary; TriggerAction only (reset, home, next camera).
 */
UENUM(BlueprintType)
enum class ERammsControlKind : uint8
{
	Continuous UMETA(DisplayName = "Continuous"),
	Position   UMETA(DisplayName = "Position"),
	Velocity   UMETA(DisplayName = "Velocity"),
	Action	   UMETA(DisplayName = "Action"),
};

/** Units of a control's value, for display and for input scaling. */
UENUM(BlueprintType)
enum class ERammsControlUnits : uint8
{
	Normalized			 UMETA(DisplayName = "Normalized (-1..1 / 0..1)"),
	Radians				 UMETA(DisplayName = "Radians"),
	Degrees				 UMETA(DisplayName = "Degrees"),
	Centimeters			 UMETA(DisplayName = "Centimeters"),
	Meters				 UMETA(DisplayName = "Meters"),
	RadiansPerSecond	 UMETA(DisplayName = "Radians / s"),
	CentimetersPerSecond UMETA(DisplayName = "Centimeters / s"),
	None				 UMETA(DisplayName = "None"),
};

/**
 * Who is driving a control. Sinks arbitrate between sources (an external
 * Remote / Autonomy command holds for a timeout over local input, the way the
 * differential drive's external input does) and report which one is active.
 */
UENUM(BlueprintType)
enum class ERammsControlSource : uint8
{
	Keyboard UMETA(DisplayName = "Keyboard"),
	Gamepad	 UMETA(DisplayName = "Gamepad"),
	Touch	 UMETA(DisplayName = "Touch / UI"),
	Remote	 UMETA(DisplayName = "Remote (HTTP / network)"),
	Autonomy UMETA(DisplayName = "Autonomy"),
	Script	 UMETA(DisplayName = "Script"),
};

/**
 * One control a robot exposes. Ids are dotted paths, group first
 * ("drive.forward", "lift.left_elevator", "camera.next"); the UI groups by
 * Group and labels by DisplayName. Everything a generic panel or an input map
 * needs to render / drive the control is here — no robot-specific code.
 */
USTRUCT(BlueprintType)
struct RAMMSCONTROL_API FRammsControlAxis
{
	GENERATED_BODY()

	/** Stable identifier, unique on the robot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	FName Id;

	/** Panel / section this control belongs to ("Drive", "Lift", "Motors"...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	FName Group;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	ERammsControlKind Kind = ERammsControlKind::Continuous;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	ERammsControlUnits Units = ERammsControlUnits::Normalized;

	/** Valid value range (min, max). Ignored for Action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	FVector2D Range = FVector2D(-1.0, 1.0);

	/** Value at rest / after ReleaseAxis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	float DefaultValue = 0.0f;

	/** The robot can report the control's live value (GetAxisValue). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	bool bReadback = true;

	/** Sort key within the group. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	int32 Order = 0;

	/** Continuous axes that pair with another for a 2-D control (drive
	 *  forward/turn as one joystick): the Id of the partner, NAME_None if none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	FName PairedAxis;

	bool  IsAction() const { return Kind == ERammsControlKind::Action; }
	float Clamp(float Value) const { return Range.X < Range.Y ? FMath::Clamp(Value, static_cast<float>(Range.X), static_cast<float>(Range.Y)) : Value; }
};

/** Everything a robot exposes, in presentation order. */
USTRUCT(BlueprintType)
struct RAMMSCONTROL_API FRammsControlSurface
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	FText RobotName;

	/** Groups in the order they should be shown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	TArray<FName> Groups;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	TArray<FRammsControlAxis> Axes;

	const FRammsControlAxis* Find(FName Id) const
	{
		return Axes.FindByPredicate([Id](const FRammsControlAxis& A) { return A.Id == Id; });
	}

	/** Add an axis, registering its group in order of first appearance. */
	void Add(const FRammsControlAxis& Axis)
	{
		Groups.AddUnique(Axis.Group);
		Axes.Add(Axis);
	}

	void GetAxesInGroup(FName Group, TArray<FRammsControlAxis>& Out) const
	{
		for (const FRammsControlAxis& A : Axes)
		{
			if (A.Group == Group)
			{
				Out.Add(A);
			}
		}
		Out.Sort([](const FRammsControlAxis& L, const FRammsControlAxis& R) { return L.Order < R.Order; });
	}
};
