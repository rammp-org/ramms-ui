// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsAxisControl.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBoxSlot.h"

URammsAxisControl::URammsAxisControl(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ── Widget Tree ──────────────────────────────────────────────────

void URammsAxisControl::ResetCachedWidgets()
{
	OuterHBox = nullptr;
	IconSizeBox = nullptr;
	IconImage = nullptr;
	LabelText = nullptr;
	ValueText = nullptr;
	ResetButton = nullptr;
	ResetLabel = nullptr;
	InnerSlider = nullptr;
}

void URammsAxisControl::BuildWidgetTree()
{
	if (!WidgetTree || OuterHBox)
		return;

	// Layout: HBox(Icon, VBox(HBox(Label, Value, Reset), Slider))

	OuterHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("OuterHBox"));
	WidgetTree->RootWidget = OuterHBox;

	// ── Icon (left, spans full height of control) ──

	IconSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("IconBox"));
	IconSizeBox->SetWidthOverride(Config.IconSize.X);
	IconSizeBox->SetHeightOverride(Config.IconSize.Y);

	IconImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("Icon"));
	if (Config.Icon)
	{
		IconImage->SetBrushFromTexture(Config.Icon);
	}
	IconImage->SetColorAndOpacity(FLinearColor::White);
	IconSizeBox->AddChild(IconImage);

	UHorizontalBoxSlot* IconSlot = OuterHBox->AddChildToHorizontalBox(IconSizeBox);
	if (IconSlot)
	{
		IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetHorizontalAlignment(HAlign_Left);
	}

	// ── Right VBox: info row + slider ──

	UVerticalBox* RightVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("RightVBox"));

	UHorizontalBoxSlot* RightSlot = OuterHBox->AddChildToHorizontalBox(RightVBox);
	if (RightSlot)
	{
		RightSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		RightSlot->SetVerticalAlignment(VAlign_Center);
	}

	// ── Info row: Label + Value + Reset ──

	UHorizontalBox* InfoRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("InfoRow"));

	UVerticalBoxSlot* InfoSlot = RightVBox->AddChildToVerticalBox(InfoRow);
	if (InfoSlot)
	{
		InfoSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	// Label
	LabelText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Label"));
	LabelText->SetText(Config.Label);
	LabelText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	UHorizontalBoxSlot* LabelSlot = InfoRow->AddChildToHorizontalBox(LabelText);
	if (LabelSlot)
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Value display
	USizeBox* ValueSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("ValBox"));
	ValueSizeBox->SetMinDesiredWidth(56.0f);

	ValueText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Value"));
	ValueText->SetText(FText::FromString(TEXT("0.0")));
	ValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ValueText->SetJustification(ETextJustify::Center);
	ValueSizeBox->AddChild(ValueText);

	UHorizontalBoxSlot* ValSlot = InfoRow->AddChildToHorizontalBox(ValueSizeBox);
	if (ValSlot)
	{
		ValSlot->SetPadding(FMargin(4.0f, 0.0f, 8.0f, 0.0f));
		ValSlot->SetVerticalAlignment(VAlign_Center);
		ValSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// Reset button
	ResetButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("ResetBtn"));

	ResetLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ResetLbl"));
	ResetLabel->SetText(FText::FromString(TEXT("\u21BA"))); // ↺
	ResetLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ResetButton->AddChild(ResetLabel);

	UHorizontalBoxSlot* ResetSlot = InfoRow->AddChildToHorizontalBox(ResetButton);
	if (ResetSlot)
	{
		ResetSlot->SetPadding(FMargin(0.0f));
		ResetSlot->SetVerticalAlignment(VAlign_Center);
		ResetSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// ── Slider (below info row) ──

	InnerSlider = WidgetTree->ConstructWidget<USlider>(
		USlider::StaticClass(), TEXT("Slider"));
	InnerSlider->SetMinValue(Config.MinValue);
	InnerSlider->SetMaxValue(Config.MaxValue);
	InnerSlider->SetValue(Config.DefaultValue);
	InnerSlider->SetStepSize(Config.StepSize);
	InnerSlider->SetSliderBarColor(FLinearColor(0.2f, 0.2f, 0.25f, 1.0f));
	InnerSlider->SetSliderHandleColor(FLinearColor(0.6f, 0.6f, 0.7f, 1.0f));

	UVerticalBoxSlot* SliderSlot = RightVBox->AddChildToVerticalBox(InnerSlider);
	if (SliderSlot)
	{
		SliderSlot->SetHorizontalAlignment(HAlign_Fill);
		SliderSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
	}

	CurrentValue = Config.DefaultValue;
}

// ── Lifecycle ────────────────────────────────────────────────────

void URammsAxisControl::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsAxisControl::NativeConstruct()
{
	Super::NativeConstruct();

	if (InnerSlider)
	{
		InnerSlider->OnValueChanged.AddUniqueDynamic(this, &URammsAxisControl::OnSliderChanged);
	}
	if (ResetButton)
	{
		ResetButton->OnClicked.AddUniqueDynamic(this, &URammsAxisControl::OnResetClicked);
	}

	ApplyConfigToWidgets();
	UpdateValueDisplay();
	UpdateSliderFromValue();
}

