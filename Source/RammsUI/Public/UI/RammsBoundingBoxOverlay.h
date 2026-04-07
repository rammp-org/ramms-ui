// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "RammsDetectionTypes.h"
#include "RammsBoundingBoxOverlay.generated.h"

/**
 * How to assign colors to boxes that don't have a custom color.
 */
UENUM(BlueprintType)
enum class ERammsBBoxColorMode : uint8
{
	/** Hash the label text to pick a consistent color per class */
	LabelHash,
	/** Use a single default color for all boxes */
	Uniform,
	/** Cycle through a palette by box index */
	Palette
};

/**
 * Standalone overlay widget that draws 2D bounding boxes via NativePaint.
 *
 * Place this on top of any image (e.g., inside an Overlay panel, or hosted
 * by URammsCameraWidget). Feed it detections via SetDetections() or
 * auto-subscribe to the subsystem event bus.
 *
 * All box coordinates are normalized 0-1 relative to the overlay area.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Bounding Box Overlay"))
class RAMMSUI_API URammsBoundingBoxOverlay : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	// ── Configuration ───────────────────────────────────────────

	/** Line thickness for box outlines (in pixels) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounding Box|Appearance",
		meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float LineThickness = 2.0f;

	/** Show label text above each box */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounding Box|Appearance")
	bool bShowLabels = true;

	/** Show confidence value next to the label */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounding Box|Appearance",
		meta = (EditCondition = "bShowLabels"))
	bool bShowConfidence = true;

	/** Show centroid dot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounding Box|Appearance")
	bool bShowCentroid = false;

	/** Label font size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounding Box|Appearance",
		meta = (ClampMin = "6", ClampMax = "32"))
	int32 LabelFontSize = 12;

	/** Color assignment mode for boxes without a custom color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounding Box|Appearance")
	ERammsBBoxColorMode ColorMode = ERammsBBoxColorMode::LabelHash;

	/** Default color when ColorMode is Uniform, or fallback */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounding Box|Appearance")
	FLinearColor DefaultBoxColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);

	/** Color palette for Palette mode (cycles through these) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounding Box|Appearance",
		meta = (EditCondition = "ColorMode == ERammsBBoxColorMode::Palette"))
	TArray<FLinearColor> ColorPalette;

	/** Label text background opacity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounding Box|Appearance",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LabelBackgroundOpacity = 0.7f;

	/** Centroid dot radius in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounding Box|Appearance",
		meta = (ClampMin = "1.0", ClampMax = "10.0"))
	float CentroidRadius = 3.0f;

	// ── Subsystem Auto-Subscribe ────────────────────────────────

	/** If set, auto-subscribe to subsystem detections matching this source tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounding Box|Source")
	FName SourceTagFilter;

	/** Whether to auto-subscribe to subsystem detection events */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounding Box|Source")
	bool bAutoSubscribe = false;

	/**
	 * Detection display lifetime in seconds. When > 0, detections auto-clear
	 * after this duration since the last SetDetections call. 0 = no auto-clear.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounding Box|Source",
		meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float DetectionLifetime = 0.0f;

	// ── API ─────────────────────────────────────────────────────

	/**
	 * Set the bounding boxes to display. Replaces any existing boxes.
	 * @param InBoxes - Array of bounding boxes in normalized 0-1 coordinates
	 */
	UFUNCTION(BlueprintCallable, Category = "Bounding Box")
	void SetDetections(const TArray<FRammsBoundingBox>& InBoxes);

	/** Clear all displayed bounding boxes */
	UFUNCTION(BlueprintCallable, Category = "Bounding Box")
	void ClearDetections();

	/** Get the current bounding boxes */
	UFUNCTION(BlueprintPure, Category = "Bounding Box")
	const TArray<FRammsBoundingBox>& GetDetections() const { return Boxes; }

	/** Get the number of displayed boxes */
	UFUNCTION(BlueprintPure, Category = "Bounding Box")
	int32 GetBoxCount() const { return Boxes.Num(); }

	// ── Delegates ───────────────────────────────────────────────

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBoxClicked, const FRammsBoundingBox&, Box);

	/** Fired when a bounding box is clicked (future: hit-test support) */
	UPROPERTY(BlueprintAssignable, Category = "Bounding Box|Events")
	FOnBoxClicked OnBoxClicked;

protected:
	virtual void BuildWidgetTree() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	/** Current bounding boxes */
	UPROPERTY(Transient)
	TArray<FRammsBoundingBox> Boxes;

	/** Time (game seconds) when detections were last set */
	double LastDetectionTime = 0.0;

	/** Whether subscribed to subsystem */
	bool bSubscribed = false;

	/** Subsystem detection handler */
	UFUNCTION()
	void HandleDetectionsReceived(FName SourceTag, const TArray<FRammsBoundingBox>& InBoxes);

	/** Subsystem clear handler */
	UFUNCTION()
	void HandleDetectionsCleared(FName SourceTag);

	/** Subscribe/unsubscribe from subsystem */
	void SubscribeToSubsystem();
	void UnsubscribeFromSubsystem();

	/** Resolve the display color for a box */
	FLinearColor ResolveBoxColor(const FRammsBoundingBox& Box, int32 Index) const;

	/** Generate a color from a label string hash */
	static FLinearColor ColorFromLabelHash(const FText& Label);

	/** Draw a single axis-aligned rect box */
	void DrawRectBox(const FRammsBoundingBox& Box, int32 Index,
		const FGeometry& Geom, FSlateWindowElementList& OutDrawElements,
		int32 LayerId) const;

	/** Draw a single rotated rect box */
	void DrawRotatedRectBox(const FRammsBoundingBox& Box, int32 Index,
		const FGeometry& Geom, FSlateWindowElementList& OutDrawElements,
		int32 LayerId) const;

	/** Draw a polygon */
	void DrawPolygonBox(const FRammsBoundingBox& Box, int32 Index,
		const FGeometry& Geom, FSlateWindowElementList& OutDrawElements,
		int32 LayerId) const;

	/** Draw the label text above a box */
	void DrawLabel(const FRammsBoundingBox& Box, const FLinearColor& BoxColor,
		const FVector2D& LabelPos, const FGeometry& Geom,
		FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	/** Draw a centroid dot */
	void DrawCentroid(const FVector2D& Center, const FLinearColor& BoxColor,
		const FGeometry& Geom, FSlateWindowElementList& OutDrawElements,
		int32 LayerId) const;

	/** Cached font for label drawing */
	mutable FSlateFontInfo CachedLabelFont;
	mutable int32		   CachedFontSize = 0;

	/** Ensure CachedLabelFont is up to date */
	void EnsureLabelFont() const;
};
