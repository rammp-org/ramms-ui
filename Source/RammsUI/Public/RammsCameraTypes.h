// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RammsCameraTypes.generated.h"

/**
 * Display modes for camera widget.
 * Shared header so both URammsUIStyle and URammsCameraWidget can reference the enum.
 */
UENUM(BlueprintType)
enum class ERammsCameraDisplayMode : uint8
{
	/** Full screen (fills entire viewport) */
	Fullscreen,

	/** Large window (centered, takes most of screen) */
	Windowed,

	/** Corner widget (small, anchored to corner) */
	Corner,

	/** Widget mode — size determined by parent layout, no viewport % sizing */
	Widget
};

/**
 * Which data channel to visualize
 */
UENUM(BlueprintType)
enum class ERammsCameraViewMode : uint8
{
	/** Show RGB color image */
	RGB UMETA(DisplayName = "RGB Color"),

	/** Show data stream through visualization material */
	Data UMETA(DisplayName = "Data (Visualized)"),

	/** Side-by-side RGB + Data */
	SideBySide UMETA(DisplayName = "Side-by-Side"),

	/** RGB with data overlay (alpha blended) */
	Overlay UMETA(DisplayName = "RGB + Data Overlay")
};
