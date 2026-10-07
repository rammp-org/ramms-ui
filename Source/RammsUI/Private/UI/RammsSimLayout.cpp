// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsSimLayout.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"

TArray<FName> URammsSimLayout::GetLayoutSlotNames_Implementation() const
{
	return { FName("SurfacePanel"), FName("Joystick"), FName("Status"), FName("CameraCapturePanel") };
}

void URammsSimLayout::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	// Right column: the control-surface panel, full height.
	{
		UOverlay* SlotWidget = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SurfacePanel"));
		if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(SlotWidget))
		{
			CanvasSlot->SetAnchors(FAnchors(1.0f - PanelColumnFraction, 0.0f, 1.0f, 1.0f));
			CanvasSlot->SetOffsets(FMargin(8.0f, 8.0f, 8.0f, 8.0f));
			CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		}
		RegisterSlot(TEXT("SurfacePanel"), SlotWidget);
	}
	// Bottom-left: the drive joystick.
	{
		UOverlay* SlotWidget = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Joystick"));
		if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(SlotWidget))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
			CanvasSlot->SetAlignment(FVector2D(0.0f, 1.0f));
			CanvasSlot->SetPosition(FVector2D(24.0f, -24.0f));
			CanvasSlot->SetSize(JoystickAreaSize);
		}
		RegisterSlot(TEXT("Joystick"), SlotWidget);
	}
	// Top-left: status.
	{
		UOverlay* SlotWidget = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Status"));
		if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(SlotWidget))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
			CanvasSlot->SetPosition(FVector2D(16.0f, 16.0f));
			CanvasSlot->SetAutoSize(true);
		}
		RegisterSlot(TEXT("Status"), SlotWidget);
	}
	// Left column, below status: the camera capture feeds. Left rather than right
	// so it does not fight the control-surface panel for the same column, and
	// anchored top-to-bottom so a long list of cameras scrolls inside its own
	// panel rather than growing off the screen.
	{
		UOverlay* SlotWidget = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CameraCapturePanel"));
		if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(SlotWidget))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 1.0f));
			CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
			// Width and bottom margin in ONE call. With these anchors the slot
			// is stretched vertically but not horizontally, so Offsets.Right is
			// the width and Offsets.Bottom is the gap above the bottom edge --
			// and SetSize writes both of them, which is how a following
			// SetSize(width, 0) reset the 200 px margin to zero and ran the
			// panel down into the joystick.
			CanvasSlot->SetOffsets(FMargin(16.0f, 64.0f, CameraPanelWidth, 200.0f));
		}
		RegisterSlot(TEXT("CameraCapturePanel"), SlotWidget);
	}
	LayoutDisplayName = FText::FromString(TEXT("Sim"));
}
