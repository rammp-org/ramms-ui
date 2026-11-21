// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsControlRow.h"
#include "UI/RammsAxisControl.h"
#include "UI/RammsButton.h"
#include "UI/RammsUIStyle.h"
#include "RammsControlSink.h"
#include "RammsControlHUDSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

FText URammsControlRow::SimLabel(const FText& Label)
{
	// A name with no capital in it is an identifier some table supplied ("dw main plate l"), not one written
	// for a reader: give it a capital and spell out a trailing side letter. A name someone wrote is left alone.
	FString Name = Label.ToString();
	if (Name.IsEmpty() || !Name.Equals(Name.ToLower(), ESearchCase::CaseSensitive))
	{
		return Label;
	}
	if (Name.EndsWith(TEXT(" l"), ESearchCase::CaseSensitive))
	{
		Name = Name.LeftChop(2) + TEXT(" (L)");
	}
	else if (Name.EndsWith(TEXT(" r"), ESearchCase::CaseSensitive))
	{
		Name = Name.LeftChop(2) + TEXT(" (R)");
	}
	Name[0] = FChar::ToUpper(Name[0]);
	return FText::FromString(Name);
}

void URammsControlRowChoice::HandleClicked()
{
	if (URammsControlRow* Target = Row.Get())
	{
		Target->SelectEnum(Index);
	}
}

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
	// Every cached widget, as RammsBaseWidget asks: after the tree is invalidated a refresh or a restyle would
	// otherwise reach a detached one.
	RootBox = nullptr;
	AxisControl = nullptr;
	ActionButton = nullptr;
	EnumValue = nullptr;
	TextValue = nullptr;
	Choices.Reset();
	SegmentBacks.Reset();
	SegmentLabels.Reset();
	SegmentFrame = nullptr;
	ToggleTrack = ToggleKnob = nullptr;
	ShownEnumIndex = -1;
	RateLabel = RateValue = LiveLabel = LiveValue = MinusLabel = PlusLabel = nullptr;
	MinusButton = PlusButton = nullptr;
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
	Box->SetWidthOverride(bSimRow ? 34.0f : 44.0f);
	Box->SetHeightOverride(bSimRow ? 34.0f : 36.0f);
	Box->AddChild(Button);
	UHorizontalBoxSlot* BoxSlot = RootBox->AddChildToHorizontalBox(Box);
	if (BoxSlot)
	{
		BoxSlot->SetPadding(FMargin(bSimRow ? 6.0f : 4.0f, 0.0f, 0.0f, 0.0f));
		BoxSlot->SetVerticalAlignment(VAlign_Center);
	}
	return Button;
}

void URammsControlRow::StyleStepButton(UButton* Button) const
{
	if (!Button || !Style || !bSimRow)
	{
		return;
	}
	// The engine's default button is light grey, which white marks do not read on. A control's own surface
	// with an outline, like every other button of the look.
	const float	 R = Style->Border.CornerRadiusMedium;
	FButtonStyle ButtonStyle;
	ButtonStyle.SetNormal(FSlateRoundedBoxBrush(Style->Colors.Secondary, R, Style->Colors.Border, Style->Border.BorderWidth));
	ButtonStyle.SetHovered(
		FSlateRoundedBoxBrush(URammsUIStyle::Lighten(Style->Colors.Secondary, 0.08f), R, Style->Colors.Border, Style->Border.BorderWidth));
	ButtonStyle.SetPressed(FSlateRoundedBoxBrush(Style->Colors.Primary, R));
	ButtonStyle.SetDisabled(FSlateRoundedBoxBrush(FLinearColor::Transparent, R, Style->Colors.Border, Style->Border.BorderWidth));
	ButtonStyle.SetNormalPadding(FMargin(0.0f));
	ButtonStyle.SetPressedPadding(FMargin(0.0f));
	Button->SetStyle(ButtonStyle);
}

