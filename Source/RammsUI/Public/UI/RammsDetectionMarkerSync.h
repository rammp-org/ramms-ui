// RammsDetectionMarkerSync.h
//
// One node for a layer of detection markers driven straight by the bridge: a full-screen POI layer,
// or anything else holding a marker per detected object outside a camera's image.
//
// It owns the markers' lifetime and nothing else. Each object keeps the widget it already had, by
// instance_id, so a marker is not rebuilt every push -- which would reset its animations and lose a
// press in progress -- and the markers of objects the push no longer lists are removed. The widgets
// place and draw themselves from OnDetectionSet, exactly as they do under URammsBoundingBoxOverlay.
//
// Every push is the full set, so an empty "objects" array removes every marker. That is the whole
// point: a layer that only ever adds shows objects that are no longer there, and nothing reports it.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/RammsDetectionMarker.h"
#include "RammsDetectionMarkerSync.generated.h"

class UPanelWidget;

UCLASS()
class RAMMSUI_API URammsDetectionMarkerSync : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Match Container's markers to one UpdateDetectedObjects push (RammsBridgeSubsystem's
	 * OnDetectedObjectsJson): reuse, create and remove so that there is exactly one marker per
	 * object the push lists.
	 *
	 * Only Container's Ramms Detection Marker children are touched, so the container can hold other
	 * widgets. A marker is added with AddChild, so it gets the container's own slot type and places
	 * itself from Detection.Centroid (normalized 0-1) in OnDetectionSet, which runs after the slot
	 * exists. Detection.Label is the object's perception_class, bSelected its selected_instance_id,
	 * and StreamID the camera that reported it, so PostObjectTapped names the right stream.
	 *
	 * @param AcceptedSources Only objects whose "source" (or the push's "stream_id" when they have
	 *        none) is listed, case-insensitively. EMPTY ACCEPTS EVERY SOURCE, including an empty
	 *        one -- the opposite of URammsCameraWidget::BridgeDetectionSources, which falls back to
	 *        its own stream, because a POI layer is not one camera's.
	 * @param ImageSize Pixel size that coordinates outside 0-1 are divided by. Normalized
	 *        coordinates ignore it; (0, 0) or less means 640x480.
	 * @return how many markers are shown, 0 included -- an empty push clears the layer -- or -1 when
	 *         nothing could be applied (no container, no marker class, or a payload that is not a
	 *         JSON object), leaving the markers as they were.
	 */
	UFUNCTION(BlueprintCallable, Category = "RAMMS|Detection", meta = (AutoCreateRefTerm = "AcceptedSources", AdvancedDisplay = "AcceptedSources,ImageSize"))
	static int32 SyncDetectionMarkersFromBridge(UPanelWidget* Container, TSubclassOf<URammsDetectionMarker> MarkerClass,
		const FString& DetectedObjectsJson, const TArray<FString>& AcceptedSources, FVector2D ImageSize);
};
