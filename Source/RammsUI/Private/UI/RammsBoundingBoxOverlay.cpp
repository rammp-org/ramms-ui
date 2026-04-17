// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsBoundingBoxOverlay.h"
#include "RammsUISubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"

void URammsBoundingBoxOverlay::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	// Minimal tree: just an overlay that fills the parent.
	// All drawing is done in NativePaint.
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BBoxRoot"));
	WidgetTree->RootWidget = Root;
}

void URammsBoundingBoxOverlay::NativeConstruct()
{
	Super::NativeConstruct();

	// Overlay should not intercept input by default
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (bAutoSubscribe)
	{
		SubscribeToSubsystem();
	}

	// Default palette if empty
	if (ColorPalette.Num() == 0)
	{
		ColorPalette = {
			FLinearColor(1.0f, 0.2f, 0.2f, 1.0f), // Red
			FLinearColor(0.2f, 1.0f, 0.3f, 1.0f), // Green
			FLinearColor(0.3f, 0.5f, 1.0f, 1.0f), // Blue
			FLinearColor(1.0f, 0.9f, 0.1f, 1.0f), // Yellow
			FLinearColor(1.0f, 0.5f, 0.0f, 1.0f), // Orange
			FLinearColor(0.8f, 0.2f, 1.0f, 1.0f), // Purple
			FLinearColor(0.0f, 1.0f, 1.0f, 1.0f), // Cyan
			FLinearColor(1.0f, 0.4f, 0.7f, 1.0f), // Pink
		};
	}
}

void URammsBoundingBoxOverlay::NativeDestruct()
{
	UnsubscribeFromSubsystem();
	Super::NativeDestruct();
}

void URammsBoundingBoxOverlay::ApplyStyle_Implementation()
{
	// Force font cache rebuild on next paint so the style font is picked up
	CachedFontSize = 0;

	// Ensure widgets inside invalidation panels are repainted immediately so
	// the updated style/font is actually picked up.
	InvalidateLayoutAndVolatility();
}

// ── Helpers ──────────────────────────────────────────────────────

static const FSlateBrush* GetCachedWhiteBrush()
{
	static const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("GenericWhiteBox");
	return Brush;
}

/** Lines ≤ this threshold use a single MakeLines call; thicker use MakeBox for clean joins */
static constexpr float CornerFillThreshold = 2.0f;

// ── Effective Line Thickness ─────────────────────────────────────

float URammsBoundingBoxOverlay::GetEffectiveLineThickness() const
{
	// Explicit per-widget override always wins
	if (bOverrideLineThickness)
	{
		return LineThickness;
	}
	// Fall back to style when configured (> 0)
	if (Style && Style->Interaction.BoundingBoxLineThickness > 0.0f)
	{
		return Style->Interaction.BoundingBoxLineThickness;
	}
	return LineThickness;
}

void URammsBoundingBoxOverlay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Auto-clear after lifetime expires
	if (DetectionLifetime > 0.0f && Boxes.Num() > 0)
	{
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		if (Now - LastDetectionTime >= DetectionLifetime)
		{
			ClearDetections();
		}
	}
}

// ── API ──────────────────────────────────────────────────────────

void URammsBoundingBoxOverlay::SetDetections(const TArray<FRammsBoundingBox>& InBoxes)
{
	Boxes = InBoxes;
	if (Boxes.Num() > 0)
	{
		LastDetectionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	}
	InvalidateLayoutAndVolatility();
}

void URammsBoundingBoxOverlay::ClearDetections()
{
	if (Boxes.Num() > 0)
	{
		Boxes.Empty();
		InvalidateLayoutAndVolatility();
	}
}

// ── Subsystem ────────────────────────────────────────────────────

void URammsBoundingBoxOverlay::SubscribeToSubsystem()
{
	if (bSubscribed)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
		{
			Subsystem->OnDetectionsReceived.AddUniqueDynamic(this, &URammsBoundingBoxOverlay::HandleDetectionsReceived);
			Subsystem->OnDetectionsCleared.AddUniqueDynamic(this, &URammsBoundingBoxOverlay::HandleDetectionsCleared);
			bSubscribed = true;
		}
	}
}

