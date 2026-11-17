// RammsBridgeDetectionParse.h
//
// One place for the rules of an UpdateDetectedObjects push, shared by everything that turns one into
// normalized detections: URammsCameraWidget (a camera's own objects, over its image) and
// URammsDetectionMarkerSync (a full-screen POI layer). The rules are subtle enough -- which objects
// are the caller's, when coordinates are pixels, what counts as "no box" -- that a second copy would
// drift, and a layer disagreeing with the camera it sits over is exactly the bug nothing reports.

#pragma once

#include "CoreMinimal.h"
#include "RammsDetectionTypes.h"

class FJsonObject;

namespace RammsBridgeDetectionParse
{
	/** Read Count numbers from Field: an array, or an object keyed {"X", "Y", "Z", "W"}. */
	bool ReadNumbers(const FJsonObject& Object, const TCHAR* Field, int32 Count, double* Out);

	/**
	 * Whether these coordinates are already normalized, i.e. every component is within 0-1 (with a
	 * little slack for rounding). Anything larger is taken as pixels and divided by the image size.
	 */
	bool IsNormalized(const FVector4& Box, const FVector2D& Centroid);

	struct FResult
	{
		/** The kept objects, in normalized image coordinates, with Centroid as an override. */
		TArray<FRammsBoundingBox> Boxes;

		/** Each kept object's resolved source, parallel to Boxes: which camera reported it. */
		TArray<FString> Sources;

		/** The push's "stream_id": the source of an object that has none. */
		FString PushSource;

		/** The push's "selected_instance_id", empty for none. */
		FString SelectedInstanceId;
	};

	/**
	 * Parse one push. IsMine is asked, per object, whether its resolved source belongs to the caller.
	 * PixelDivisor scales the objects whose coordinates are not already normalized.
	 *
	 * @return how many objects were kept -- 0 is a valid answer and means "clear" -- or -1 when the
	 *         payload is not a JSON object at all, in which case Out is untouched.
	 */
	int32 Parse(const FString& DetectedObjectsJson, TFunctionRef<bool(const FString& Source)> IsMine,
		const FVector2D& PixelDivisor, FResult& Out);
} // namespace RammsBridgeDetectionParse
