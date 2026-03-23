// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsTestLayout.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

void URammsTestLayout::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	// Root: Canvas Panel (allows absolute/anchored positioning)
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// ── "Main" slot — fills most of the screen ──────────────────
	{
		UOverlay* MainSlot = WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(), TEXT("Main"));
		UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(MainSlot);
		if (PanelSlot)
		{
			// Fill the left 70% of the screen
			PanelSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.7f, 1.0f));
			PanelSlot->SetOffsets(FMargin(4.0f, 4.0f, 4.0f, 4.0f));
			PanelSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		}
		RegisterSlot(TEXT("Main"), MainSlot);

		// Add a debug label so we know the slot is visible
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("MainLabel"));
		Label->SetText(FText::FromString(TEXT("[Main Slot]")));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.3f)));
		MainSlot->AddChild(Label);
	}

	// ── "Sidebar" slot — right side panel ───────────────────────
	{
		UOverlay* SidebarSlot = WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(), TEXT("Sidebar"));
		UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(SidebarSlot);
		if (PanelSlot)
		{
			// Fill the right 30% of the screen
			PanelSlot->SetAnchors(FAnchors(0.7f, 0.0f, 1.0f, 1.0f));
			PanelSlot->SetOffsets(FMargin(4.0f, 4.0f, 4.0f, 4.0f));
			PanelSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		}
		RegisterSlot(TEXT("Sidebar"), SidebarSlot);

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("SidebarLabel"));
		Label->SetText(FText::FromString(TEXT("[Sidebar Slot]")));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.3f)));
		SidebarSlot->AddChild(Label);
	}

	LayoutDisplayName = FText::FromString(TEXT("Test Layout"));
}
