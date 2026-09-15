// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsControlRow.h"
#include "UI/RammsAxisControl.h"
#include "UI/RammsButton.h"
#include "UI/RammsUIStyle.h"
#include "RammsControlSink.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

void URammsControlRow::BuildWidgetTree()
{
	if (!WidgetTree || RootBox)
	{
		return;
	}
	RootBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RootBox"));
	WidgetTree->RootWidget = RootBox;
	// Children are created in Setup, once the control is known.
}

void URammsControlRow::ResetCachedWidgets()
{
	RootBox = nullptr;
	AxisControl = nullptr;
	ActionButton = nullptr;
	RateLabel = nullptr;
	RateValue = nullptr;
	MinusButton = nullptr;
	PlusButton = nullptr;
	MinusLabel = nullptr;
	PlusLabel = nullptr;
}

UWidget* URammsControlRow::GetRootWidgetForValidation()
{
	return RootBox;
}

FText URammsControlRow::UnitsText() const
{
	switch (Axis.Units)
	{
		case ERammsControlUnits::Radians:
			return FText::FromString(TEXT("rad"));
		case ERammsControlUnits::Degrees:
			return FText::FromString(TEXT("°"));
		case ERammsControlUnits::Centimeters:
			return FText::FromString(TEXT("cm"));
		case ERammsControlUnits::Meters:
			return FText::FromString(TEXT("m"));
		case ERammsControlUnits::RadiansPerSecond:
			return FText::FromString(TEXT("rad/s"));
		case ERammsControlUnits::CentimetersPerSecond:
			return FText::FromString(TEXT("cm/s"));
		default:
			return FText::GetEmpty();
	}
}

int32 URammsControlRow::Decimals() const
{
	switch (Axis.Units)
	{
		case ERammsControlUnits::Radians:
		case ERammsControlUnits::RadiansPerSecond:
		case ERammsControlUnits::Normalized:
			return 2;
		default:
			return 1;
	}
}

UButton* URammsControlRow::MakeHoldButton(const TCHAR* Name, const TCHAR* Label, UTextBlock*& OutLabel)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	OutLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%sLabel"), Name));
	OutLabel->SetText(FText::FromString(Label));
	OutLabel->SetJustification(ETextJustify::Center);
	Button->AddChild(OutLabel);
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("%sBox"), Name));
	Box->SetWidthOverride(44.0f);
	Box->SetHeightOverride(36.0f);
	Box->AddChild(Button);
	UHorizontalBoxSlot* BoxSlot = RootBox->AddChildToHorizontalBox(Box);
	if (BoxSlot)
	{
		BoxSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
		BoxSlot->SetVerticalAlignment(VAlign_Center);
	}
	return Button;
}

