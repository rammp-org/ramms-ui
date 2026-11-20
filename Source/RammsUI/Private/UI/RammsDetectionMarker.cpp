// RammsDetectionMarker.cpp

#include "UI/RammsDetectionMarker.h"

#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "RammsBridgeSubsystem.h"
#include "UI/RammsBoundingBoxOverlay.h"

namespace
{
	// A press older than this is not the one that clicked: OnClicked follows its own press on release.
	// It bounds how long a press whose click never came (slid off the button) stays pending. A mouse
	// press is also checked against the cursor at click time (see PostObjectTapped); a touch press
	// can't be, so a keyboard or switch activation of this marker within the window after a cancelled
	// touch would report that touch's point.
	constexpr double MaxPressToClickSeconds = 10.0;
} // namespace

void URammsDetectionMarker::SetDetection(const FRammsBoundingBox& InDetection)
{
	Detection = InDetection;
	OnDetectionSet(Detection);
}

void URammsDetectionMarker::SetOverlay(URammsBoundingBoxOverlay* InOverlay)
{
	Overlay = InOverlay;
}

bool URammsDetectionMarker::IsCursorOver(FVector2D& OutCursor) const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}
	OutCursor = FSlateApplication::Get().GetCursorPos();
	return GetCachedGeometry().IsUnderLocation(OutCursor);
}

void URammsDetectionMarker::PostObjectTapped() const
{
	if (Detection.ID.IsNone())
	{
		return;
	}
	URammsBridgeSubsystem* Bridge = GEngine ? GEngine->GetEngineSubsystem<URammsBridgeSubsystem>() : nullptr;
	if (!Bridge)
	{
		return;
	}

	const URammsBoundingBoxOverlay* OverlayPtr = Overlay.Get();
	FVector2D						Normalized;
	bool							bHavePoint = false;
	FVector2D						CursorPos;
	const bool						bCursorOver = IsCursorOver(CursorPos);
	if (PressTime >= 0.0 && FPlatformTime::Seconds() - PressTime <= MaxPressToClickSeconds)
	{
		// A click is released over its button, so a mouse press whose cursor has left the marker is not
		// this click's press: a keyboard activation after a press that slid off, for example.
		bHavePoint = bPressWasTouch || bCursorOver;
		Normalized = PressNormalized;
	}
	PressTime = -1.0; // each press reports once
	if (!bHavePoint && bCursorOver && OverlayPtr)
	{
		// No preview came: the second click of a double-click, which Slate routes without one.
		bHavePoint = OverlayPtr->ScreenToPaneNormalized(CursorPos, OverlayPtr->MarkerPane, Normalized);
	}

	FVector2D Pixel;
	FIntPoint ImageSize;
	if (bHavePoint && OverlayPtr && OverlayPtr->NormalizedToImagePixel(Normalized, Pixel, ImageSize))
	{
		Bridge->PostObjectTappedOnImage(Detection.ID.ToString(), StreamID, Pixel, ImageSize);
		return;
	}
	Bridge->PostObjectTapped(Detection.ID.ToString(), StreamID);
}

FReply URammsDetectionMarker::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const bool	 bTouch = InMouseEvent.IsTouchEvent();
	const double Now = FPlatformTime::Seconds();
	const bool	 bOtherPointerPending = PressTime >= 0.0 && Now - PressTime <= MaxPressToClickSeconds
		&& PressPointer != InMouseEvent.GetPointerIndex();
	const URammsBoundingBoxOverlay* OverlayPtr = Overlay.Get();
	FVector2D						Normalized;
	if ((bTouch || InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && !bOtherPointerPending && OverlayPtr
		&& OverlayPtr->ScreenToPaneNormalized(InMouseEvent.GetScreenSpacePosition(), OverlayPtr->MarkerPane, Normalized))
	{
		// Normalized now, through the layout the user pressed in: the image can move or resize before
		// the finger lifts (a display-mode change, a new frame size), and the click comes on release.
		PressNormalized = Normalized;
		PressTime = Now;
		PressPointer = InMouseEvent.GetPointerIndex();
		bPressWasTouch = bTouch;
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}
