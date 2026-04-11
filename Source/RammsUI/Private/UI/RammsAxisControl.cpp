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
	ControlRow = nullptr;
	DecrementButton = nullptr;
	DecrementLabel = nullptr;
	IncrementButton = nullptr;
	IncrementLabel = nullptr;
}

void URammsAxisControl::BuildWidgetTree()
{
	if (!WidgetTree || OuterHBox)
		return;

	// Layout: HBox(Icon, VBox(HBox(Label, Value, Reset), HBox([−Btn], [Slider], [+Btn])))

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

	// ── Right VBox: info row + control row ──

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

	// ── Control row: [−Btn] [Slider] [+Btn] (below info row) ──

	ControlRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("ControlRow"));

	UVerticalBoxSlot* CtrlSlot = RightVBox->AddChildToVerticalBox(ControlRow);
	if (CtrlSlot)
	{
		CtrlSlot->SetHorizontalAlignment(HAlign_Fill);
		CtrlSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
	}

	// Decrement button  −
	DecrementSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("DecBox"));
	DecrementSizeBox->SetMinDesiredWidth(40.0f);
	DecrementSizeBox->SetMinDesiredHeight(32.0f);

	DecrementButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("DecBtn"));
	DecrementLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("DecLbl"));
	DecrementLabel->SetText(FText::FromString(TEXT("\u2212"))); // −
	DecrementLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	DecrementLabel->SetJustification(ETextJustify::Center);
	DecrementButton->AddChild(DecrementLabel);
	DecrementSizeBox->AddChild(DecrementButton);

	UHorizontalBoxSlot* DecSlot = ControlRow->AddChildToHorizontalBox(DecrementSizeBox);
	if (DecSlot)
	{
		DecSlot->SetVerticalAlignment(VAlign_Center);
		DecSlot->SetHorizontalAlignment(HAlign_Center);
		DecSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
	}

	// Slider (fills space between buttons)
	InnerSlider = WidgetTree->ConstructWidget<USlider>(
		USlider::StaticClass(), TEXT("Slider"));
	InnerSlider->SetMinValue(Config.MinValue);
	InnerSlider->SetMaxValue(Config.MaxValue);
	InnerSlider->SetValue(Config.DefaultValue);
	InnerSlider->SetStepSize(GetNormalizedStepSize());
	InnerSlider->SetSliderBarColor(FLinearColor(0.2f, 0.2f, 0.25f, 1.0f));
	InnerSlider->SetSliderHandleColor(FLinearColor(0.6f, 0.6f, 0.7f, 1.0f));

	UHorizontalBoxSlot* SliderSlot = ControlRow->AddChildToHorizontalBox(InnerSlider);
	if (SliderSlot)
	{
		SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SliderSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Increment button  +
	IncrementSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("IncBox"));
	IncrementSizeBox->SetMinDesiredWidth(40.0f);
	IncrementSizeBox->SetMinDesiredHeight(32.0f);

	IncrementButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("IncBtn"));
	IncrementLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("IncLbl"));
	IncrementLabel->SetText(FText::FromString(TEXT("+"))); // +
	IncrementLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	IncrementLabel->SetJustification(ETextJustify::Center);
	IncrementButton->AddChild(IncrementLabel);
	IncrementSizeBox->AddChild(IncrementButton);

	UHorizontalBoxSlot* IncSlot = ControlRow->AddChildToHorizontalBox(IncrementSizeBox);
	if (IncSlot)
	{
		IncSlot->SetVerticalAlignment(VAlign_Center);
		IncSlot->SetHorizontalAlignment(HAlign_Center);
		IncSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
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
	if (DecrementButton)
	{
		DecrementButton->OnClicked.AddUniqueDynamic(this, &URammsAxisControl::OnDecrementClicked);
	}
	if (IncrementButton)
	{
		IncrementButton->OnClicked.AddUniqueDynamic(this, &URammsAxisControl::OnIncrementClicked);
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

	// Shared button style for reset/decrement/increment
	FButtonStyle BtnStyle;
	if (Style)
	{
		BtnStyle.SetNormal(FSlateRoundedBoxBrush(Style->Colors.Surface, Style->Border.CornerRadiusMedium));
		BtnStyle.SetHovered(FSlateRoundedBoxBrush(Style->Colors.Primary * 0.7f, Style->Border.CornerRadiusMedium));
		BtnStyle.SetPressed(FSlateRoundedBoxBrush(Style->Colors.Primary, Style->Border.CornerRadiusMedium));
	}
	if (ResetButton)
	{
		ResetButton->SetStyle(BtnStyle);
	}
	if (DecrementButton)
	{
		DecrementButton->SetStyle(BtnStyle);
	}
	if (IncrementButton)
	{
		IncrementButton->SetStyle(BtnStyle);
	}
	if (DecrementLabel)
	{
		DecrementLabel->SetFont(Style->Interaction.GetActionButtonFont(Style->Typography.HeadingSmall));
		DecrementLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}
	if (IncrementLabel)
	{
		IncrementLabel->SetFont(Style->Interaction.GetActionButtonFont(Style->Typography.HeadingSmall));
		IncrementLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}

	// Apply style-driven min touch target sizes to action button SizeBoxes
	if (DecrementSizeBox)
	{
		DecrementSizeBox->SetMinDesiredWidth(Style->Interaction.ActionButtonMinSize.X);
		DecrementSizeBox->SetMinDesiredHeight(Style->Interaction.ActionButtonMinSize.Y);
	}
	if (IncrementSizeBox)
	{
		IncrementSizeBox->SetMinDesiredWidth(Style->Interaction.ActionButtonMinSize.X);
		IncrementSizeBox->SetMinDesiredHeight(Style->Interaction.ActionButtonMinSize.Y);
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
		InnerSlider->SetStepSize(GetNormalizedStepSize());
		InnerSlider->SetVisibility(Config.bShowSlider
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
	if (DecrementButton)
	{
		UWidget* DecParent = DecrementButton->GetParent();
		UWidget* Target = DecParent ? DecParent : Cast<UWidget>(DecrementButton);
		Target->SetVisibility(Config.bShowButtons
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
	if (IncrementButton)
	{
		UWidget* IncParent = IncrementButton->GetParent();
		UWidget* Target = IncParent ? IncParent : Cast<UWidget>(IncrementButton);
		Target->SetVisibility(Config.bShowButtons
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
	// Hide entire control row when both slider and buttons are disabled
	if (ControlRow)
	{
		ControlRow->SetVisibility((Config.bShowSlider || Config.bShowButtons)
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
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

float URammsAxisControl::GetNormalizedStepSize() const
{
	const float Range = Config.MaxValue - Config.MinValue;
	if (Range > KINDA_SMALL_NUMBER && Config.StepSize > 0.0f)
	{
		return FMath::Clamp(Config.StepSize / Range, 0.0f, 1.0f);
	}
	return 0.0f; // continuous
}

float URammsAxisControl::GetEffectiveButtonStep() const
{
	if (Config.StepSize > 0.0f)
	{
		return Config.StepSize;
	}
	// When continuous, default to 5% of range
	return (Config.MaxValue - Config.MinValue) * 0.05f;
}

float URammsAxisControl::SnapToStep(float Value) const
{
	if (Config.StepSize <= 0.0f)
	{
		return Value;
	}
	// Snap relative to MinValue so steps align with the range boundary
	const float Offset = Value - Config.MinValue;
	const float Snapped = FMath::RoundToFloat(Offset / Config.StepSize) * Config.StepSize;
	return FMath::Clamp(Config.MinValue + Snapped, Config.MinValue, Config.MaxValue);
}

void URammsAxisControl::OnSliderChanged(float Value)
{
	float Snapped = SnapToStep(FMath::Clamp(Value, Config.MinValue, Config.MaxValue));
	if (FMath::IsNearlyEqual(CurrentValue, Snapped, KINDA_SMALL_NUMBER))
		return;

	CurrentValue = Snapped;
	UpdateValueDisplay();
	OnValueChanged.Broadcast(CurrentValue);
}

void URammsAxisControl::OnResetClicked()
{
	ResetToDefault();
	OnResetPressed.Broadcast(CurrentValue);
}

void URammsAxisControl::OnDecrementClicked()
{
	const float Step = GetEffectiveButtonStep();
	float		NewValue = SnapToStep(FMath::Clamp(CurrentValue - Step, Config.MinValue, Config.MaxValue));
	if (FMath::IsNearlyEqual(CurrentValue, NewValue, KINDA_SMALL_NUMBER))
		return;

	CurrentValue = NewValue;
	UpdateValueDisplay();
	UpdateSliderFromValue();
	OnValueChanged.Broadcast(CurrentValue);
	OnDecrementPressed.Broadcast(CurrentValue);
}

void URammsAxisControl::OnIncrementClicked()
{
	const float Step = GetEffectiveButtonStep();
	float		NewValue = SnapToStep(FMath::Clamp(CurrentValue + Step, Config.MinValue, Config.MaxValue));
	if (FMath::IsNearlyEqual(CurrentValue, NewValue, KINDA_SMALL_NUMBER))
		return;

	CurrentValue = NewValue;
	UpdateValueDisplay();
	UpdateSliderFromValue();
	OnValueChanged.Broadcast(CurrentValue);
	OnIncrementPressed.Broadcast(CurrentValue);
}