void URammsAxisControl::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	if (LabelText)
	{
		LabelText->SetFont(Style->Typography.Body);
		LabelText->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}
	if (ValueText)
	{
		ValueText->SetFont(Style->Typography.Body);
		ValueText->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
	}
	if (ResetLabel)
	{
		ResetLabel->SetFont(Style->Typography.HeadingSmall);
		ResetLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}
	if (InnerSlider)
	{
		URammsUIStyle::ApplySliderStyle(InnerSlider, Style->Slider);
	}
	if (ResetButton)
	{
		FButtonStyle BtnStyle;
		BtnStyle.SetNormal(FSlateRoundedBoxBrush(Style->Colors.Surface, Style->Border.CornerRadiusMedium));
		BtnStyle.SetHovered(FSlateRoundedBoxBrush(Style->Colors.Primary * 0.7f, Style->Border.CornerRadiusMedium));
		BtnStyle.SetPressed(FSlateRoundedBoxBrush(Style->Colors.Primary, Style->Border.CornerRadiusMedium));
		ResetButton->SetStyle(BtnStyle);
	}
}

void URammsAxisControl::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// In the designer, sync displayed value to DefaultValue so property
	// changes are immediately reflected without needing a reset click
	if (IsDesignTime())
	{
		CurrentValue = FMath::Clamp(Config.DefaultValue, Config.MinValue, Config.MaxValue);
	}

	ApplyConfigToWidgets();
	UpdateValueDisplay();
	UpdateSliderFromValue();
}

// ── Public API ───────────────────────────────────────────────────

void URammsAxisControl::SetValue(float NewValue)
{
	NewValue = FMath::Clamp(NewValue, Config.MinValue, Config.MaxValue);
	if (FMath::IsNearlyEqual(CurrentValue, NewValue, KINDA_SMALL_NUMBER))
		return;

	CurrentValue = NewValue;
	UpdateValueDisplay();
	UpdateSliderFromValue();
	// NOTE: Does NOT broadcast OnValueChanged — only user interactions do.
}

void URammsAxisControl::ResetToDefault()
{
	float OldValue = CurrentValue;
	float NewValue = FMath::Clamp(Config.DefaultValue, Config.MinValue, Config.MaxValue);
	CurrentValue = NewValue;
	UpdateValueDisplay();
	UpdateSliderFromValue();

	if (!FMath::IsNearlyEqual(OldValue, CurrentValue, KINDA_SMALL_NUMBER))
	{
		OnValueChanged.Broadcast(CurrentValue);
	}
}

void URammsAxisControl::ApplyConfig(const FRammsAxisConfig& NewConfig)
{
	Config = NewConfig;
	CurrentValue = FMath::Clamp(CurrentValue, Config.MinValue, Config.MaxValue);
	ApplyConfigToWidgets();
	UpdateValueDisplay();
	UpdateSliderFromValue();
}

void URammsAxisControl::SetIcon(UTexture2D* NewIcon)
{
	Config.Icon = NewIcon;
	if (IconImage)
	{
		if (NewIcon)
		{
			IconImage->SetBrushFromTexture(NewIcon);
		}
		else
		{
			IconImage->SetBrush(FSlateBrush());
		}
	}
}

// ── Internal ─────────────────────────────────────────────────────

void URammsAxisControl::ApplyConfigToWidgets()
{
	if (IconSizeBox)
	{
		IconSizeBox->SetWidthOverride(Config.IconSize.X);
		IconSizeBox->SetHeightOverride(Config.IconSize.Y);
	}
	if (IconImage)
	{
		if (Config.Icon)
		{
			IconImage->SetBrushFromTexture(Config.Icon);
		}
		else
		{
			IconImage->SetBrush(FSlateBrush());
		}
	}
	if (LabelText)
	{
		LabelText->SetText(Config.Label);
	}
	if (InnerSlider)
	{
		InnerSlider->SetMinValue(Config.MinValue);
		InnerSlider->SetMaxValue(Config.MaxValue);
		InnerSlider->SetStepSize(Config.StepSize);
	}
}

void URammsAxisControl::UpdateValueDisplay()
{
	if (!ValueText)
		return;

	FNumberFormattingOptions Fmt;
	Fmt.MinimumFractionalDigits = Config.DecimalPlaces;
	Fmt.MaximumFractionalDigits = Config.DecimalPlaces;

	FText ValText = FText::AsNumber(CurrentValue, &Fmt);
	if (!Config.Units.IsEmpty())
	{
		ValueText->SetText(FText::Format(
			NSLOCTEXT("RammsAxisControl", "ValueWithUnits", "{0} {1}"), ValText, Config.Units));
	}
	else
	{
		ValueText->SetText(ValText);
	}
}

void URammsAxisControl::UpdateSliderFromValue()
{
	if (InnerSlider)
	{
		InnerSlider->SetValue(CurrentValue);
	}
}

void URammsAxisControl::OnSliderChanged(float Value)
{
	float Clamped = FMath::Clamp(Value, Config.MinValue, Config.MaxValue);
	if (FMath::IsNearlyEqual(CurrentValue, Clamped, KINDA_SMALL_NUMBER))
		return;

	CurrentValue = Clamped;
	UpdateValueDisplay();
	OnValueChanged.Broadcast(CurrentValue);
}

void URammsAxisControl::OnResetClicked()
{
	ResetToDefault();
}
