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

	/** Tell the robot this object was tapped: RammsBridgeSubsystem::PostObjectTapped with Detection.ID and StreamID. */
	UFUNCTION(BlueprintCallable, Category = "Detection")
	void PostObjectTapped() const;
};
