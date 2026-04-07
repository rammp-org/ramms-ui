// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RammsUITransitionTypes.generated.h"

/**
 * Transition animation style for layout switches.
 */
UENUM(BlueprintType)
enum class ERammsTransitionStyle : uint8
{
	/** Crossfade opacity only */
	Crossfade,
	/** Slide incoming/outgoing layouts left or right */
	Slide,
	/** Scale down outgoing, scale up incoming (with fade) */
	Scale,
	/** Slide with a slight scale for depth (recommended) */
	SlideAndScale
};

/**
 * Slide direction hint for layout transitions.
 */
UENUM(BlueprintType)
enum class ERammsSlideDirection : uint8
{
	/** Determine automatically from layout order (higher index = slide left) */
	Auto,
	/** New layout slides in from the right, old slides out left */
	Left,
	/** New layout slides in from the left, old slides out right */
	Right
};
