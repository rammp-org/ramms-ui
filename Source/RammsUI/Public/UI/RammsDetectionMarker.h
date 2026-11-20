// RammsDetectionMarker.h
//
// A widget placed on one detection by URammsBoundingBoxOverlay (MarkerWidgetClass): for example an
// outline the user taps, with the object's label. Make a Blueprint subclass, lay it out, and implement
// OnDetectionSet to show the detection; call PostObjectTapped from its button.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RammsDetectionTypes.h"
#include "RammsDetectionMarker.generated.h"

class URammsBoundingBoxOverlay;

/** Where the overlay puts a marker on its detection. */
UENUM(BlueprintType)
enum class ERammsDetectionMarkerPlacement : uint8
{
	/**
	 * The marker fills the detection's box (the box around its corners when rotated, or around its points
	 * for a polygon). A detection with no box (only a centroid) gets the marker at its own size, centred.
	 */
	Box,
	/** The marker keeps its own size and is centred on the detection's centre. */
	Centroid,
};

UCLASS(Abstract, Blueprintable)
class RAMMSUI_API URammsDetectionMarker : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The detection this marker shows, in normalized image coordinates. */
	UPROPERTY(BlueprintReadOnly, Category = "Detection")
	FRammsBoundingBox Detection;

	/** The stream of the camera this marker is on (e.g. "camera/scene/color"); sent with a tap. */
	UPROPERTY(BlueprintReadOnly, Category = "Detection")
	FString StreamID;

	/** This detection is the selected one (the bridge's selected_instance_id). Set before OnDetectionSet. */
	UPROPERTY(BlueprintReadOnly, Category = "Detection")
	bool bSelected = false;

	/** Set by the overlay whenever this marker is given a detection, or its detection is updated. */
	void SetDetection(const FRammsBoundingBox& InDetection);

	/** Called on every SetDetection: show the detection (its Label, ID, Confidence ...). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Detection")
	void OnDetectionSet(const FRammsBoundingBox& InDetection);

	/**
	 * Tell the robot this object was tapped, with Detection.ID and StreamID. When the tap was a press on
	 * this marker, the event also carries where on the camera's frame it landed (in pixels, through
	 * RammsBridgeSubsystem::PostObjectTappedOnImage); otherwise it goes out without (PostObjectTapped).
	 * Call it from the marker's button's OnClicked: a button gives no position, so the press is
	 * recorded on the way down to it (NativeOnPreviewMouseButtonDown) and used here. A mouse click
	 * with no recorded press (the second click of a double-click, which Slate delivers without a
	 * preview) uses the cursor, which a click is released over.
	 */
	UFUNCTION(BlueprintCallable, Category = "Detection")
	void PostObjectTapped() const;

	/** The overlay that placed this marker, which maps a press on it to the camera's pixels (set by the overlay). */
	void SetOverlay(URammsBoundingBoxOverlay* InOverlay);

protected:
	/**
	 * Records where a press on this marker landed, as a point on the image, while the layout is the
	 * one the user pressed. Slate tunnels the preview to every widget above the one pressed, touch
	 * included, before the button sees it; this returns unhandled, so the button still gets the
	 * press and its OnClicked still fires. Only a touch or the left mouse button, the presses a
	 * button clicks for, and the first pointer down until its click (a second finger on the same
	 * marker does not move the point).
	 */
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	/** Whether the mouse cursor is over this marker now. */
	bool IsCursorOver(FVector2D& OutCursor) const;

	TWeakObjectPtr<URammsBoundingBoxOverlay> Overlay;

	/**
	 * The pending press on this marker: where on the image (normalized, unclamped), from which pointer,
	 * whether it was a touch, and when. Used once by PostObjectTapped. Mutable because PostObjectTapped
	 * is const for its existing Blueprint callers. The press is where the finger went down; OnClicked
	 * fires when it lifts, on the same button.
	 */
	mutable FVector2D PressNormalized = FVector2D::ZeroVector;
	mutable double	  PressTime = -1.0;
	mutable uint32	  PressPointer = 0;
	mutable bool	  bPressWasTouch = false;
};