void URammsBoundingBoxOverlay::UnsubscribeFromSubsystem()
{
	if (!bSubscribed)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
		{
			Subsystem->OnDetectionsReceived.RemoveDynamic(this, &URammsBoundingBoxOverlay::HandleDetectionsReceived);
			Subsystem->OnDetectionsCleared.RemoveDynamic(this, &URammsBoundingBoxOverlay::HandleDetectionsCleared);
		}
	}
	bSubscribed = false;
}

void URammsBoundingBoxOverlay::HandleDetectionsReceived(FName SourceTag, const TArray<FRammsBoundingBox>& InBoxes)
{
	// Filter by source tag if configured
	if (!SourceTagFilter.IsNone() && SourceTag != SourceTagFilter)
	{
		return;
	}

	SetDetections(InBoxes);
}

void URammsBoundingBoxOverlay::HandleDetectionsCleared(FName SourceTag)
{
	// NAME_None = clear all overlays unconditionally
	if (SourceTag.IsNone())
	{
		ClearDetections();
		return;
	}

	// Specific source tag: only clear if our filter matches
	if (!SourceTagFilter.IsNone() && SourceTag == SourceTagFilter)
	{
		ClearDetections();
	}
	// Unfiltered overlays (SourceTagFilter is None) are NOT cleared by
	// source-specific clears — only by NAME_None broadcasts.
}

// ── Color Resolution ─────────────────────────────────────────────

FLinearColor URammsBoundingBoxOverlay::ResolveBoxColor(const FRammsBoundingBox& Box, int32 Index) const
{
	if (Box.HasCustomColor())
	{
		return Box.Color;
	}

	switch (ColorMode)
	{
		case ERammsBBoxColorMode::LabelHash:
			if (!Box.Label.IsEmpty())
			{
				return ColorFromLabelHash(Box.Label);
			}
			return DefaultBoxColor;

		case ERammsBBoxColorMode::Palette:
			if (ColorPalette.Num() > 0)
			{
				return ColorPalette[Index % ColorPalette.Num()];
			}
			return DefaultBoxColor;

		case ERammsBBoxColorMode::Uniform:
		default:
			return DefaultBoxColor;
	}
}

FLinearColor URammsBoundingBoxOverlay::ColorFromLabelHash(const FText& Label)
{
	const uint32 Hash = GetTypeHash(Label.ToString());

	// Generate a saturated color from the hash using HSV
	const float Hue = (Hash % 360);
	const float Saturation = 0.75f + (((Hash >> 10) % 25) / 100.0f); // 0.75-1.0
	const float Value = 0.85f + (((Hash >> 16) % 15) / 100.0f);		 // 0.85-1.0

	return FLinearColor::MakeFromHSV8(
		static_cast<uint8>(Hue * 255.0f / 360.0f),
		static_cast<uint8>(Saturation * 255.0f),
		static_cast<uint8>(Value * 255.0f));
}

// ── Font Cache ───────────────────────────────────────────────────

void URammsBoundingBoxOverlay::EnsureLabelFont() const
{
	// Style font takes priority when configured
	if (Style && Style->Interaction.GetBoundingBoxLabelFont())
	{
		const FSlateFontInfo& StyleFont = *Style->Interaction.GetBoundingBoxLabelFont();
		if (!(CachedLabelFont == StyleFont))
		{
			CachedLabelFont = StyleFont;
			CachedFontSize = StyleFont.Size;
		}
		return;
	}

	if (CachedFontSize != LabelFontSize)
	{
		CachedLabelFont = FCoreStyle::GetDefaultFontStyle("Regular", LabelFontSize);
		CachedFontSize = LabelFontSize;
	}
}

// ── NativePaint ──────────────────────────────────────────────────

