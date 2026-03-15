// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsSlider.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

void URammsSlider::ResetCachedWidgets()
{
	ContainerBorder = nullptr;
	ContentVBox = nullptr;
	InnerSlider = nullptr;
	SliderLabel = nullptr;
	ValueLabel = nullptr;
}

void URammsSlider::BuildWidgetTree()
{
	if (!WidgetTree || InnerSlider)
		return; // Already built or no tree

	// Root: border panel (always created; transparent when bShowPanel is false)
	ContainerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ContainerBorder"));
	ContainerBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(
		FLinearColor(0.06f, 0.06f, 0.08f, 0.92f), 4.0f,
		FLinearColor(0.3f, 0.3f, 0.3f, 1.0f), 1.0f);
	ContainerBorder->SetPadding(PanelPadding);
	ContainerBorder->SetClipping(EWidgetClipping::ClipToBounds);
	WidgetTree->RootWidget = ContainerBorder;

	// Content VBox inside the border
	ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentVBox"));
	ContainerBorder->AddChild(ContentVBox);

	// Row 1: HorizontalBox with SliderLabel (left) + ValueLabel (right)
	UHorizontalBox* LabelRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LabelRow"));
	UVerticalBoxSlot* LabelRowSlot = ContentVBox->AddChildToVerticalBox(LabelRow);
	if (LabelRowSlot)
	{
		LabelRowSlot->SetHorizontalAlignment(HAlign_Fill);
		LabelRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	SliderLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SliderLabel"));
	SliderLabel->SetText(LabelText);
	UHorizontalBoxSlot* SliderLabelSlot = LabelRow->AddChildToHorizontalBox(SliderLabel);
	if (SliderLabelSlot)
	{
		SliderLabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SliderLabelSlot->SetHorizontalAlignment(HAlign_Left);
		SliderLabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	ValueLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ValueLabel"));
	ValueLabel->SetVisibility(bShowValue ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	UHorizontalBoxSlot* ValueLabelSlot = LabelRow->AddChildToHorizontalBox(ValueLabel);
	if (ValueLabelSlot)
	{
		ValueLabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		ValueLabelSlot->SetHorizontalAlignment(HAlign_Right);
		ValueLabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Row 2: InnerSlider
	InnerSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("InnerSlider"));
	UVerticalBoxSlot* SliderSlot = ContentVBox->AddChildToVerticalBox(InnerSlider);
	if (SliderSlot)
	{
		SliderSlot->SetHorizontalAlignment(HAlign_Fill);
		SliderSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
	}

	// Apply initial panel visibility
	if (!bShowPanel)
	{
		ContainerBorder->Background = FSlateBrush();
		ContainerBorder->Background.DrawAs = ESlateBrushDrawType::NoDrawType;
		ContainerBorder->SetPadding(FMargin(0.0f));
	}

	// Apply initial slider appearance
	ApplySliderAppearance();
}

void URammsSlider::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsSlider::NativeConstruct()
{
	Super::NativeConstruct();

	if (InnerSlider)
	{
		InnerSlider->SetMinValue(MinValue);
		InnerSlider->SetMaxValue(MaxValue);
		InnerSlider->SetValue(Value);
		InnerSlider->SetStepSize(StepSize);
		InnerSlider->OnValueChanged.AddDynamic(this, &URammsSlider::OnSliderValueChanged);
	}

	if (SliderLabel)
	{
		SliderLabel->SetText(LabelText);
	}

	UpdateValueLabel();
	ApplySliderAppearance();
}

void URammsSlider::ApplyStyle_Implementation()
{
	float Radius = Style ? Style->Border.CornerRadiusMedium : 4.0f;
	float BorderW = Style ? Style->Border.BorderWidth : 1.0f;

	// Panel background
	if (ContainerBorder)
	{
		if (bShowPanel && Style)
		{
			FLinearColor Bg = Style->Colors.Background;
			Bg.A = 0.92f;

			if (bShowPanelBorder)
			{
				FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrush(Bg, Radius, Style->Colors.Border, BorderW);
				URammsUIStyle::ApplyRoundedBrushToBorder(ContainerBorder, Brush);
			}
			else
			{
				FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrush(Bg, Radius);
				URammsUIStyle::ApplyRoundedBrushToBorder(ContainerBorder, Brush);
			}
			ContainerBorder->SetPadding(PanelPadding);
		}
		else if (bShowPanel)
		{
			// No style but panel requested — use hardcoded dark defaults
			FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrush(
				FLinearColor(0.06f, 0.06f, 0.08f, 0.92f), 4.0f,
				FLinearColor(0.3f, 0.3f, 0.3f, 1.0f), 1.0f);
			URammsUIStyle::ApplyRoundedBrushToBorder(ContainerBorder, Brush);
			ContainerBorder->SetPadding(PanelPadding);
		}
		else
		{
			// No panel — transparent, no padding
			FSlateBrush NoBrush;
			NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
			URammsUIStyle::ApplyRoundedBrushToBorder(ContainerBorder, NoBrush);
			ContainerBorder->SetPadding(FMargin(0.0f));
		}
	}

	// Label styling
	if (SliderLabel)
	{
		if (Style)
		{
			SliderLabel->SetFont(Style->Typography.Body);
			SliderLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
		}
		else
		{
			SliderLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
	}

	// Value label styling
	if (ValueLabel)
	{
		if (Style)
		{
			ValueLabel->SetFont(Style->Typography.Monospace);
			ValueLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
		}
		else
		{
			ValueLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)));
		}
		ValueLabel->SetVisibility(bShowValue ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Slider appearance
	ApplySliderAppearance();
}

void URammsSlider::ApplySliderAppearance()
{
	if (!InnerSlider)
		return;

	FRammsSliderStyle Resolved;

	if (bUseStyleColors && Style)
	{
		// Start from the centralized style definition
		Resolved = Style->Slider;

		// Apply explicit per-widget size overrides
		if (bOverrideThumbSize)
			Resolved.ThumbSize = ThumbSize;
		if (bOverrideBarThickness)
			Resolved.BarThickness = BarThickness;
	}
	else
	{
		// Per-widget manual overrides for everything
		Resolved.ThumbSize = ThumbSize;
		Resolved.BarThickness = BarThickness;
		Resolved.TrackColor = TrackColor;
		Resolved.ActiveBarColor = ActiveBarColor;
		Resolved.ThumbColor = ThumbColor;
	}

	URammsUIStyle::ApplySliderStyle(InnerSlider, Resolved);
}

void URammsSlider::SetValue(float NewValue)
{
	Value = FMath::Clamp(NewValue, MinValue, MaxValue);

	if (InnerSlider)
	{
		InnerSlider->SetValue(Value);
	}

	UpdateValueLabel();
}

void URammsSlider::SetRange(float Min, float Max)
{
	MinValue = Min;
	MaxValue = Max;
	Value = FMath::Clamp(Value, MinValue, MaxValue);

	if (InnerSlider)
	{
		InnerSlider->SetMinValue(MinValue);
		InnerSlider->SetMaxValue(MaxValue);
		InnerSlider->SetValue(Value);
	}

	UpdateValueLabel();
}

void URammsSlider::SetLabel(FText Label)
{
	LabelText = Label;
	if (SliderLabel)
	{
		SliderLabel->SetText(LabelText);
	}
}

void URammsSlider::SetUnits(FText Units)
{
	UnitsText = Units;
	UpdateValueLabel();
}

void URammsSlider::SetShowPanel(bool bShow)
{
	bShowPanel = bShow;
	if (Style)
	{
		ApplyStyle();
	}
	else if (ContainerBorder)
	{
		// No style — toggle panel manually
		if (bShowPanel)
		{
			ContainerBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(
				FLinearColor(0.06f, 0.06f, 0.08f, 0.92f), 4.0f,
				FLinearColor(0.3f, 0.3f, 0.3f, 1.0f), 1.0f);
			ContainerBorder->SetPadding(PanelPadding);
		}
		else
		{
			FSlateBrush NoBrush;
			NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
			ContainerBorder->Background = NoBrush;
			ContainerBorder->SetPadding(FMargin(0.0f));
		}
	}
}

void URammsSlider::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (SliderLabel)
	{
		SliderLabel->SetText(LabelText);
	}
	if (InnerSlider)
	{
		InnerSlider->SetMinValue(MinValue);
		InnerSlider->SetMaxValue(MaxValue);
		InnerSlider->SetValue(Value);
		InnerSlider->SetStepSize(StepSize);
	}
	if (ValueLabel)
	{
		ValueLabel->SetVisibility(bShowValue ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	UpdateValueLabel();
	ApplySliderAppearance();
}

void URammsSlider::OnSliderValueChanged(float NewValue)
{
	Value = NewValue;
	UpdateValueLabel();
	OnValueChanged.Broadcast(Value);
}

void URammsSlider::UpdateValueLabel()
{
	if (!ValueLabel || !bShowValue)
		return;

	FNumberFormattingOptions FormatOptions;
	FormatOptions.MinimumFractionalDigits = DecimalPlaces;
	FormatOptions.MaximumFractionalDigits = DecimalPlaces;

	FText ValueText = FText::AsNumber(Value, &FormatOptions);
	if (!UnitsText.IsEmpty())
	{
		ValueLabel->SetText(FText::Format(NSLOCTEXT("RammsSlider", "ValueWithUnits", "{0} {1}"), ValueText, UnitsText));
	}
	else
	{
		ValueLabel->SetText(ValueText);
	}
}
