// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RammsControlTypes.h"
#include "RammsSurfacePositionPad.generated.h"

/**
 * A pad for a pair of Position controls: it shows where the thing actually is
 * inside the region it can reach, and moves it by pointing.
 *
 * The joystick beside it commands a RATE, and a rate never has to describe
 * where anything is -- which is why it needs no region and no readback. A
 * position pair is the opposite: the value IS a place, and two sliders showing
 * one coordinate each cannot show a pose. Worse, for a mechanism like a 5-bar
 * the pair's two Ranges describe a box that is mostly out of reach, so a
 * faithful pad has to draw the region rather than the box.
 *
 * So the region comes from the surface (`FRammsControlAxis::RegionOutline`,
 * published by whichever contributor knows the mechanism) and is drawn as a
 * polygon. Inside it: a dot for the live readback, and, when it differs, a ring
 * for the target being held -- the gap between them is the mechanism still
 * moving, which is exactly what a pad is for.
 *
 * With no outline published it falls back to the rectangle the two Ranges
 * describe, which is the honest picture for a pair that really is rectangular.
 */
UCLASS(meta = (DisplayName = "Ramms Surface Position Pad"))
class RAMMSUI_API URammsSurfacePositionPad : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Object implementing IRammsControlSink; auto-found when null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	TObjectPtr<UObject> TargetSink;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	bool bAutoFindSink = true;

	/** The horizontal control (the vertical one's `PairedAxis`). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	FName ControlIdX;

	/** The vertical control -- the one carrying the region. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	FName ControlIdY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	ERammsControlSource Source = ERammsControlSource::Touch;

	/** Value ranges, used for the widget-space mapping and as the fallback
	 *  shape when no region is published. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	FVector2D RangeX = FVector2D(-1.0, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	FVector2D RangeY = FVector2D(-1.0, 1.0);

	/** Region boundary in (horizontal, vertical) values; empty = the rectangle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	TArray<FVector2D> Region;

	/** Inset from the widget edge, in pixels, so the outline is not clipped.
	 *  Not `Padding`: UUserWidget already has one, and shadowing it is an error. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (ClampMin = "0.0"))
	float EdgeInset = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor RegionColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor LiveColor = FLinearColor(0.20f, 0.85f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor TargetColor = FLinearColor(1.0f, 0.75f, 0.15f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (ClampMin = "1.0"))
	float DotRadius = 4.0f;

	/** How close the live readback has to be to the target, in the pair's own
	 *  units, before the pose counts as arrived and the ring stops being drawn.
	 *  A servo settles near its target rather than exactly on it, so an exact
	 *  comparison would leave the ring up forever. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (ClampMin = "0.0"))
	float ArrivedTolerance = 0.25f;

	/** Point the pair at a sink and a pair of ids. */
	UFUNCTION(BlueprintCallable, Category = "Control Surface")
	void SetTarget(UObject* Sink, FName IdX, FName IdY);

	/** The shape to draw and the mapping to draw it with. */
	UFUNCTION(BlueprintCallable, Category = "Control Surface")
	void SetRegion(FVector2D InRangeX, FVector2D InRangeY, const TArray<FVector2D>& InRegion);

	/** Command a value pair as though it had been pointed at (tests, scripts). */
	UFUNCTION(BlueprintCallable, Category = "Control Surface")
	bool CommandValue(FVector2D Value);

	/** Where the pad currently reads the pair as being (live readback). */
	UFUNCTION(BlueprintPure, Category = "Control Surface")
	FVector2D GetLiveValue() const;

	/** The pair's held target; false when nothing is held. */
	UFUNCTION(BlueprintPure, Category = "Control Surface")
	bool GetTargetValue(FVector2D& OutValue) const;

	/** Value <-> widget-space, exposed because a test that cannot convert can
	 *  only assert that something was drawn, not that it was drawn in the
	 *  right place. */
	UFUNCTION(BlueprintPure, Category = "Control Surface")
	FVector2D ValueToLocal(FVector2D Value, FVector2D WidgetSize) const;

	UFUNCTION(BlueprintPure, Category = "Control Surface")
	FVector2D LocalToValue(FVector2D Local, FVector2D WidgetSize) const;

	URammsSurfacePositionPad(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InEvent) override;

private:
	UObject* ResolveSink() const;

	/** Command from a pointer at LocalPosition. */
	bool CommandAt(const FGeometry& Geometry, const FVector2D& ScreenPosition);

	/** The shape actually drawn: the published region, or the range rectangle. */
	TArray<FVector2D> ShapeToDraw() const;

	bool bDragging = false;
};