int32 URammsBoundingBoxOverlay::NativePaint(const FPaintArgs& Args,
	const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (Boxes.Num() == 0)
	{
		return LayerId;
	}

	// Draw all boxes on a layer above the base
	const int32 BoxLayerId = LayerId + 1;

	const int32		EffectivePanes = FMath::Clamp(PaneCount, 1, 4);
	const FVector2D FullSize = AllottedGeometry.GetLocalSize();

	for (int32 Pane = 0; Pane < EffectivePanes; ++Pane)
	{
		// Compute pane geometry: each pane is an equal-width region
		FGeometry PaneGeom = AllottedGeometry;
		if (EffectivePanes > 1)
		{
			const float TotalGap = PaneGap * (EffectivePanes - 1);
			const float PaneWidth = (FullSize.X - TotalGap) / EffectivePanes;
			const float PaneOffset = Pane * (PaneWidth + PaneGap);
			PaneGeom = AllottedGeometry.MakeChild(
				FVector2D(PaneWidth, FullSize.Y),
				FSlateLayoutTransform(FVector2D(PaneOffset, 0.0f)));
		}

		for (int32 i = 0; i < Boxes.Num(); ++i)
		{
			const FRammsBoundingBox& Box = Boxes[i];

			switch (Box.Shape)
			{
				case ERammsDetectionShape::RotatedRect:
					DrawRotatedRectBox(Box, i, PaneGeom, OutDrawElements, BoxLayerId);
					break;
				case ERammsDetectionShape::Polygon:
					DrawPolygonBox(Box, i, PaneGeom, OutDrawElements, BoxLayerId);
					break;
				case ERammsDetectionShape::Rect:
				default:
					DrawRectBox(Box, i, PaneGeom, OutDrawElements, BoxLayerId);
					break;
			}
		}
	}

	return BoxLayerId + 1;
}

// ── Drawing Helpers ──────────────────────────────────────────────

void URammsBoundingBoxOverlay::DrawRectBox(const FRammsBoundingBox& Box, int32 Index,
	const FGeometry& Geom, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	const FLinearColor BoxColor = ResolveBoxColor(Box, Index);
	const FVector2D	   GeomSize = Geom.GetLocalSize();
	const float		   T = GetEffectiveLineThickness();

	// Convert normalized coords to local pixel coords and normalize extents
	const float RawX0 = Box.Position.X * GeomSize.X;
	const float RawY0 = Box.Position.Y * GeomSize.Y;
	const float RawX1 = (Box.Position.X + Box.Size.X) * GeomSize.X;
	const float RawY1 = (Box.Position.Y + Box.Size.Y) * GeomSize.Y;

	const float XMin = FMath::Min(RawX0, RawX1);
	const float YMin = FMath::Min(RawY0, RawY1);
	const float XMax = FMath::Max(RawX0, RawX1);
	const float YMax = FMath::Max(RawY0, RawY1);
	const float BoxW = XMax - XMin;
	const float BoxH = YMax - YMin;

	// Early-out: box too small to meaningfully outline
	if (BoxW < 1.0f && BoxH < 1.0f)
	{
		return;
	}

	// Thin lines: single MakeLines call (1 draw element, gaps invisible at ≤ threshold)
	// Thick lines: 4 MakeBox filled rectangles for perfectly joined corners
	if (T <= CornerFillThreshold)
	{
		ScratchPoints.SetNumUninitialized(5);
		ScratchPoints[0] = FVector2D(XMin, YMin);
		ScratchPoints[1] = FVector2D(XMax, YMin);
		ScratchPoints[2] = FVector2D(XMax, YMax);
		ScratchPoints[3] = FVector2D(XMin, YMax);
		ScratchPoints[4] = ScratchPoints[0]; // Close
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geom.ToPaintGeometry(),
			ScratchPoints, ESlateDrawEffect::None, BoxColor, true, T);
	}
	else
	{
		const float		   HalfT = T * 0.5f;
		const FSlateBrush* WhiteBrush = GetCachedWhiteBrush();

		// Left edge (full height)
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			Geom.ToPaintGeometry(FVector2D(T, BoxH + T), FSlateLayoutTransform(FVector2D(XMin - HalfT, YMin - HalfT))),
			WhiteBrush, ESlateDrawEffect::None, BoxColor);

		// Right edge (full height)
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			Geom.ToPaintGeometry(FVector2D(T, BoxH + T), FSlateLayoutTransform(FVector2D(XMax - HalfT, YMin - HalfT))),
			WhiteBrush, ESlateDrawEffect::None, BoxColor);

		// Top/bottom edges fill between left and right (width clamped to >= 0)
		const float InnerW = FMath::Max(0.0f, BoxW - T);
		if (InnerW > 0.0f)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
				Geom.ToPaintGeometry(FVector2D(InnerW, T), FSlateLayoutTransform(FVector2D(XMin + HalfT, YMin - HalfT))),
				WhiteBrush, ESlateDrawEffect::None, BoxColor);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
				Geom.ToPaintGeometry(FVector2D(InnerW, T), FSlateLayoutTransform(FVector2D(XMin + HalfT, YMax - HalfT))),
				WhiteBrush, ESlateDrawEffect::None, BoxColor);
		}
	}

	// Label
	if (bShowLabels && !Box.Label.IsEmpty())
	{
		DrawLabel(Box, BoxColor, FVector2D(XMin, YMin), Geom, OutDrawElements, LayerId + 1);
	}

	// Centroid
	if (bShowCentroid)
	{
		const FVector2D Center = Box.GetCenter();
		const FVector2D PixelCenter(Center.X * GeomSize.X, Center.Y * GeomSize.Y);
		DrawCentroid(PixelCenter, BoxColor, Geom, OutDrawElements, LayerId + 1);
	}
}

