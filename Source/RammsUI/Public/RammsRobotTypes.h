// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RammsRobotTypes.generated.h"

/**
 * Shared enums and types used by the robot controller interface and UI widgets.
 *
 * This header is intentionally free of any UI / widget dependencies so that
 * IRammsRobotController.h (and any other non-UI code) can reference these
 * types without pulling in the widget layer.
 */

// ── Arm ──────────────────────────────────────────────────────────

/** Arm action types */
UENUM(BlueprintType)
enum class ERammsArmAction : uint8
{
	Home	UMETA(DisplayName = "Home"),
	Retract UMETA(DisplayName = "Retract")
};

// ── MEBot / Chair ────────────────────────────────────────────────

/** MEBot driving modes */
UENUM(BlueprintType)
enum class ERammsMebotMode : uint8
{
	None		UMETA(DisplayName = "None"),
	SelfLevel	UMETA(DisplayName = "Self-Levelling"),
	CurbAscent	UMETA(DisplayName = "Curb Ascent"),
	CurbDescent UMETA(DisplayName = "Curb Descent")
};

// ── Arm Tasks ────────────────────────────────────────────────────

/** Arm-level task selection (mutually exclusive) */
UENUM(BlueprintType)
enum class ERammsArmTask : uint8
{
	None	   UMETA(DisplayName = "None"),
	OpenDoor   UMETA(DisplayName = "Open Door"),
	OrderDrink UMETA(DisplayName = "Order Drink"),
	Drink	   UMETA(DisplayName = "Drink")
};

// ── Seat Control ─────────────────────────────────────────────────

/** Seat adjustment axes */
UENUM(BlueprintType)
enum class ERammsSeatAxis : uint8
{
	Elevation			  UMETA(DisplayName = "Elevation"),
	LateralTilt			  UMETA(DisplayName = "Lateral Tilt"),
	AnteriorPosteriorTilt UMETA(DisplayName = "A/P Tilt")
};

/** Current seat state (all axes) */
USTRUCT(BlueprintType)
struct FRammsSeatState
{
	GENERATED_BODY()

	/** Elevation value (normalized 0..1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	float Elevation = 0.0f;

	/** Lateral tilt value (normalized -1..1, negative=left, positive=right) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	float LateralTilt = 0.0f;

	/** Anterior/posterior tilt value (normalized -1..1, negative=anterior, positive=posterior) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	float AnteriorPosteriorTilt = 0.0f;

	/** Get value for a specific axis */
	float GetAxisValue(ERammsSeatAxis Axis) const
	{
		switch (Axis)
		{
			case ERammsSeatAxis::Elevation:
				return Elevation;
			case ERammsSeatAxis::LateralTilt:
				return LateralTilt;
			case ERammsSeatAxis::AnteriorPosteriorTilt:
				return AnteriorPosteriorTilt;
			default:
				return 0.0f;
		}
	}

	/** Set value for a specific axis */
	void SetAxisValue(ERammsSeatAxis Axis, float Value)
	{
		switch (Axis)
		{
			case ERammsSeatAxis::Elevation:
				Elevation = Value;
				break;
			case ERammsSeatAxis::LateralTilt:
				LateralTilt = Value;
				break;
			case ERammsSeatAxis::AnteriorPosteriorTilt:
				AnteriorPosteriorTilt = Value;
				break;
		}
	}
};

// ── Axis Config ──────────────────────────────────────────────────

class UTexture2D;

/**
 * Configuration for a single controllable axis.
 * Groups icon, range, default value, display formatting, and step size
 * into a single structure for cleaner details-panel grouping and reuse.
 */
USTRUCT(BlueprintType)
struct FRammsAxisConfig
{
	GENERATED_BODY()

	/** Display label for this axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axis")
	FText Label;

	/** Icon texture displayed beside the control */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axis")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** Icon display size in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axis")
	FVector2D IconSize = FVector2D(28.0f, 28.0f);

	/** Minimum value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axis")
	float MinValue = 0.0f;

	/** Maximum value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axis")
	float MaxValue = 1.0f;

	/** Default / home value (used by the reset button) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axis")
	float DefaultValue = 0.0f;

	/** Step size in actual value units (0 = continuous). Used by the slider and +/- buttons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axis", meta = (ClampMin = "0.0"))
	float StepSize = 0.0f;

	/** Units suffix for value display (e.g. "°", "%") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axis")
	FText Units;

	/** Number of decimal places in value display */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axis", meta = (ClampMin = "0", ClampMax = "3"))
	int32 DecimalPlaces = 1;

	/** Whether to show the slider control */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axis")
	bool bShowSlider = true;

	/** Whether to show +/- increment/decrement buttons */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axis")
	bool bShowButtons = true;
};

// ── Task ─────────────────────────────────────────────────────────

/** Task action types */
UENUM(BlueprintType)
enum class ERammsTaskAction : uint8
{
	Exit   UMETA(DisplayName = "Exit"),
	Cancel UMETA(DisplayName = "Cancel")
};