void URammsControlRow::BuildChoiceRow(const FText& Label)
{
	const int32 Count = Axis.EnumLabels.Num();
	const auto	IsWord = [this](int32 I, const TCHAR* Word) {
		 return Axis.EnumLabels[I].ToString().TrimStartAndEnd().Equals(Word, ESearchCase::IgnoreCase);
	};
	RateLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnumLabel"));
	RateLabel->SetText(Label);
	RateLabel->SetAutoWrapText(true);
	RateLabel->SetClipping(EWidgetClipping::ClipToBounds);
	if (!Axis.bReadback)
	{
		// Nothing will ever read the choice back: show the one the control starts at, then whatever is picked.
		ShownEnumIndex = FMath::Clamp(FMath::RoundToInt(Axis.DefaultValue), 0, Count - 1);
	}

	if (Count == 2 && IsWord(0, TEXT("Off")) && IsWord(1, TEXT("On")))
	{
		// Off / On: a switch. The label says what it switches; the switch says which way it is.
		if (UHorizontalBoxSlot* LabelSlot = RootBox->AddChildToHorizontalBox(RateLabel))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		UButton* Toggle = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Toggle"));
		Toggle->SetStyle(URammsUIStyle::MakeTransparentButtonStyle());
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ToggleBox"));
		Box->SetWidthOverride(42.0f);
		Box->SetHeightOverride(24.0f);
		UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ToggleStack"));
		ToggleTrack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ToggleTrack"));
		ToggleTrack->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* TrackSlot = Stack->AddChildToOverlay(ToggleTrack))
		{
			TrackSlot->SetHorizontalAlignment(HAlign_Fill);
			TrackSlot->SetVerticalAlignment(VAlign_Fill);
		}
		ToggleKnob = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ToggleKnob"));
		ToggleKnob->SetVisibility(ESlateVisibility::HitTestInvisible);
		ToggleKnob->SetPadding(FMargin(9.0f)); // a border has no size of its own: 18 across
		if (UOverlaySlot* KnobSlot = Stack->AddChildToOverlay(ToggleKnob))
		{
			KnobSlot->SetHorizontalAlignment(HAlign_Left);
			KnobSlot->SetVerticalAlignment(VAlign_Center);
			KnobSlot->SetPadding(FMargin(3.0f, 0.0f));
		}
		Box->AddChild(Stack);
		Toggle->AddChild(Box);
		Toggle->OnClicked.AddUniqueDynamic(this, &URammsControlRow::OnToggleClicked);
		if (UHorizontalBoxSlot* ToggleSlot = RootBox->AddChildToHorizontalBox(Toggle))
		{
			ToggleSlot->SetPadding(FMargin(6.0f, 5.0f, 0.0f, 5.0f));
			ToggleSlot->SetVerticalAlignment(VAlign_Center);
		}
		return;
	}

	// Every choice on show, the active one filled: one click to any of them, and no stepping round to find
	// out what the others are.
	int32 Chars = 0;
	for (const FText& Choice : Axis.EnumLabels)
	{
		Chars += Choice.ToString().Len();
	}
	SegmentFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Segments"));
	SegmentFrame->SetPadding(FMargin(2.0f));
	UHorizontalBox* Segments = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SegmentRow"));
	SegmentFrame->AddChild(Segments);
	for (int32 I = 0; I < Count; ++I)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetStyle(URammsUIStyle::MakeTransparentButtonStyle());
		UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Back->SetPadding(FMargin(4.0f, 7.0f));
		Back->SetHorizontalAlignment(HAlign_Fill); // the label's cell is the segment's width, so it can wrap in it
		Back->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(Axis.EnumLabels[I]);
		Text->SetJustification(ETextJustify::Center);
		// Like every label of a row: wrapped in its cell and clipped to it, never drawn over the next choice.
		Text->SetAutoWrapText(true);
		Text->SetClipping(EWidgetClipping::ClipToBounds);
		Back->AddChild(Text);
		Button->AddChild(Back);
		if (UButtonSlot* InButton = Cast<UButtonSlot>(Back->Slot))
		{
			InButton->SetHorizontalAlignment(HAlign_Fill);
			InButton->SetVerticalAlignment(VAlign_Fill);
			InButton->SetPadding(FMargin(0.0f));
		}
		URammsControlRowChoice* Choice = NewObject<URammsControlRowChoice>(this);
		Choice->Row = this;
		Choice->Index = I;
		Button->OnClicked.AddUniqueDynamic(Choice, &URammsControlRowChoice::HandleClicked);
		if (UHorizontalBoxSlot* SegmentSlot = Segments->AddChildToHorizontalBox(Button))
		{
			SegmentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		Choices.Add(Choice);
		SegmentBacks.Add(Back);
		SegmentLabels.Add(Text);
	}
	if (Count <= 3 && Chars <= 18)
	{
		// Short enough to share a line with its label.
		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EnumLabelBox"));
		LabelBox->SetWidthOverride(120.0f);
		LabelBox->AddChild(RateLabel);
		if (UHorizontalBoxSlot* LabelSlot = RootBox->AddChildToHorizontalBox(LabelBox))
		{
			LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* FrameSlot = RootBox->AddChildToHorizontalBox(SegmentFrame))
		{
			FrameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			FrameSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	else
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EnumColumn"));
		Column->AddChildToVerticalBox(RateLabel);
		if (UVerticalBoxSlot* FrameSlot = Column->AddChildToVerticalBox(SegmentFrame))
		{
			FrameSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
		}
		if (UHorizontalBoxSlot* ColumnSlot = RootBox->AddChildToHorizontalBox(Column))
		{
			ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
}

void URammsControlRow::ApplyChoiceState()
{
	if (!Style)
	{
		return;
	}
	const float R = FMath::Max(Style->Border.CornerRadiusMedium - 1.0f, 0.0f);
	for (int32 I = 0; I < SegmentBacks.Num(); ++I)
	{
		const bool bActive = I == ShownEnumIndex;
		if (SegmentBacks[I])
		{
			URammsUIStyle::ApplyRoundedBrushToBorder(SegmentBacks[I],
				URammsUIStyle::MakeRoundedBoxBrush(bActive ? Style->Colors.Primary : FLinearColor::Transparent, R));
		}
		if (SegmentLabels.IsValidIndex(I) && SegmentLabels[I])
		{
			SegmentLabels[I]->SetColorAndOpacity(FSlateColor(bActive ? FLinearColor::White : Style->Colors.TextSecondary));
		}
	}
	if (ToggleTrack && ToggleKnob)
	{
		const bool bOn = ShownEnumIndex == 1;
		URammsUIStyle::ApplyRoundedBrushToBorder(ToggleTrack,
			URammsUIStyle::MakeRoundedBoxBrush(bOn ? Style->Colors.Primary : Style->Slider.TrackColor, 12.0f));
		URammsUIStyle::ApplyRoundedBrushToBorder(ToggleKnob, URammsUIStyle::MakeRoundedBoxBrush(FLinearColor::White, 9.0f));
		if (UOverlaySlot* KnobSlot = Cast<UOverlaySlot>(ToggleKnob->Slot))
		{
			KnobSlot->SetHorizontalAlignment(bOn ? HAlign_Right : HAlign_Left);
		}
	}
}

void URammsControlRow::SelectEnum(int32 Index)
{
	if (!Sink || Axis.Kind != ERammsControlKind::Enum || Axis.bReadOnly || !Axis.EnumLabels.IsValidIndex(Index))
	{
		return;
	}
	LastUserInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	IRammsControlSink::Execute_SetAxis(Sink, Axis.Id, static_cast<float>(Index), Source);
	// Shown at once; the readback then confirms it, or puts back whatever the robot kept.
	ShownEnumIndex = Index;
	ApplyChoiceState();
}

void URammsControlRow::OnToggleClicked()
{
	// The other of the two, by the same path as a segment: shown at once, and held for a moment against a
	// readback that has not caught up.
	SelectEnum(ShownEnumIndex == 1 ? 0 : 1);
}

FText URammsControlRow::GetTextWords() const
{
	return TextValue ? TextValue->GetText() : FText::GetEmpty();
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
	TextValue = nullptr;
	RateLabel = RateValue = LiveLabel = LiveValue = MinusLabel = PlusLabel = nullptr;
	MinusButton = PlusButton = nullptr;
	TextSeverity = 0;
	Choices.Reset();
	SegmentBacks.Reset();
	SegmentLabels.Reset();
	SegmentFrame = nullptr;
	ToggleTrack = ToggleKnob = nullptr;
	ShownEnumIndex = -1;
	// The look is fixed here, with the row's parts: a theme that changes it later needs the row set up again
	// (the panel rebuilds its rows when its theme's look changes).
	bSimRow = URammsUIStyle::UsesSimLook(Style);

	const FText RawLabel = Axis.DisplayName.IsEmpty() ? FText::FromName(Axis.Id) : Axis.DisplayName;
	const FText Label = bSimRow ? SimLabel(RawLabel) : RawLabel;
	// Words wrap inside the space their slot gives them and never draw past it: a
	// long label or choice makes its row taller instead of running over the value,
	// the buttons or the next row.
	const auto Contain = [](UTextBlock* Text) {
		Text->SetAutoWrapText(true);
		Text->SetClipping(EWidgetClipping::ClipToBounds);
	};

	if (Axis.IsText())
	{
		// A line the robot reports (a status): its label over it, the words across
		// the row's whole width.
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TextColumn"));
		RateLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TextLabel"));
		RateLabel->SetText(Label);
		Contain(RateLabel);
		TextValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TextValue"));
		Contain(TextValue);
		Column->AddChildToVerticalBox(RateLabel);
		if (UVerticalBoxSlot* WordsSlot = Column->AddChildToVerticalBox(TextValue))
		{
			WordsSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
		}
		if (UHorizontalBoxSlot* ColumnSlot = RootBox->AddChildToHorizontalBox(Column))
		{
			ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ColumnSlot->SetPadding(FMargin(0.0f, 2.0f));
		}
	}
	else if (bSimRow && Axis.Kind == ERammsControlKind::Enum && !Axis.bReadOnly && Axis.EnumLabels.Num() >= 2 && Axis.EnumLabels.Num() <= 4)
	{
		BuildChoiceRow(Label);
	}
	else if (Axis.Kind == ERammsControlKind::Enum && Axis.EnumLabels.Num() > 0)
	{
		// Label, the active choice, and a pair of steppers -- the same shape as
		// a rate row, so a selector does not look like a different species of
		// control. Clicks step rather than hold: there is nothing to ramp.
		RateLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnumLabel"));
		RateLabel->SetText(Label);
		Contain(RateLabel);
		if (UHorizontalBoxSlot* LabelSlot = RootBox->AddChildToHorizontalBox(RateLabel))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		EnumValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnumValue"));
		Contain(EnumValue);
		if (Axis.bReadOnly || (bSimRow && Axis.EnumLabels.Num() < 2))
		{
			// A state to show (curb.phase), not a choice to make: no steppers that
			// can never be pressed, and the words share the row with the label
			// rather than squeezing into a stepper's column. In the Sim look the
			// same goes for a choice of one (the only robot there is to start).
			EnumValue->SetJustification(ETextJustify::Right);
			if (UHorizontalBoxSlot* ValueSlot = RootBox->AddChildToHorizontalBox(EnumValue))
			{
				ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				ValueSlot->SetPadding(FMargin(6.0f, 0.0f, 2.0f, 0.0f));
				ValueSlot->SetVerticalAlignment(VAlign_Center);
			}
			ApplyStyle();
			RefreshInternal(/*bPeriodic*/ false);
			return;
		}
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
		if (bSimRow)
		{
			// As wide as the place the panel gives it (actions sit side by side), and one of three weights:
			// what the settings name a group's main action is filled, what they name a stop or a reset is red.
			if (BoxSlot)
			{
				BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
			ActionButton->SetContentPadding(FMargin(10.0f, 9.0f));
			const URammsControlHUDSettings* Settings = GetDefault<URammsControlHUDSettings>();
			ActionButton->SetVariant(URammsControlHUDSettings::MatchesAny(Settings->DangerControls, Axis.Id) ? ERammsButtonVariant::Error
					: URammsControlHUDSettings::MatchesAny(Settings->PrimaryControls, Axis.Id)				 ? ERammsButtonVariant::Primary
																											 : ERammsButtonVariant::Secondary);
		}
	}
	else if (Axis.Kind == ERammsControlKind::Continuous && !Axis.bReadOnly)
	{
		// Rate axis: label, live value, hold buttons.
		RateLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RateLabel"));
		RateLabel->SetText(Label);
		Contain(RateLabel);
		UHorizontalBoxSlot* LabelSlot = RootBox->AddChildToHorizontalBox(RateLabel);
		if (LabelSlot)
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		RateValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RateValue"));
		RateValue->SetText(FText::FromString(TEXT("+0.00")));
		RateValue->SetJustification(ETextJustify::Right);
		Contain(RateValue); // a value longer than its column goes onto a second line, not over the buttons
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
		Contain(RateLabel);
		UHorizontalBoxSlot* LabelSlot = RootBox->AddChildToHorizontalBox(RateLabel);
		if (LabelSlot)
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		RateValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ReadValue"));
		RateValue->SetJustification(ETextJustify::Right);
		Contain(RateValue);
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
		// (Not in the Sim look: there the live reading is a mark on the slider's own track.)
		if (Axis.bReadback && !bSimRow)
		{
			UVerticalBox* Live = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LiveColumn"));
			LiveLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LiveLabel"));
			LiveLabel->SetText(NSLOCTEXT("RammsUI", "LiveValueLabel", "live"));
			LiveLabel->SetJustification(ETextJustify::Right);
			LiveValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LiveValue"));
			LiveValue->SetJustification(ETextJustify::Right);
			Contain(LiveLabel);
			Contain(LiveValue);
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
	if (TextValue)
	{
		FText Words;
		IRammsControlSink::Execute_GetAxisText(Sink, Axis.Id, Words);
		if (!Words.EqualTo(TextValue->GetText()))
		{
			TextValue->SetText(Words);
		}
		const uint8 Severity = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Value), 0, static_cast<int32>(ERammsControlTextSeverity::Fault)));
		if (Severity != TextSeverity)
		{
			TextSeverity = Severity;
			ApplyTextColour();
		}
		return;
	}
	if (SegmentBacks.Num() > 0 || ToggleTrack)
	{
		// Left alone for a moment after a pick, so a choice applied a tick later is not flicked back first.
		const int32 Index = FMath::Clamp(FMath::RoundToInt(Value), 0, Axis.EnumLabels.Num() - 1);
		if (Index != ShownEnumIndex && Now - LastUserInputTime >= ReadbackHoldOff)
		{
			ShownEnumIndex = Index;
			ApplyChoiceState();
		}
		return;
	}
	if (EnumValue)
	{
		// The value IS the index, so the row always shows whatever is actually
		// live -- including a change made by something other than this row.
		const int32 Index = FMath::Clamp(FMath::RoundToInt(Value), 0, Axis.EnumLabels.Num() - 1);
		ShownEnumIndex = Index;
		EnumValue->SetText(Axis.EnumLabels.IsValidIndex(Index) ? Axis.EnumLabels[Index] : FText::GetEmpty());
		return;
	}
	if (Axis.bReadOnly && RateValue)
	{
		if (bSimRow && Axis.Units == ERammsControlUnits::Normalized && FMath::IsNearlyEqual(Axis.Range.X, 0.0) && FMath::IsNearlyEqual(Axis.Range.Y, 1.0))
		{
			// A 0..1 reading with no units is a state ("Closed", "Stowed"), not a quantity: say which, and
			// only give the number while it is in between.
			const bool bYes = Value >= 0.99f;
			const bool bNo = Value <= 0.01f;
			RateValue->SetText(bYes ? NSLOCTEXT("RammsUI", "StateYes", "\u25CF Yes")
					: bNo			? NSLOCTEXT("RammsUI", "StateNo", "\u25CB No")
									: FText::FromString(FString::Printf(TEXT("%.0f%%"), Value * 100.0f)));
			if (Style)
			{
				RateValue->SetColorAndOpacity(FSlateColor(bYes ? Style->Colors.Success : Style->Colors.TextSecondary));
			}
			return;
		}
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
		if (bSimRow)
		{
			AxisControl->SetLiveValue(Value);
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
	if (Style->IsSimLook() != bSimRow && Sink && RootBox)
	{
		// A theme of the other look after the row was set up: its parts are that look's, so set it up again.
		// (Setup ends by applying the style, with the looks then agreeing.)
		Setup(Sink, Axis, Source);
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
	if (bSimRow)
	{
		StyleStepButton(MinusButton);
		StyleStepButton(PlusButton);
		if (EnumValue)
		{
			// A state among the rows, not the one accent on the panel: plain text.
			EnumValue->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
		}
		if (SegmentFrame)
		{
			URammsUIStyle::ApplyRoundedBrushToBorder(SegmentFrame, URammsUIStyle::MakeRoundedBoxBrush(Style->Colors.Secondary, Style->Border.CornerRadiusMedium, Style->Colors.Border, Style->Border.BorderWidth));
		}
		for (UTextBlock* Text : SegmentLabels)
		{
			if (Text)
			{
				Text->SetFont(Style->Typography.Caption);
			}
		}
		ApplyChoiceState();
	}
	if (TextValue)
	{
		// A status: its label a caption over the words, which are the point.
		if (RateLabel)
		{
			RateLabel->SetFont(Style->Typography.Caption);
			RateLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
		}
		TextValue->SetFont(Style->Typography.Body);
		ApplyTextColour();
	}
}

void URammsControlRow::ApplyTextColour()
{
	if (!TextValue || !Style)
	{
		return;
	}
	const ERammsControlTextSeverity Severity = static_cast<ERammsControlTextSeverity>(TextSeverity);
	const FLinearColor				Colour = Severity == ERammsControlTextSeverity::Fault ? Style->Colors.Error
					 : Severity == ERammsControlTextSeverity::Attention					  ? Style->Colors.Warning
																						  : Style->Colors.TextPrimary;
	TextValue->SetColorAndOpacity(FSlateColor(Colour));
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