void URammsBoundingBoxOverlay::DrawRotatedRectBox(const FRammsBoundingBox& Box, int32 Index,
	const FGeometry& Geom, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	const FLinearColor BoxColor = ResolveBoxColor(Box, Index);
	const FVector2D	   GeomSize = Geom.GetLocalSize();
	const float		   T = GetEffectiveLineThickness();

	// Compute rotated corners
	const FVector2D Center = Box.GetCenter() * GeomSize;
	const FVector2D HalfSize = Box.Size * GeomSize * 0.5f;
	const float		Rad = FMath::DegreesToRadians(Box.AngleDegrees);
	const float		CosA = FMath::Cos(Rad);
	const float		SinA = FMath::Sin(Rad);

	// Corner offsets relative to center
	const FVector2D Offsets[4] = {
		FVector2D(-HalfSize.X, -HalfSize.Y),
		FVector2D(+HalfSize.X, -HalfSize.Y),
		FVector2D(+HalfSize.X, +HalfSize.Y),
		FVector2D(-HalfSize.X, +HalfSize.Y)
	};

	ScratchPoints.SetNumUninitialized(5);
	for (int32 i = 0; i < 4; ++i)
	{
		const float RX = Offsets[i].X * CosA - Offsets[i].Y * SinA;
		const float RY = Offsets[i].X * SinA + Offsets[i].Y * CosA;
		ScratchPoints[i] = Center + FVector2D(RX, RY);
	}
	ScratchPoints[4] = ScratchPoints[0]; // Close

	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geom.ToPaintGeometry(),
		ScratchPoints, ESlateDrawEffect::None, BoxColor, true, T);

	// Fill corners with small squares to cover line-join gaps (only needed for thick lines)
	if (T > CornerFillThreshold)
	{
		const FSlateBrush* WhiteBrush = GetCachedWhiteBrush();
		const float		   HalfT = T * 0.5f;
		for (int32 i = 0; i < 4; ++i)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
				Geom.ToPaintGeometry(FVector2D(T, T),
					FSlateLayoutTransform(ScratchPoints[i] - FVector2D(HalfT, HalfT))),
				WhiteBrush, ESlateDrawEffect::None, BoxColor);
		}
	}

	// Label at top-left corner
	if (bShowLabels && !Box.Label.IsEmpty())
	{
		DrawLabel(Box, BoxColor, ScratchPoints[0], Geom, OutDrawElements, LayerId + 1);
	}

	if (bShowCentroid)
	{
		DrawCentroid(Center, BoxColor, Geom, OutDrawElements, LayerId + 1);
	}
}