void URammsControlRow::Setup(UObject* InSink, const FRammsControlAxis& InAxis, ERammsControlSource InSource)
{
	Sink = InSink;
	Axis = InAxis;
	Source = InSource;
	if (!WidgetTree)
	{
		return;
	}
	if (!RootBox)
	{
		BuildWidgetTree();
	}
	RootBox->ClearChildren();
	AxisControl = nullptr;
	ActionButton = nullptr;
	MinusButton = PlusButton = nullptr;

	const FText Label = Axis.DisplayName.IsEmpty() ? FText::FromName(Axis.Id) : Axis.DisplayName;

	if (Axis.IsAction())
	{
		ActionButton = CreateWidget<URammsButton>(this);
		ActionButton->SetText(Label);
		ActionButton->OnClicked.AddUniqueDynamic(this, &URammsControlRow::OnActionClicked);
		UHorizontalBoxSlot* BoxSlot = RootBox->AddChildToHorizontalBox(ActionButton);
		if (BoxSlot)
		{
			BoxSlot->SetPadding(FMargin(0.0f, 2.0f));
		}
	}
	else if (Axis.Kind == ERammsControlKind::Continuous)
	{
		// Rate axis: label, live value, hold buttons.
		RateLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RateLabel"));
		RateLabel->SetText(Label);
		UHorizontalBoxSlot* LabelSlot = RootBox->AddChildToHorizontalBox(RateLabel);
		if (LabelSlot)
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		RateValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RateValue"));
		RateValue->SetText(FText::FromString(TEXT("0.00")));
		UHorizontalBoxSlot* ValueSlot = RootBox->AddChildToHorizontalBox(RateValue);
		if (ValueSlot)
		{
			ValueSlot->SetPadding(FMargin(6.0f, 0.0f));
			ValueSlot->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* MinusText = nullptr;
		UTextBlock* PlusText = nullptr;
		MinusButton = MakeHoldButton(TEXT("Minus"), TEXT("−"), MinusText);
		PlusButton = MakeHoldButton(TEXT("Plus"), TEXT("+"), PlusText);
		MinusLabel = MinusText;
		PlusLabel = PlusText;
		MinusButton->OnPressed.AddUniqueDynamic(this, &URammsControlRow::OnMinusPressed);
		MinusButton->OnReleased.AddUniqueDynamic(this, &URammsControlRow::OnMinusReleased);
		PlusButton->OnPressed.AddUniqueDynamic(this, &URammsControlRow::OnPlusPressed);
		PlusButton->OnReleased.AddUniqueDynamic(this, &URammsControlRow::OnPlusReleased);
	}
	else
	{
		// Position / Velocity target.
		AxisControl = CreateWidget<URammsAxisControl>(this);
		FRammsAxisConfig Config;
		Config.Label = Label;
		Config.IconSize = FVector2D::ZeroVector; // no icon: don't reserve the box
		Config.MinValue = static_cast<float>(Axis.Range.X);
		Config.MaxValue = static_cast<float>(Axis.Range.Y);
		Config.DefaultValue = Axis.DefaultValue;
		Config.Units = UnitsText();
		Config.DecimalPlaces = Decimals();
		Config.bShowSlider = true;
		// No step snapping: a snapped readback would differ from the pushed
		// value and the axis control would re-broadcast it as user input.
		Config.StepSize = 0.0f;
		AxisControl->ApplyConfig(Config);
		AxisControl->SetValue(Axis.DefaultValue);
		AxisControl->OnValueChanged.AddUniqueDynamic(this, &URammsControlRow::OnAxisValue);
		UHorizontalBoxSlot* BoxSlot = RootBox->AddChildToHorizontalBox(AxisControl);
		if (BoxSlot)
		{
			BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
	ApplyStyle();
	Refresh();
}

void URammsControlRow::Refresh()
{
	if (!Sink || !Axis.bReadback || Axis.IsAction())
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now - LastUserInputTime < ReadbackHoldOff)
	{
		return;
	}
	const float Value = IRammsControlSink::Execute_GetAxisValue(Sink, Axis.Id);
	if (AxisControl)
	{
		// Display only. The guard drops any OnValueChanged the control might
		// still raise for a programmatic update, so readback never becomes a
		// command (that loop would hold the axis and fight other drivers).
		bRefreshing = true;
		AxisControl->SetValue(Value);
		bRefreshing = false;
	}
	else if (RateValue)
	{
		RateValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), Value)));
	}
}

void URammsControlRow::SetAxis(float Value)
{
	if (Sink && Sink->GetClass()->ImplementsInterface(URammsControlSink::StaticClass()))
	{
		LastUserInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		IRammsControlSink::Execute_SetAxis(Sink, Axis.Id, Value, Source);
	}
}

void URammsControlRow::ReleaseAxis()
{
	if (Sink && Sink->GetClass()->ImplementsInterface(URammsControlSink::StaticClass()))
	{
		IRammsControlSink::Execute_ReleaseAxis(Sink, Axis.Id, Source);
	}
}

void URammsControlRow::OnAxisValue(float NewValue)
{
	if (!bRefreshing)
	{
		SetAxis(NewValue);
	}
}

void URammsControlRow::OnActionClicked()
{
	if (Sink && Sink->GetClass()->ImplementsInterface(URammsControlSink::StaticClass()))
	{
		IRammsControlSink::Execute_TriggerAction(Sink, Axis.Id, Source);
	}
}

void URammsControlRow::OnPlusPressed()
{
	SetAxis(HoldValue);
}

void URammsControlRow::OnPlusReleased()
{
	ReleaseAxis();
}

void URammsControlRow::OnMinusPressed()
{
	SetAxis(-HoldValue);
}

void URammsControlRow::OnMinusReleased()
{
	ReleaseAxis();
}

void URammsControlRow::ApplyStyle_Implementation()
{
	Super::ApplyStyle_Implementation();
	if (!Style)
	{
		return;
	}
	for (UTextBlock* Text : { RateLabel.Get(), MinusLabel.Get(), PlusLabel.Get() })
	{
		if (Text)
		{
			Text->SetFont(Style->Typography.Body);
			Text->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
		}
	}
	if (RateValue)
	{
		RateValue->SetFont(Style->Typography.Body);
		RateValue->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
	}
}

// --- scripted interaction ------------------------------------------------------

void URammsControlRow::SimulateValue(float Value)
{
	if (AxisControl)
	{
		AxisControl->SetValue(Value);
	}
	SetAxis(Value);
}

void URammsControlRow::SimulateAction()
{
	OnActionClicked();
}

void URammsControlRow::SimulateHold(bool bPlus)
{
	bPlus ? OnPlusPressed() : OnMinusPressed();
}

void URammsControlRow::SimulateRelease()
{
	ReleaseAxis();
}
