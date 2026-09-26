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
#include "Components/VerticalBox.h"

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
	EnumValue = nullptr;
	RateLabel = nullptr;
	RateValue = nullptr;
	LiveValue = nullptr;
	LiveLabel = nullptr;
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

void URammsControlRow::NativeDestruct()
{
	// Torn down (panel rebuild, HUD destroyed) with a hold button down: let
	// the axis go, or the sink keeps it owned by this source.
	if (bHoldActive)
	{
		ReleaseAxis();
	}
	Super::NativeDestruct();
}

void URammsControlRow::Setup(UObject* InSink, const FRammsControlAxis& InAxis, ERammsControlSource InSource)
{
	if (bHoldActive)
	{
		ReleaseAxis(); // rebinding while held: release the old control first
	}
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
	EnumValue = nullptr;
	MinusButton = PlusButton = nullptr;

	const FText Label = Axis.DisplayName.IsEmpty() ? FText::FromName(Axis.Id) : Axis.DisplayName;

	if (Axis.Kind == ERammsControlKind::Enum && Axis.EnumLabels.Num() > 0)
	{
		// Label, the active choice, and a pair of steppers -- the same shape as
		// a rate row, so a selector does not look like a different species of
		// control. Clicks step rather than hold: there is nothing to ramp.
		RateLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnumLabel"));
		RateLabel->SetText(Label);
		if (UHorizontalBoxSlot* LabelSlot = RootBox->AddChildToHorizontalBox(RateLabel))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		EnumValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnumValue"));
		EnumValue->SetJustification(ETextJustify::Center);
		USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EnumBox"));
		ValueBox->SetWidthOverride(ValueColumnWidth * 2.0f);
		ValueBox->AddChild(EnumValue);

		// Added before the steppers: MakeHoldButton appends to RootBox as it
		// builds, so constructing the buttons first would lay the row out as
		// label / < / > / value.
		if (UHorizontalBoxSlot* ValueSlot = RootBox->AddChildToHorizontalBox(ValueBox))
		{
			ValueSlot->SetPadding(FMargin(6.0f, 0.0f));
			ValueSlot->SetVerticalAlignment(VAlign_Center);
		}

		// Same steppers the rate row uses, so they are styled and sized alike --
		// but wired to OnClicked, since stepping a choice has nothing to ramp.
		UTextBlock* PrevText = nullptr;
		UTextBlock* NextText = nullptr;
		MinusButton = MakeHoldButton(TEXT("EnumPrev"), TEXT("<"), PrevText);
		PlusButton = MakeHoldButton(TEXT("EnumNext"), TEXT(">"), NextText);
		MinusLabel = PrevText;
		PlusLabel = NextText;
		MinusButton->OnClicked.AddUniqueDynamic(this, &URammsControlRow::OnEnumPrevClicked);
		PlusButton->OnClicked.AddUniqueDynamic(this, &URammsControlRow::OnEnumNextClicked);

		// A read-only selector still shows which choice is live; it just cannot
		// be stepped.
		if (Axis.bReadOnly)
		{
			MinusButton->SetIsEnabled(false);
			PlusButton->SetIsEnabled(false);
		}
		Refresh();
	}
	else if (Axis.IsAction())
	{
		ActionButton = CreateWidget<URammsButton>(this);
		if (Style)
		{
			ActionButton->SetStyle(Style); // nested user widget: outside our tree, styled by hand
		}
		ActionButton->SetText(Label);
		ActionButton->OnClicked.AddUniqueDynamic(this, &URammsControlRow::OnActionClicked);
		UHorizontalBoxSlot* BoxSlot = RootBox->AddChildToHorizontalBox(ActionButton);
		if (BoxSlot)
		{
			BoxSlot->SetPadding(FMargin(0.0f, 2.0f));
		}
	}
	else if (Axis.Kind == ERammsControlKind::Continuous && !Axis.bReadOnly)
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
		RateValue->SetText(FText::FromString(TEXT("+0.00")));
		RateValue->SetJustification(ETextJustify::Right);
		USizeBox* RateBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RateBox"));
		RateBox->SetWidthOverride(ValueColumnWidth);
		RateBox->AddChild(RateValue);
		UHorizontalBoxSlot* ValueSlot = RootBox->AddChildToHorizontalBox(RateBox);
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
	else if (Axis.bReadOnly)
	{
		// Readback-only state: label + live value, nothing to drag.
		RateLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ReadLabel"));
		RateLabel->SetText(Label);
		UHorizontalBoxSlot* LabelSlot = RootBox->AddChildToHorizontalBox(RateLabel);
		if (LabelSlot)
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		RateValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ReadValue"));
		RateValue->SetJustification(ETextJustify::Right);
		USizeBox* ReadBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ReadBox"));
		ReadBox->SetWidthOverride(ValueColumnWidth);
		ReadBox->AddChild(RateValue);
		UHorizontalBoxSlot* ValueSlot = RootBox->AddChildToHorizontalBox(ReadBox);
		if (ValueSlot)
		{
			ValueSlot->SetPadding(FMargin(6.0f, 0.0f));
			ValueSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	else
	{
		// Position / Velocity target. An unbounded axis (non-increasing range:
		// "defer to the backend") still needs slider limits: a display range by
		// units, which only shapes the slider — the sink does not clamp to it.
		AxisControl = CreateWidget<URammsAxisControl>(this);
		if (Style)
		{
			AxisControl->SetStyle(Style); // nested user widget: outside our tree, styled by hand
		}
		FRammsAxisConfig Config;
		Config.Label = Label;
		Config.IconSize = FVector2D::ZeroVector; // no icon: don't reserve the box
		const FVector2D Range = DisplayRangeFor(Axis);
		Config.MinValue = static_cast<float>(Range.X);
		Config.MaxValue = static_cast<float>(Range.Y);
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
		// The slider and its value are the TARGET (set by the user, initialised
		// from the live pose once); this column is the live readback, which
		// keeps moving while the motor gets there.
		if (Axis.bReadback)
		{
			UVerticalBox* Live = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LiveColumn"));
			LiveLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LiveLabel"));
			LiveLabel->SetText(NSLOCTEXT("RammsUI", "LiveValueLabel", "live"));
			LiveLabel->SetJustification(ETextJustify::Right);
			LiveValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LiveValue"));
			LiveValue->SetJustification(ETextJustify::Right);
			Live->AddChildToVerticalBox(LiveLabel);
			Live->AddChildToVerticalBox(LiveValue);
			// Fixed width: the number's text changes every refresh (sign flips
			// around zero, digit counts), and a changing desired width would
			// re-layout the row, the group and the panel each time.
			USizeBox* LiveBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LiveBox"));
			LiveBox->SetWidthOverride(ValueColumnWidth);
			LiveBox->AddChild(Live);
			UHorizontalBoxSlot* LiveSlot = RootBox->AddChildToHorizontalBox(LiveBox);
			if (LiveSlot)
			{
				LiveSlot->SetPadding(FMargin(6.0f, 0.0f, 2.0f, 0.0f));
				LiveSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
		bHasTarget = false;
		bTargetSeeded = false;
	}
	ApplyStyle();
	RefreshInternal(/*bPeriodic*/ false);
}

void URammsControlRow::Refresh()
{
	RefreshInternal(/*bPeriodic*/ true);
}

void URammsControlRow::RefreshInternal(bool bPeriodic)
{
	if (!Sink || !Axis.bReadback || Axis.IsAction())
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const float	 Value = IRammsControlSink::Execute_GetAxisValue(Sink, Axis.Id);
	if (EnumValue)
	{
		// The value IS the index, so the row always shows whatever is actually
		// live -- including a change made by something other than this row.
		const int32 Index = FMath::Clamp(FMath::RoundToInt(Value), 0, Axis.EnumLabels.Num() - 1);
		EnumValue->SetText(Axis.EnumLabels.IsValidIndex(Index) ? Axis.EnumLabels[Index] : FText::GetEmpty());
		return;
	}
	if (Axis.bReadOnly && RateValue)
	{
		RateValue->SetText(FText::FromString(FString::Printf(TEXT("%+.*f %s"), Decimals(), Value, *UnitsText().ToString())));
	}
	else if (AxisControl)
	{
		// The slider is the target: seeded once from the live pose (on the first
		// periodic refresh, after the base has real readings — not at build
		// time, when it still reads zero) and then only moved by the user,
		// while the live column follows the motor. The guard drops any
		// OnValueChanged a programmatic update might raise, so readback never
		// becomes a command (that loop would hold the axis and fight others).
		float	   Target = 0.0f;
		const bool bSinkHasTarget = IRammsControlSink::Execute_GetAxisTarget(Sink, Axis.Id, Target);
		if (bSinkHasTarget)
		{
			// Whoever commanded it (this slider, a key, a script, autonomy):
			// the slider shows the surface's target. Not while the user is on
			// it, so a drag isn't yanked back before its command lands.
			if (Now - LastUserInputTime >= ReadbackHoldOff)
			{
				bRefreshing = true;
				AxisControl->SetValue(Target);
				bRefreshing = false;
			}
			bTargetSeeded = true;
		}
		else if (!bTargetSeeded && bPeriodic)
		{
			bRefreshing = true;
			AxisControl->SetValue(Value);
			bRefreshing = false;
			bTargetSeeded = true;
		}
		if (LiveValue)
		{
			LiveValue->SetText(FText::FromString(FString::Printf(TEXT("%+.*f %s"), Decimals(), Value, *UnitsText().ToString())));
		}
	}
	else if (RateValue)
	{
		RateValue->SetText(FText::FromString(FString::Printf(TEXT("%+.2f"), Value)));
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
	bHoldActive = false;
	if (Sink && Sink->GetClass()->ImplementsInterface(URammsControlSink::StaticClass()))
	{
		IRammsControlSink::Execute_ReleaseAxis(Sink, Axis.Id, Source);
	}
}

void URammsControlRow::OnAxisValue(float NewValue)
{
	if (!bRefreshing)
	{
		bHasTarget = true; // the slider is a target from now on
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
	bHoldActive = true;
	SetAxis(HoldValue);
}

void URammsControlRow::OnPlusReleased()
{
	ReleaseAxis();
}

void URammsControlRow::OnMinusPressed()
{
	bHoldActive = true;
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
	if (AxisControl)
	{
		AxisControl->SetStyle(Style);
	}
	if (ActionButton)
	{
		ActionButton->SetStyle(Style);
	}
	if (LiveLabel)
	{
		LiveLabel->SetFont(Style->Typography.Caption);
		LiveLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
	}
	if (LiveValue)
	{
		LiveValue->SetFont(Style->Typography.Monospace); // constant digit widths
		LiveValue->SetColorAndOpacity(FSlateColor(Style->Colors.Info));
	}
	for (UTextBlock* Text : { RateLabel.Get(), MinusLabel.Get(), PlusLabel.Get() })
	{
		if (Text)
		{
			Text->SetFont(Style->Typography.Body);
			Text->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
		}
	}
	if (EnumValue)
	{
		// Words rather than digits, so body type rather than the monospace the
		// numeric fields use; Info because the active choice is the point of
		// the row.
		EnumValue->SetFont(Style->Typography.Body);
		EnumValue->SetColorAndOpacity(FSlateColor(Style->Colors.Info));
	}
	if (RateValue)
	{
		RateValue->SetFont(Style->Typography.Monospace); // constant digit widths
		RateValue->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
	}
}

// --- scripted interaction ------------------------------------------------------

void URammsControlRow::SimulateValue(float Value)
{
	bHasTarget = true;
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
	if (bPlus)
	{
		OnPlusPressed();
	}
	else
	{
		OnMinusPressed();
	}
}

void URammsControlRow::SimulateRelease()
{
	ReleaseAxis();
}

float URammsControlRow::GetTargetValue() const
{
	return AxisControl ? AxisControl->GetValue() : 0.0f;
}

void URammsControlRow::StepEnum(int32 Delta)
{
	if (!Sink || Axis.EnumLabels.Num() == 0)
	{
		return;
	}
	const int32 Count = Axis.EnumLabels.Num();
	// Step from what was last commanded, falling back to the readback when
	// nothing has been. Stepping from the readback loses a click whenever the
	// choice is applied asynchronously: both clicks read the old index and
	// send the same next value, so the second one does nothing.
	float From = 0.0f;
	if (!IRammsControlSink::Execute_GetAxisTarget(Sink, Axis.Id, From))
	{
		From = IRammsControlSink::Execute_GetAxisValue(Sink, Axis.Id);
	}
	const int32 Current = FMath::Clamp(FMath::RoundToInt(From), 0, Count - 1);
	// Wrap: with two modes a one-way stepper would need the user to know which
	// end they were at.
	const int32 Next = ((Current + Delta) % Count + Count) % Count;
	IRammsControlSink::Execute_SetAxis(Sink, Axis.Id, static_cast<float>(Next), Source);
	Refresh();
}

void URammsControlRow::OnEnumPrevClicked()
{
	StepEnum(-1);
}

void URammsControlRow::OnEnumNextClicked()
{
	StepEnum(1);
}

FVector2D URammsControlRow::DisplayRangeFor(const FRammsControlAxis& Axis)
{
	if (Axis.Range.X < Axis.Range.Y)
	{
		return Axis.Range;
	}
	switch (Axis.Units)
	{
		case ERammsControlUnits::Radians:
			return FVector2D(-PI, PI);
		case ERammsControlUnits::Degrees:
			return FVector2D(-180.0, 180.0);
		case ERammsControlUnits::Centimeters:
			return FVector2D(-100.0, 100.0);
		case ERammsControlUnits::Meters:
			return FVector2D(-1.0, 1.0);
		case ERammsControlUnits::RadiansPerSecond:
		case ERammsControlUnits::CentimetersPerSecond:
			return FVector2D(-10.0, 10.0);
		default:
			return FVector2D(-1.0, 1.0);
	}
}