void URammsBoundingBoxOverlay::DrawPolygonBox(const FRammsBoundingBox& Box, int32 Index,
	const FGeometry& Geom, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	if (Box.PolygonPoints.Num() < 3)
	{
		return;
	}

	const FLinearColor BoxColor = ResolveBoxColor(Box, Index);
	const FVector2D	   GeomSize = Geom.GetLocalSize();
	const float		   T = GetEffectiveLineThickness();
	const int32		   NumVerts = Box.PolygonPoints.Num();

	ScratchPoints.Reset(NumVerts + 1);
	FVector2D MinPt(FLT_MAX, FLT_MAX);

	for (const FVector2D& Pt : Box.PolygonPoints)
	{
		const FVector2D PixelPt(Pt.X * GeomSize.X, Pt.Y * GeomSize.Y);
		ScratchPoints.Add(PixelPt);
		MinPt.X = FMath::Min(MinPt.X, PixelPt.X);
		MinPt.Y = FMath::Min(MinPt.Y, PixelPt.Y);
	}
	if (ScratchPoints.Num() > 0)
	{
		const FVector2D ClosePt = ScratchPoints[0];
		ScratchPoints.Add(ClosePt); // Close
	}

	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geom.ToPaintGeometry(),
		ScratchPoints, ESlateDrawEffect::None, BoxColor, true, T);

	// Fill corners with small squares to cover line-join gaps (only needed for thick lines)
	if (T > CornerFillThreshold)
	{
		const FSlateBrush* WhiteBrush = GetCachedWhiteBrush();
		const float		   HalfT = T * 0.5f;
		for (int32 i = 0; i < NumVerts; ++i)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
				Geom.ToPaintGeometry(FVector2D(T, T),
					FSlateLayoutTransform(ScratchPoints[i] - FVector2D(HalfT, HalfT))),
				WhiteBrush, ESlateDrawEffect::None, BoxColor);
		}
	}

	if (bShowLabels && !Box.Label.IsEmpty())
	{
		DrawLabel(Box, BoxColor, MinPt, Geom, OutDrawElements, LayerId + 1);
	}

	if (bShowCentroid)
	{
		const FVector2D Center = Box.GetCenter();
		const FVector2D PixelCenter(Center.X * GeomSize.X, Center.Y * GeomSize.Y);
		DrawCentroid(PixelCenter, BoxColor, Geom, OutDrawElements, LayerId + 1);
	}
}

void URammsBoundingBoxOverlay::DrawLabel(const FRammsBoundingBox& Box, const FLinearColor& BoxColor,
	const FVector2D& LabelPos, const FGeometry& Geom,
	FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	EnsureLabelFont();

	// Build label string
	FString LabelStr = Box.Label.ToString();
	if (bShowConfidence && Box.HasConfidence())
	{
		LabelStr += FString::Printf(TEXT(" %.0f%%"), Box.Confidence * 100.0f);
	}

	// Slate may be unavailable in headless contexts or during shutdown.
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();
	if (!Renderer)
	{
		return;
	}

	// Measure text
	const TSharedRef<FSlateFontMeasure> FontMeasure = Renderer->GetFontMeasureService();
	const FVector2D						TextSize = FontMeasure->Measure(LabelStr, CachedLabelFont);

	const float LabelPad = 3.0f;
	const float BgWidth = TextSize.X + LabelPad * 2.0f;
	const float BgHeight = TextSize.Y + LabelPad * 2.0f;

	// Position label above the box (shift up by label height)
	const FVector2D BgPos(LabelPos.X, LabelPos.Y - BgHeight);

	// Draw background
	FLinearColor BgColor(0.0f, 0.0f, 0.0f, LabelBackgroundOpacity);
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
		Geom.ToPaintGeometry(FVector2D(BgWidth, BgHeight), FSlateLayoutTransform(BgPos)),
		GetCachedWhiteBrush(),
		ESlateDrawEffect::None, BgColor);

	// Draw text
	FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
		Geom.ToPaintGeometry(TextSize, FSlateLayoutTransform(BgPos + FVector2D(LabelPad, LabelPad))),
		LabelStr, CachedLabelFont, ESlateDrawEffect::None, BoxColor);
}

void URammsBoundingBoxOverlay::DrawCentroid(const FVector2D& Center, const FLinearColor& BoxColor,
	const FGeometry& Geom, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	const float		R = CentroidRadius;
	const FVector2D DotPos = Center - FVector2D(R, R);
	const FVector2D DotSize(R * 2.0f, R * 2.0f);

	// Draw a small filled square as centroid marker
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
		Geom.ToPaintGeometry(DotSize, FSlateLayoutTransform(DotPos)),
		GetCachedWhiteBrush(),
		ESlateDrawEffect::None, BoxColor);
}
