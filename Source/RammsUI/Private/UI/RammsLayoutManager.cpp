// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsLayoutManager.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"

URammsLayoutManager::URammsLayoutManager()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void URammsLayoutManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Clean up invalid widgets
	ManagedWidgets.RemoveAll([](const FRammsWidgetLayout& Layout) {
		return !Layout.Widget || !IsValid(Layout.Widget);
	});
}

void URammsLayoutManager::AddWidget(URammsBaseWidget* Widget, int32 ZOrder)
{
	if (!Widget)
		return;

	// Check if already managed
	for (FRammsWidgetLayout& Layout : ManagedWidgets)
	{
		if (Layout.Widget == Widget)
		{
			Layout.ZOrder = ZOrder;
			UpdateZOrder();
			return;
		}
	}

	// Add new widget
	FRammsWidgetLayout NewLayout;
	NewLayout.Widget = Widget;
	NewLayout.ZOrder = ZOrder;
	NewLayout.TargetPosition = FVector2D(0.5f, 0.5f);
	NewLayout.TargetSize = FVector2D(0.5f, 0.5f);
	NewLayout.bVisible = true;

	ManagedWidgets.Add(NewLayout);
	UpdateZOrder();

	// Apply current layout
	ApplyLayout(false);
}

void URammsLayoutManager::RemoveWidget(URammsBaseWidget* Widget)
{
	ManagedWidgets.RemoveAll([Widget](const FRammsWidgetLayout& Layout) {
		return Layout.Widget == Widget;
	});
}

void URammsLayoutManager::ClearWidgets()
{
	ManagedWidgets.Empty();
}

void URammsLayoutManager::TransitionTo(ERammsLayoutPreset Preset, bool bAnimated)
{
	CurrentPreset = Preset;
	ApplyLayout(bAnimated);
}

void URammsLayoutManager::SetWidgetPosition(URammsBaseWidget* Widget, FVector2D Position, FVector2D Size, bool bAnimated)
{
	for (FRammsWidgetLayout& Layout : ManagedWidgets)
	{
		if (Layout.Widget == Widget)
		{
			Layout.TargetPosition = Position;
			Layout.TargetSize = Size;

			// Get viewport size
			FVector2D ViewportSize(1920, 1080);
			if (GEngine && GEngine->GameViewport)
			{
				GEngine->GameViewport->GetViewportSize(ViewportSize);
			}

			FVector2D PixelPosition = Position * ViewportSize;
			FVector2D PixelSize = Size * ViewportSize;

			// Detect if widget is inside a Canvas Panel or added to viewport
			UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
			if (CanvasSlot)
			{
				CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
				CanvasSlot->SetPosition(PixelPosition);
				CanvasSlot->SetSize(PixelSize);
			}
			else
			{
				float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(Widget);
				if (ViewportScale <= 0.0f) ViewportScale = 1.0f;

				Widget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
				Widget->SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
				Widget->SetDesiredSizeInViewport(PixelSize / ViewportScale);
				Widget->SetPositionInViewport(PixelPosition, true);
			}

			if (Layout.bVisible)
			{
				Widget->SetVisibility(ESlateVisibility::Visible);
			}

			if (bAnimated)
			{
				Widget->ScaleIn(TransitionDuration);
			}
			return;
		}
	}
}

void URammsLayoutManager::BringToFront(URammsBaseWidget* Widget)
{
	int32 MaxZOrder = 0;
	for (const FRammsWidgetLayout& Layout : ManagedWidgets)
	{
		if (Layout.ZOrder > MaxZOrder)
		{
			MaxZOrder = Layout.ZOrder;
		}
	}

	for (FRammsWidgetLayout& Layout : ManagedWidgets)
	{
		if (Layout.Widget == Widget)
		{
			Layout.ZOrder = MaxZOrder + 1;
			UpdateZOrder();
			return;
		}
	}
}

void URammsLayoutManager::SendToBack(URammsBaseWidget* Widget)
{
	int32 MinZOrder = 0;
	for (const FRammsWidgetLayout& Layout : ManagedWidgets)
	{
		if (Layout.ZOrder < MinZOrder)
		{
			MinZOrder = Layout.ZOrder;
		}
	}

	for (FRammsWidgetLayout& Layout : ManagedWidgets)
	{
		if (Layout.Widget == Widget)
		{
			Layout.ZOrder = MinZOrder - 1;
			UpdateZOrder();
			return;
		}
	}
}

void URammsLayoutManager::ApplyLayout(bool bAnimated)
{
	switch (CurrentPreset)
	{
	case ERammsLayoutPreset::SingleFullscreen:
		ApplyFullscreenLayout(bAnimated);
		break;

	case ERammsLayoutPreset::Grid:
		ApplyGridLayout(bAnimated);
		break;

	case ERammsLayoutPreset::PictureInPicture:
		ApplyPIPLayout(bAnimated);
		break;

	case ERammsLayoutPreset::SideBySide:
		ApplySideBySideLayout(bAnimated);
		break;

	case ERammsLayoutPreset::Custom:
		// Custom layout - positions set manually via SetWidgetPosition
		break;
	}
}

