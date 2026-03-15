// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsUIStyle.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Styling/SlateTypes.h"

void URammsUIStyle::ApplyRoundedBrushToBorder(UBorder* Border, const FSlateBrush& Brush)
{
	if (!Border)
		return;

	Border->Background = Brush;

	// If the underlying Slate widget already exists, push changes to it
	if (Border->GetCachedWidget().IsValid())
	{
		Border->SynchronizeProperties();
	}
}

void URammsUIStyle::ApplyScrollBarStyle(UScrollBox* ScrollBox, const FRammsScrollBarStyle& SBS)
{
	if (!ScrollBox)
		return;

	float R = SBS.CornerRadius;

	// Thumb brushes (rounded, semi-transparent)
	FSlateBrush ThumbNormal  = MakeRoundedBoxBrush(SBS.ThumbNormalColor, R);
	FSlateBrush ThumbHovered = MakeRoundedBoxBrush(SBS.ThumbHoveredColor, R);
	FSlateBrush ThumbDragged = MakeRoundedBoxBrush(SBS.ThumbDraggedColor, R);

	// Track brush
	FSlateBrush TrackBrush = MakeRoundedBoxBrush(SBS.TrackColor, R);

	FScrollBarStyle BarStyle = FScrollBarStyle::GetDefault();
	BarStyle.SetNormalThumbImage(ThumbNormal);
	BarStyle.SetHoveredThumbImage(ThumbHovered);
	BarStyle.SetDraggedThumbImage(ThumbDragged);
	BarStyle.SetVerticalBackgroundImage(TrackBrush);
	BarStyle.SetHorizontalBackgroundImage(TrackBrush);
	BarStyle.SetVerticalTopSlotImage(TrackBrush);
	BarStyle.SetVerticalBottomSlotImage(TrackBrush);
	BarStyle.SetHorizontalTopSlotImage(TrackBrush);
	BarStyle.SetHorizontalBottomSlotImage(TrackBrush);
	BarStyle.SetThickness(SBS.Thickness);

	ScrollBox->SetWidgetBarStyle(BarStyle);
	ScrollBox->SetScrollbarThickness(FVector2D(SBS.Thickness, SBS.Thickness));
	ScrollBox->SetScrollbarPadding(SBS.Padding);

	// Disable scroll edge shadows (they ignore rounded corners)
	if (SBS.bDisableEdgeShadows)
	{
		FScrollBoxStyle BoxStyle = ScrollBox->GetWidgetStyle();
		FSlateBrush EmptyBrush;
		EmptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		BoxStyle.SetTopShadowBrush(EmptyBrush);
		BoxStyle.SetBottomShadowBrush(EmptyBrush);
		BoxStyle.SetLeftShadowBrush(EmptyBrush);
		BoxStyle.SetRightShadowBrush(EmptyBrush);
		ScrollBox->SetWidgetStyle(BoxStyle);
	}
}

void URammsUIStyle::ApplySliderStyle(USlider* Slider, const FRammsSliderStyle& SS)
{
	if (!Slider)
		return;

	float ThumbRadius = SS.ThumbSize * 0.5f;
	float BarRadius = SS.BarThickness * 0.5f;

	// Thumb brushes (circular via large corner radius)
	FSlateBrush ThumbNormal = MakeRoundedBoxBrush(SS.ThumbColor, ThumbRadius);
	ThumbNormal.SetImageSize(FVector2D(SS.ThumbSize, SS.ThumbSize));

	FSlateBrush ThumbHovered = MakeRoundedBoxBrush(SS.GetThumbHoveredColor(), ThumbRadius);
	ThumbHovered.SetImageSize(FVector2D(SS.ThumbSize, SS.ThumbSize));

	FSlateBrush ThumbDisabled = MakeRoundedBoxBrush(SS.DisabledColor, ThumbRadius);
	ThumbDisabled.SetImageSize(FVector2D(SS.ThumbSize, SS.ThumbSize));

	// Bar / track brushes (rounded rectangle)
	FSlateBrush BarNormal = MakeRoundedBoxBrush(SS.TrackColor, BarRadius);
	BarNormal.SetImageSize(FVector2D(SS.BarThickness, SS.BarThickness));

	FLinearColor BarHoverColor = FLinearColor::LerpUsingHSV(SS.TrackColor, SS.ActiveBarColor, 0.15f);
	FSlateBrush BarHovered = MakeRoundedBoxBrush(BarHoverColor, BarRadius);
	BarHovered.SetImageSize(FVector2D(SS.BarThickness, SS.BarThickness));

	FSlateBrush BarDisabled = MakeRoundedBoxBrush(SS.DisabledColor, BarRadius);
	BarDisabled.SetImageSize(FVector2D(SS.BarThickness, SS.BarThickness));

	// Assemble FSliderStyle
	FSliderStyle SliderStyle = FSliderStyle::GetDefault();
	SliderStyle.SetNormalBarImage(BarNormal);
	SliderStyle.SetHoveredBarImage(BarHovered);
	SliderStyle.SetDisabledBarImage(BarDisabled);
	SliderStyle.SetNormalThumbImage(ThumbNormal);
	SliderStyle.SetHoveredThumbImage(ThumbHovered);
	SliderStyle.SetDisabledThumbImage(ThumbDisabled);
	SliderStyle.SetBarThickness(SS.BarThickness);

	Slider->SetWidgetStyle(SliderStyle);

	// Tint the filled bar with the active color
	Slider->SetSliderBarColor(SS.ActiveBarColor);
	Slider->SetSliderHandleColor(FLinearColor::White);

	// Push to Slate if already constructed
	if (Slider->GetCachedWidget().IsValid())
	{
		Slider->SynchronizeProperties();
	}
}
