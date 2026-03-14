// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsSlider.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

void URammsSlider::ResetCachedWidgets()
{
	InnerSlider = nullptr;
	SliderLabel = nullptr;
	ValueLabel = nullptr;
}

void URammsSlider::BuildWidgetTree()
{
	if (!WidgetTree || InnerSlider)
		return; // Already built or no tree

	// Root: VerticalBox
	UVerticalBox* RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
	WidgetTree->RootWidget = RootBox;

	// Row 1: HorizontalBox with SliderLabel (left) + ValueLabel (right)
	UHorizontalBox* LabelRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LabelRow"));
	UVerticalBoxSlot* LabelRowSlot = RootBox->AddChildToVerticalBox(LabelRow);
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
	UVerticalBoxSlot* SliderSlot = RootBox->AddChildToVerticalBox(InnerSlider);
	if (SliderSlot)
	{
		SliderSlot->SetHorizontalAlignment(HAlign_Fill);
	}
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
}

void URammsSlider::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	// Apply label styling
	if (SliderLabel)
	{
		SliderLabel->SetFont(Style->Typography.Body);
		SliderLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}

	// Apply value label styling
	if (ValueLabel)
	{
		ValueLabel->SetFont(Style->Typography.Monospace);
		ValueLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
		ValueLabel->SetVisibility(bShowValue ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Style the slider
	if (InnerSlider)
	{
		// USlider uses FSliderStyle which we can't easily set from code in UE5
		// In production, you'd want to create a USliderStyle asset and reference it
		// For now, the slider will use default styling
	}
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
