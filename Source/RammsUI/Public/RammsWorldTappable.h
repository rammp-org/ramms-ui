// RammsWorldTappable.h
//
// What a thing in the 3D scene has to implement to take a tap from the operator.
//
// URammsWorldTapLayer traces the view under a press and calls this on the nearest thing it finds
// that implements it. Nothing else about that thing matters: it needs a collider the trace can see
// and this one function. An actor or a single component may implement it -- the component is asked
// first, so one actor can carry several separately tappable parts.
//
// An interface rather than a base class for the same reason IRammsDetectionPOI is one: the things
// worth tapping are already something else (a detection marker today, a door handle, a waypoint or
// a Blueprint prop tomorrow), and they should not have to share an ancestor to be tappable.
//
// It is also the ONE entry point for a tap, which is the part that matters as this moves. A marker
// also binds UPrimitiveComponent::OnClicked and OnInputTouchBegin, and both of those call this
// rather than duplicating what they do; a Blueprint that overrides this event therefore changes
// every route in. (Those native bindings depend on APlayerController click events, which the HUD
// swallows whenever a full-screen hit-testable widget sits over the view -- the tap layer exists
// because of exactly that. They are kept because they cost nothing and still work where input is
// routed normally, not because they can be relied on.)

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "UObject/Interface.h"
#include "RammsWorldTappable.generated.h"

UINTERFACE(Blueprintable, BlueprintType, meta = (DisplayName = "Ramms World Tappable"))
class RAMMSUI_API URammsWorldTappable : public UInterface
{
	GENERATED_BODY()
};

class RAMMSUI_API IRammsWorldTappable
{
	GENERATED_BODY()

public:
	/**
	 * The operator tapped this in the 3D view. Hit is where the ray met it, so an implementation
	 * that cares which face or which end was touched has it without tracing again.
	 *
	 * To stop taking taps, turn the collider off (SetCollisionEnabled(NoCollision)) rather than
	 * ignoring the call: the trace then skips it entirely and whatever is behind it becomes
	 * tappable, which is what "not tappable" has to mean for the thing in front.
	 *
	 * UHT generates an empty default, so implementing the interface and not the event is a marker
	 * that absorbs taps silently -- useless, but not a crash.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RAMMS|Interaction")
	void OnRammsWorldTapped(const FHitResult& Hit);
};
