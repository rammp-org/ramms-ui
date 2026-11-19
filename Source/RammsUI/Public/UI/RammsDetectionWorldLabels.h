// RammsDetectionWorldLabels.h
//
// Draws the 3D detection markers' labels as UI, the way the camera overlay draws its own.
//
// A world marker used to label itself with DrawDebugString: bare text, no plate, tinted the
// marker's colour. Over a pale wall it read; over a bright or busy part of the scene it did not,
// and which one you got depended on where the object happened to be in frame. The camera overlay
// solved this long ago -- URammsBoundingBoxOverlay puts a translucent black plate behind every
// label and draws the text in the box's colour on top -- so this does the same thing, with the
// same font, the same padding and the same plate, for the markers in the world.
//
// It has to be a widget. Text drawn from the marker is a debug draw: no background, no font
// control, and sized in pixels at a world position, so a plate drawn behind it in the world would
// scale with distance while the text did not. Labels belong in screen space with the rest of the
// UI, which is where this draws them.
//
// Separate from URammsWorldTapLayer on purpose. That one routes presses to anything implementing
// IRammsWorldTappable and knows nothing about detections; this one knows only about detections and
// takes no input at all. One full-screen widget doing both would be a worse name for either.
//
// Put it in the HUD above the tap layer and below the panels: it is SelfHitTestInvisible, so it
// never takes a press meant for the layer beneath it, and a panel that overlaps it still wins.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "RammsDetectionWorldLabels.generated.h"

UCLASS(meta = (DisplayName = "Ramms Detection World Labels"))
class RAMMSUI_API URammsDetectionWorldLabels : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	URammsDetectionWorldLabels();

	/** Off draws nothing at all, leaving the markers' boxes unlabelled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	bool bShowLabels = true;

	/**
	 * Text size, in Slate units, and the one knob that decides whether a label can be read.
	 *
	 * 22 rather than the camera overlay's 12 because this HUD lays out at roughly twice the
	 * viewport's pixel size: a 12-unit label renders about six pixels tall here, which is legible
	 * on a desk and not on a wheelchair. The 2D POIs settled on something close to this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection", meta = (ClampMin = "1"))
	int32 LabelFontSize = 22;

	/** Upper case, as the 2D POIs are: short class names read as tags rather than as prose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	bool bUppercaseLabels = true;

	/**
	 * The ink on the pill. Near-black rather than white because the pill takes the MARKER's
	 * colour, and those are bright -- amber and green read as a highlighter with dark ink on them
	 * and wash out white.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	FLinearColor LabelTextColour = FLinearColor(0.04f, 0.05f, 0.04f, 1.0f);

	/** Space around the text inside its pill: X each side, Y above and below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	FVector2D LabelPadding = FVector2D(11.0f, 3.0f);

	/**
	 * Pill opacity once a marker is fully stale. The pill fades rather than darkening: the ink
	 * stays the same, so an old label recedes without becoming unreadable -- and an old label is
	 * the one whose age you most want to read.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StaleLabelOpacity = 0.45f;

	/**
	 * Hide labels on markers further than this from the camera, in Unreal units. Zero is no limit.
	 *
	 * A label is the same size at any distance, so a far-off cluster turns into a wall of
	 * overlapping plates that hides the scene they are describing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection", meta = (ClampMin = "0.0"))
	float MaxLabelDistance = 0.0f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32				NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
					  const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
					  const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	/** Style font when the asset has one, otherwise the engine's at LabelFontSize. */
	void EnsureLabelFont() const;

	mutable FSlateFontInfo CachedLabelFont;
	mutable int32		   CachedFontSize = -1;

	/** Rounded to a capsule by the brush itself, so the radius follows the text's height. */
	FSlateBrush PlateBrush;
};