void URammsLayoutManager::ApplyFullscreenLayout(bool bAnimated)
{
	if (ManagedWidgets.Num() == 0)
		return;

	// Show first widget fullscreen, hide others
	for (int32 i = 0; i < ManagedWidgets.Num(); ++i)
	{
		FRammsWidgetLayout& Layout = ManagedWidgets[i];
		
		if (i == 0)
		{
			Layout.TargetPosition = FVector2D::ZeroVector;
			Layout.TargetSize = FVector2D(1.0f, 1.0f);
			Layout.bVisible = true;
			SetWidgetPosition(Layout.Widget, Layout.TargetPosition, Layout.TargetSize, bAnimated);
		}
		else
		{
			Layout.bVisible = false;
			Layout.Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void URammsLayoutManager::ApplyGridLayout(bool bAnimated)
{
	int32 Count = ManagedWidgets.Num();
	if (Count == 0)
		return;

	float CellWidth = 1.0f / GridColumns;
	float CellHeight = 1.0f / GridRows;
	float SpacingNorm = WidgetSpacing / 1920.0f; // Normalize to viewport

	int32 Index = 0;
	for (int32 Row = 0; Row < GridRows && Index < Count; ++Row)
	{
		for (int32 Col = 0; Col < GridColumns && Index < Count; ++Col, ++Index)
		{
			FRammsWidgetLayout& Layout = ManagedWidgets[Index];
			
			Layout.TargetPosition = FVector2D(Col * CellWidth + SpacingNorm, Row * CellHeight + SpacingNorm);
			Layout.TargetSize = FVector2D(CellWidth - 2 * SpacingNorm, CellHeight - 2 * SpacingNorm);
			Layout.bVisible = true;

			SetWidgetPosition(Layout.Widget, Layout.TargetPosition, Layout.TargetSize, bAnimated);
		}
	}

	// Hide excess widgets
	for (int32 i = Index; i < Count; ++i)
	{
		ManagedWidgets[i].bVisible = false;
		ManagedWidgets[i].Widget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URammsLayoutManager::ApplyPIPLayout(bool bAnimated)
{
	if (ManagedWidgets.Num() == 0)
		return;

	// First widget takes 80% of screen
	FRammsWidgetLayout& MainLayout = ManagedWidgets[0];
	MainLayout.TargetPosition = FVector2D(0.1f, 0.1f);
	MainLayout.TargetSize = FVector2D(0.8f, 0.8f);
	MainLayout.bVisible = true;
	SetWidgetPosition(MainLayout.Widget, MainLayout.TargetPosition, MainLayout.TargetSize, bAnimated);

	// Additional widgets go in corners (20% size)
	const FVector2D CornerPositions[] = {
		FVector2D(0.75f, 0.05f),  // Top-right
		FVector2D(0.75f, 0.75f),  // Bottom-right
		FVector2D(0.05f, 0.75f),  // Bottom-left
		FVector2D(0.05f, 0.05f)   // Top-left
	};

	for (int32 i = 1; i < ManagedWidgets.Num() && i < 5; ++i)
	{
		FRammsWidgetLayout& Layout = ManagedWidgets[i];
		Layout.TargetPosition = CornerPositions[i - 1];
		Layout.TargetSize = FVector2D(0.2f, 0.2f);
		Layout.bVisible = true;
		SetWidgetPosition(Layout.Widget, Layout.TargetPosition, Layout.TargetSize, bAnimated);
	}

	// Hide excess widgets
	for (int32 i = 5; i < ManagedWidgets.Num(); ++i)
	{
		ManagedWidgets[i].bVisible = false;
		ManagedWidgets[i].Widget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URammsLayoutManager::ApplySideBySideLayout(bool bAnimated)
{
	int32 Count = FMath::Min(ManagedWidgets.Num(), 2);
	if (Count == 0)
		return;

	float Width = 1.0f / Count;
	float SpacingNorm = WidgetSpacing / 1920.0f;

	for (int32 i = 0; i < Count; ++i)
	{
		FRammsWidgetLayout& Layout = ManagedWidgets[i];
		Layout.TargetPosition = FVector2D(i * Width + SpacingNorm, SpacingNorm);
		Layout.TargetSize = FVector2D(Width - 2 * SpacingNorm, 1.0f - 2 * SpacingNorm);
		Layout.bVisible = true;
		SetWidgetPosition(Layout.Widget, Layout.TargetPosition, Layout.TargetSize, bAnimated);
	}

	// Hide excess widgets
	for (int32 i = Count; i < ManagedWidgets.Num(); ++i)
	{
		ManagedWidgets[i].bVisible = false;
		ManagedWidgets[i].Widget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URammsLayoutManager::UpdateZOrder()
{
	// Sort by Z-order
	ManagedWidgets.Sort([](const FRammsWidgetLayout& A, const FRammsWidgetLayout& B) {
		return A.ZOrder < B.ZOrder;
	});

	FVector2D ViewportSize(1920, 1080);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	// Re-add widgets with correct z-order and re-apply positions
	for (FRammsWidgetLayout& Layout : ManagedWidgets)
	{
		if (!Layout.Widget || !Layout.bVisible)
			continue;

		// For widgets in a Canvas Panel, z-order is handled by child order (no re-add needed)
		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Layout.Widget->Slot);
		if (CanvasSlot)
		{
			// Canvas Panel children are drawn in order; slot manipulation not needed here
			continue;
		}

		// For viewport widgets, re-add with z-order
		if (Layout.Widget->IsInViewport())
		{
			Layout.Widget->RemoveFromParent();
			Layout.Widget->AddToViewport(Layout.ZOrder);

			// Re-apply stored position/size
			float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(Layout.Widget);
			if (ViewportScale <= 0.0f) ViewportScale = 1.0f;

			FVector2D PixelPosition = Layout.TargetPosition * ViewportSize;
			FVector2D PixelSize = Layout.TargetSize * ViewportSize;

			Layout.Widget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			Layout.Widget->SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
			Layout.Widget->SetDesiredSizeInViewport(PixelSize / ViewportScale);
			Layout.Widget->SetPositionInViewport(PixelPosition, true);
		}
	}
}
