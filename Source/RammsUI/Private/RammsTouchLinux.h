// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Touch fixes and tracing for UE's Linux platform layer (SDL3).
 *
 * With a touchscreen (an SDL direct touch device):
 *
 * - Relative mouse mode stays off (ramms.Touch.NoRelativeMouse, read at
 *   startup). A touch on the game view captures the mouse and, when the
 *   input mode hides the cursor during capture, turns on high-precision
 *   mouse mode, which UE's Linux layer makes SDL's relative mouse mode. On
 *   X11, SDL3 then grabs the pointer and puts a passive touch grab on the
 *   window that it never accepts, so the X server holds touches back: short
 *   taps never arrive, and drags start only after the finger has been down
 *   for up to a second. This sets UE_NORELATIVEMOUSEMODE, which has UE skip
 *   relative mode (the same as -norelativemousemode). The cursor is still
 *   hidden during capture; it just isn't locked in place.
 *
 * - SDL text input stays off on the app's windows (ramms.Touch.NoTextInput,
 *   read at startup). UE turns text input on for every window it creates,
 *   which tells the desktop a text field has focus, so GNOME opens its
 *   on-screen keyboard. Keys still arrive with it off; typed characters
 *   (text boxes, the console) do not.
 *
 * On any Linux machine:
 *
 * - UE 5.8's FLinuxApplication handles SDL's finger down, motion and up, but
 *   not SDL_EVENT_FINGER_CANCELED. SDL cancels a touch when the compositor
 *   takes it (Wayland) and when a finger goes down again before its up
 *   arrived. Unhandled, the touch never ends in Slate: the widget it was on
 *   keeps it captured, and its touch index is never freed (after ten of them
 *   Slate takes no touches at all). An SDL event filter delivers each cancel
 *   as a finger-up instead, which UE does handle. ramms.Touch.CancelAsUp 0
 *   turns that off.
 *
 * - ramms.Touch.Trace 1 logs each finger as SDL delivers it: when it went
 *   down, how long until its first move, the longest gap between its events,
 *   and how many arrived in bursts. That tells a touch the desktop held back
 *   (delivered late, all at once) from a slow frame or the app's own handling.
 *   2 logs every event as well.
 *
 * All of it is a no-op off Linux.
 */
namespace RammsTouchLinux
{
	void Startup();
	void Shutdown();
} // namespace RammsTouchLinux
