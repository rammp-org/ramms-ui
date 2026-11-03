// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Touch fixes and tracing for UE's Linux platform layer (SDL3).
 *
 * UE 5.8's FLinuxApplication handles SDL's finger down, motion and up, but
 * not SDL_EVENT_FINGER_CANCELED. SDL cancels a touch when the compositor
 * takes it (Wayland) and when a finger goes down again before its up
 * arrived. Unhandled, the touch never ends in Slate: the widget it was on
 * keeps it captured, and its touch index is never freed (after ten of them
 * Slate takes no touches at all). An SDL event filter delivers each cancel
 * as a finger-up instead, which UE does handle. ramms.Touch.CancelAsUp 0
 * turns that off.
 *
 * ramms.Touch.Trace 1 logs each finger as SDL delivers it: when it went
 * down, how long until its first move, the longest gap between its events,
 * and how many arrived in bursts. That tells a touch the desktop held back
 * (delivered late, all at once) from a slow frame or the app's own handling.
 * 2 logs every event as well.
 *
 * Both are no-ops off Linux.
 */
namespace RammsTouchLinux
{
	void Startup();
	void Shutdown();
} // namespace RammsTouchLinux
