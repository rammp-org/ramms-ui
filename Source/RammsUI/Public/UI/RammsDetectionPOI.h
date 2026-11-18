// RammsDetectionPOI.h
//
// What a POI widget has to implement to be placed by URammsDetectionPOILayer.
//
// An interface, not a base class, deliberately: RAMMS_POI is already built around
// FRammsBridgeDetectedObject and has its own variables, functions and designer tree. Reparenting it
// would hand it a different struct and risk colliding with names it already uses, so the layer asks
// for one function instead and leaves the widget's class, type and logic exactly as they are.

#pragma once

#include "CoreMinimal.h"
#include "RammsBridgeSubsystem.h"
#include "UObject/Interface.h"
#include "RammsDetectionPOI.generated.h"

UINTERFACE(Blueprintable, BlueprintType, meta = (DisplayName = "Ramms Detection POI"))
class RAMMSUI_API URammsDetectionPOI : public UInterface
{
	GENERATED_BODY()
};

class RAMMSUI_API IRammsDetectionPOI
{
	GENERATED_BODY()

public:
	/**
	 * Show this object. Called on every push that lists it -- not only the first -- so a POI that
	 * moves keeps up, and called after the widget is in its slot, so the implementation can place
	 * itself through Slot As Canvas Slot.
	 *
	 * Coordinates arrive NORMALIZED 0-1, whichever way the bridge sent them. A producer may send
	 * pixels -- FRammsBridgeDetectedObject allows it -- and URammsDetectionPOILayer divides those
	 * by the frame size before you see them, so an implementation multiplies by its own size and
	 * nothing else. Do not divide again.
	 *
	 * (This used to say the layer passed coordinates through untouched. It did, and a push of
	 * pixel coordinates put every POI hundreds of screens off while normalized ones beside them
	 * looked fine.)
	 *
	 * UHT generates an empty default, so a widget that adds the interface and does not implement the
	 * event simply shows nothing -- the layer still owns its lifetime, and never calls into nothing.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RAMMS|Detection")
	void SetDetectedObject(const FRammsBridgeDetectedObject& DetectedObject);
};
