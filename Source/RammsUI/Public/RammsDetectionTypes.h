// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RammsDetectionTypes.generated.h"

/**
 * Shape type for a detection annotation.
 */
UENUM(BlueprintType)
enum class ERammsDetectionShape : uint8
{
	/** Axis-aligned bounding box */
	Rect,
	/** Rotated bounding box (uses AngleDegrees) */
	RotatedRect,
	/** Arbitrary polygon (uses PolygonPoints) */
	Polygon
};

/**
 * A single 2D detection / bounding box annotation.
 * All spatial values use normalized image coordinates (0-1).
 */
USTRUCT(BlueprintType)
struct RAMMSUI_API FRammsBoundingBox
{
	GENERATED_BODY()

	// ── Identity ────────────────────────────────────────────────

	/** Unique ID for tracking across frames (e.g., tracker output) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	FName ID;

	/** Class / category label (e.g., "Person", "Obstacle") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	FText Label;

	/** Detection confidence score (0-1). Negative = not applicable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection",
		meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float Confidence = -1.0f;

	// ── Geometry ────────────────────────────────────────────────

	/** Shape type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	ERammsDetectionShape Shape = ERammsDetectionShape::Rect;

	/** Top-left corner in normalized image coords (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	FVector2D Position = FVector2D::ZeroVector;

	/** Width and height in normalized image coords (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	FVector2D Size = FVector2D::ZeroVector;

	/** Rotation angle in degrees (only used for RotatedRect) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection",
		meta = (EditCondition = "Shape == ERammsDetectionShape::RotatedRect"))
	float AngleDegrees = 0.0f;

	/**
	 * Polygon vertices in normalized image coords (only used for Polygon shape).
	 * Points are connected in order; last point connects back to first.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection",
		meta = (EditCondition = "Shape == ERammsDetectionShape::Polygon"))
	TArray<FVector2D> PolygonPoints;

	/** Centroid override in normalized coords. Only used when bHasCentroidOverride is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection",
		meta = (EditCondition = "bHasCentroidOverride"))
	FVector2D Centroid = FVector2D::ZeroVector;

	/** Whether Centroid contains an explicit override value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	bool bHasCentroidOverride = false;

	// ── Appearance ──────────────────────────────────────────────

	/** Per-box color override. Alpha = 0 means use auto-color (from label hash or default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	FLinearColor Color = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

	// ── Helpers ─────────────────────────────────────────────────

	/** Get effective center point (uses Centroid if override set, otherwise rect center) */
	FVector2D GetCenter() const
	{
		if (bHasCentroidOverride)
		{
			return Centroid;
		}
		return Position + Size * 0.5f;
	}

	/** Get the bottom-right corner */
	FVector2D GetBottomRight() const
	{
		return Position + Size;
	}

	/** Whether this box has a valid confidence value */
	bool HasConfidence() const
	{
		return Confidence >= 0.0f;
	}

	/** Whether this box has a custom color (vs auto-color) */
	bool HasCustomColor() const
	{
		return Color.A > 0.0f;
	}
};

/**
 * A frame of detections from a single source.
 */
USTRUCT(BlueprintType)
struct RAMMSUI_API FRammsDetectionFrame
{
	GENERATED_BODY()

	/** Source identifier (e.g., camera name, detector name) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	FName SourceTag;

	/** All bounding boxes in this frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	TArray<FRammsBoundingBox> Boxes;

	/** Timestamp (seconds since game start, or -1 for "now") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	double Timestamp = -1.0;
};
