// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsSimLayout.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"

TArray<FName> URammsSimLayout::GetLayoutSlotNames_Implementation() const
{
	return { FName("SurfacePanel"), FName("Joystick"), FName("Status") };
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
	LayoutDisplayName = FText::FromString(TEXT("Sim"));
}
