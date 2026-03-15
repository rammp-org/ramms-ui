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
	Home     UMETA(DisplayName = "Home"),
	Retract  UMETA(DisplayName = "Retract")
};

// ── MEBot / Chair ────────────────────────────────────────────────

/** MEBot driving modes */
UENUM(BlueprintType)
enum class ERammsMebotMode : uint8
{
	None        UMETA(DisplayName = "None"),
	SelfLevel   UMETA(DisplayName = "Self-Levelling"),
	CurbAscent  UMETA(DisplayName = "Curb Ascent"),
	CurbDescent UMETA(DisplayName = "Curb Descent")
};

// ── Task ─────────────────────────────────────────────────────────

/** Task action types */
UENUM(BlueprintType)
enum class ERammsTaskAction : uint8
{
	Exit   UMETA(DisplayName = "Exit"),
	Cancel UMETA(DisplayName = "Cancel")
};
