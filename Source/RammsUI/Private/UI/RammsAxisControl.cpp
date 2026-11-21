// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsAxisControl.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Spacer.h"
#include "UI/RammsUIStyle.h"

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
	DecrementSizeBox = nullptr;
	IncrementSizeBox = nullptr;
	RightVBox = nullptr;
	InfoRow = nullptr;
	LabelSizeBox = nullptr;
	ValueSizeBox = nullptr;
	SliderOverlay = nullptr;
	TrackBar = NegFill = PosFill = LiveMark = nullptr;
	FillRow = LiveRow = nullptr;
	LiveBefore = LiveAfter = nullptr;
}

void URammsAxisControl::BuildWidgetTree()
{
	if (!WidgetTree || OuterHBox)
		return;

	// The widgets are made here and placed by ArrangeLayout, which has two arrangements of the same parts:
	//   classic: HBox(Icon, VBox(HBox(Label, Value, Reset), HBox([−Btn], [Slider], [+Btn])))
	//   compact: HBox(Icon, Label, [−Btn], Overlay(track, fill, live mark, Slider), [+Btn], Reset(Value))

	OuterHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("OuterHBox"));
	WidgetTree->RootWidget = OuterHBox;

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

	RightVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("RightVBox"));
	InfoRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("InfoRow"));

	// Label
	LabelText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Label"));
	LabelText->SetText(Config.Label);
	LabelText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	// A long name wraps in the space its slot gives it, rather than drawing over the value and the reset button.
	LabelText->SetAutoWrapText(true);
	LabelText->SetClipping(EWidgetClipping::ClipToBounds);

	// Value display
	ValueSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("ValBox"));
	ValueSizeBox->SetMinDesiredWidth(56.0f);

	ValueText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Value"));
	ValueText->SetText(FText::FromString(TEXT("0.0")));
	ValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ValueText->SetJustification(ETextJustify::Center);
	ValueText->SetClipping(EWidgetClipping::ClipToBounds); // its box grows to fit; clipped all the same

	// Reset button
	ResetButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("ResetBtn"));

	ResetLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ResetLbl"));
	ResetLabel->SetText(FText::FromString(TEXT("\u21BA"))); // ↺
	ResetLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	ControlRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("ControlRow"));

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

	// Slider
	InnerSlider = WidgetTree->ConstructWidget<USlider>(
		USlider::StaticClass(), TEXT("Slider"));
	InnerSlider->SetMinValue(Config.MinValue);
	InnerSlider->SetMaxValue(Config.MaxValue);
	InnerSlider->SetValue(Config.DefaultValue);
	InnerSlider->SetStepSize(GetNormalizedStepSize());
	InnerSlider->SetSliderBarColor(FLinearColor(0.2f, 0.2f, 0.25f, 1.0f));
	InnerSlider->SetSliderHandleColor(FLinearColor(0.6f, 0.6f, 0.7f, 1.0f));
	// The engine's own choice, kept for the classic arrangement to put back: the compact one turns it off.
	bClassicIndentHandle = InnerSlider->HasIndentHandle();

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

	CurrentValue = Config.DefaultValue;
	ArrangeLayout();
}

void URammsAxisControl::EnsureCompactParts()
{
	if (SliderOverlay || !WidgetTree)
	{
		return;
	}
	// Made the first time the compact arrangement is asked for, so a control that never leaves the classic
	// one carries none of them.
	LabelSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LabelBox"));

	// What the slider sits on. The engine's slider tints one bar and has no filled
	// part, so the track, the fill up to the thumb and the live mark are drawn here, under a slider whose
	// own bar is clear.
	SliderOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SliderOverlay"));
	const auto MakeBar = [this](const TCHAR* Name) {
		UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Bar;
	};
	TrackBar = MakeBar(TEXT("TrackBar"));
	NegFill = MakeBar(TEXT("NegFill"));
	PosFill = MakeBar(TEXT("PosFill"));
	LiveMark = MakeBar(TEXT("LiveMark"));
	NegFill->SetRenderTransformPivot(FVector2D(1.0, 0.5)); // grows leftward from the zero point
	PosFill->SetRenderTransformPivot(FVector2D(0.0, 0.5)); // grows rightward from it
	FillRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FillRow"));
	FillRow->SetVisibility(ESlateVisibility::HitTestInvisible);
	LiveRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LiveRow"));
	LiveRow->SetVisibility(ESlateVisibility::HitTestInvisible);
	LiveBefore = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("LiveBefore"));
	LiveAfter = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("LiveAfter"));
}

void URammsAxisControl::ArrangeLayout()
{
	if (!OuterHBox || !RightVBox || !InfoRow || !ControlRow)
	{
		return;
	}
	if (bCompactLayout)
	{
		EnsureCompactParts();
		if (!SliderOverlay)
		{
			return;
		}
	}
	// Take every part out of wherever the other arrangement put it. Parents first would do it in one call each,
	// but a part that moves between parents (the value, the slider) has to be free before it is added again.
	for (UWidget* Part : { (UWidget*)IconSizeBox, (UWidget*)RightVBox, (UWidget*)InfoRow, (UWidget*)ControlRow, (UWidget*)LabelSizeBox,
			 (UWidget*)LabelText, (UWidget*)ValueSizeBox, (UWidget*)ValueText, (UWidget*)ResetButton, (UWidget*)ResetLabel,
			 (UWidget*)DecrementSizeBox, (UWidget*)IncrementSizeBox, (UWidget*)InnerSlider, (UWidget*)SliderOverlay, (UWidget*)TrackBar,
			 (UWidget*)FillRow, (UWidget*)NegFill, (UWidget*)PosFill, (UWidget*)LiveRow, (UWidget*)LiveBefore, (UWidget*)LiveMark,
			 (UWidget*)LiveAfter })
	{
		if (Part)
		{
			Part->RemoveFromParent();
		}
	}

	if (UHorizontalBoxSlot* IconSlot = OuterHBox->AddChildToHorizontalBox(IconSizeBox))
	{
		IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetHorizontalAlignment(HAlign_Left);
	}

	if (!bCompactLayout)
	{
		// ── Classic: info row (Label + Value + Reset) over control row ([−Btn] [Slider] [+Btn]) ──
		if (UHorizontalBoxSlot* RightSlot = OuterHBox->AddChildToHorizontalBox(RightVBox))
		{
			RightSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			RightSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* InfoSlot = RightVBox->AddChildToVerticalBox(InfoRow))
		{
			InfoSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		if (UHorizontalBoxSlot* LabelSlot = InfoRow->AddChildToHorizontalBox(LabelText))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		ValueText->SetJustification(ETextJustify::Center);
		ValueSizeBox->AddChild(ValueText);
		ValueSizeBox->SetMinDesiredWidth(56.0f);
		ValueSizeBox->ClearWidthOverride();
		if (UHorizontalBoxSlot* ValSlot = InfoRow->AddChildToHorizontalBox(ValueSizeBox))
		{
			ValSlot->SetPadding(FMargin(4.0f, 0.0f, 8.0f, 0.0f));
			ValSlot->SetVerticalAlignment(VAlign_Center);
			ValSlot->SetHorizontalAlignment(HAlign_Center);
		}
		ResetButton->AddChild(ResetLabel);
		if (bCompactTipSet)
		{
			ResetButton->SetToolTipText(FText::GetEmpty()); // only ever set by the compact arrangement
			bCompactTipSet = false;
		}
		if (UHorizontalBoxSlot* ResetSlot = InfoRow->AddChildToHorizontalBox(ResetButton))
		{
			ResetSlot->SetPadding(FMargin(0.0f));
			ResetSlot->SetVerticalAlignment(VAlign_Center);
			ResetSlot->SetHorizontalAlignment(HAlign_Center);
		}
		if (UVerticalBoxSlot* CtrlSlot = RightVBox->AddChildToVerticalBox(ControlRow))
		{
			CtrlSlot->SetHorizontalAlignment(HAlign_Fill);
			CtrlSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
		}
		if (UHorizontalBoxSlot* DecSlot = ControlRow->AddChildToHorizontalBox(DecrementSizeBox))
		{
			DecSlot->SetVerticalAlignment(VAlign_Center);
			DecSlot->SetHorizontalAlignment(HAlign_Center);
			DecSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
		}
		InnerSlider->SetIndentHandle(bClassicIndentHandle);
		if (UHorizontalBoxSlot* SliderSlot = ControlRow->AddChildToHorizontalBox(InnerSlider))
		{
			SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			SliderSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* IncSlot = ControlRow->AddChildToHorizontalBox(IncrementSizeBox))
		{
			IncSlot->SetVerticalAlignment(VAlign_Center);
			IncSlot->SetHorizontalAlignment(HAlign_Center);
			IncSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
		}
		return;
	}

	// ── Compact: Label, [−Btn], slider on its track, [+Btn], Value (click to reset) ──
	LabelSizeBox->SetWidthOverride(CompactLabelWidth);
	LabelSizeBox->AddChild(LabelText);
	if (UHorizontalBoxSlot* LabelSlot = OuterHBox->AddChildToHorizontalBox(LabelSizeBox))
	{
		LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* CtrlSlot = OuterHBox->AddChildToHorizontalBox(ControlRow))
	{
		CtrlSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CtrlSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* DecSlot = ControlRow->AddChildToHorizontalBox(DecrementSizeBox))
	{
		DecSlot->SetVerticalAlignment(VAlign_Center);
		DecSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
	}
	// The thumb's centre runs from half a thumb in to half a thumb short of the end (no handle indent), so
	// the track, the fill and the live mark are all inset by that much: a value sits at one place for all four.
	const float Inset = (Style ? Style->Slider.ThumbSize : 18.0f) * 0.5f;
	ArrangedInset = Inset;
	const auto Under = [this, Inset](UWidget* Child) {
		if (UOverlaySlot* S = SliderOverlay->AddChildToOverlay(Child))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(Inset, 0.0f));
		}
	};
	Under(TrackBar);
	Under(FillRow);
	FillRow->AddChildToHorizontalBox(NegFill);
	FillRow->AddChildToHorizontalBox(PosFill);
	Under(LiveRow);
	LiveRow->AddChildToHorizontalBox(LiveBefore);
	if (UHorizontalBoxSlot* MarkSlot = LiveRow->AddChildToHorizontalBox(LiveMark))
	{
		MarkSlot->SetVerticalAlignment(VAlign_Center);
	}
	LiveRow->AddChildToHorizontalBox(LiveAfter);
	InnerSlider->SetIndentHandle(false);
	if (UOverlaySlot* SliderSlot = SliderOverlay->AddChildToOverlay(InnerSlider))
	{
		SliderSlot->SetHorizontalAlignment(HAlign_Fill);
		SliderSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* OverlaySlot = ControlRow->AddChildToHorizontalBox(SliderOverlay))
	{
		OverlaySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		OverlaySlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* IncSlot = ControlRow->AddChildToHorizontalBox(IncrementSizeBox))
	{
		IncSlot->SetVerticalAlignment(VAlign_Center);
		IncSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
	}
	// The value is the reset: a click puts the default back. A button of its own per row was most of a
	// line for something rarely pressed.
	ValueText->SetJustification(ETextJustify::Right);
	ResetButton->AddChild(ValueText);
	if (UButtonSlot* InButton = Cast<UButtonSlot>(ValueText->Slot))
	{
		// Across the button, so the number sits against the right edge whatever its length or sign.
		InButton->SetHorizontalAlignment(HAlign_Fill);
		InButton->SetVerticalAlignment(VAlign_Center);
		InButton->SetPadding(FMargin(0.0f));
	}
	ValueSizeBox->AddChild(ResetButton);
	ValueSizeBox->SetWidthOverride(CompactValueWidth);
	if (UHorizontalBoxSlot* ValSlot = OuterHBox->AddChildToHorizontalBox(ValueSizeBox))
	{
		ValSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
		ValSlot->SetVerticalAlignment(VAlign_Center);
	}
	TrackDrawn = FVector4(-1.0, -1.0, -1.0, -1.0);
	UpdateTrackDecor();
}

void URammsAxisControl::SetCompactLayout(bool bInCompact)
{
	if (bCompactLayout == bInCompact)
	{
		return;
	}
	bCompactLayout = bInCompact;
	ArrangeLayout();
	ApplyConfigToWidgets();
	UpdateValueDisplay();
	UpdateSliderFromValue();
}

void URammsAxisControl::SetLiveValue(float Value)
{
	bHasLiveValue = true;
	LiveValue = Value;
	UpdateTrackDecor();
}

void URammsAxisControl::ClearLiveValue()
{
	bHasLiveValue = false;
	UpdateTrackDecor();
}

void URammsAxisControl::UpdateTrackDecor()
{
	if (!bCompactLayout || !NegFill || !PosFill || !LiveMark || !LiveBefore || !LiveAfter)
	{
		return;
	}
	const float Range = Config.MaxValue - Config.MinValue;
	const float P = Range > KINDA_SMALL_NUMBER ? FMath::Clamp((CurrentValue - Config.MinValue) / Range, 0.0f, 1.0f) : 0.0f;
	// Where the fill starts: zero when the range spans it (so a signed value fills out from the middle), the
	// nearer end otherwise.
	const float Zero = Range > KINDA_SMALL_NUMBER ? FMath::Clamp((0.0f - Config.MinValue) / Range, 0.0f, 1.0f) : 0.0f;
	const float Live = bHasLiveValue && Range > KINDA_SMALL_NUMBER ? FMath::Clamp((LiveValue - Config.MinValue) / Range, 0.0f, 1.0f) : -1.0f;
	const float ShowSlider = Config.bShowSlider ? 1.0f : 0.0f;
	// Only on a change the eye could see: this runs at readback rate for every row.
	const FVector4 Now(P, Zero, Live, ShowSlider);
	if (FMath::Abs(Now.X - TrackDrawn.X) < 0.002 && FMath::Abs(Now.Y - TrackDrawn.Y) < 0.002 && FMath::Abs(Now.Z - TrackDrawn.Z) < 0.002
		&& Now.W == TrackDrawn.W)
	{
		return;
	}
	TrackDrawn = Now;

	const auto Fill = [](UWidget* Widget, float Weight) {
		if (UHorizontalBoxSlot* S = Widget ? Cast<UHorizontalBoxSlot>(Widget->Slot) : nullptr)
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = FMath::Max(Weight, 0.0f);
			S->SetSize(Size);
		}
	};
	const auto Scale = [](UWidget* Bar, float Amount) {
		// A bar scaled to nothing is hidden instead: a zero scale has no inverse for Slate to work with.
		if (Amount < 0.002f)
		{
			Bar->SetVisibility(ESlateVisibility::Hidden);
		}
		else
		{
			Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
			Bar->SetRenderScale(FVector2D(Amount, 1.0));
		}
	};
	Fill(NegFill, Zero);
	Fill(PosFill, 1.0f - Zero);
	Scale(NegFill, Zero > KINDA_SMALL_NUMBER ? (Zero - P) / Zero : 0.0f);
	Scale(PosFill, Zero < 1.0f - KINDA_SMALL_NUMBER ? (P - Zero) / (1.0f - Zero) : 0.0f);

	if (Live < 0.0f)
	{
		LiveMark->SetVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		LiveMark->SetVisibility(ESlateVisibility::HitTestInvisible);
		Fill(LiveBefore, Live);
		Fill(LiveAfter, 1.0f - Live);
	}
	if (SliderOverlay)
	{
		const ESlateVisibility Decor = Config.bShowSlider ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		TrackBar->SetVisibility(Decor);
		FillRow->SetVisibility(Decor);
		LiveRow->SetVisibility(Decor);
	}
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

	// The theme picks the arrangement, each time it is applied: the Sim look is the one-line row.
	SetCompactLayout(Style->IsSimLook());
	if (bCompactLayout && !FMath::IsNearlyEqual(ArrangedInset, Style->Slider.ThumbSize * 0.5f))
	{
		ArrangeLayout(); // a theme with another thumb size: the track's inset follows it
	}

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
	FButtonStyle ResetStyle = BtnStyle;
	if (bCompactLayout && ValueText && InnerSlider && TrackBar && NegFill && PosFill && LiveMark)
	{
		// − and + are a control's own surface with an outline, like any button of the look; the value
		// (the reset) is bare until the pointer is on it.
		const float R = Style->Border.CornerRadiusMedium;
		BtnStyle.SetNormal(FSlateRoundedBoxBrush(Style->Colors.Secondary, R, Style->Colors.Border, Style->Border.BorderWidth));
		BtnStyle.SetHovered(FSlateRoundedBoxBrush(URammsUIStyle::Lighten(Style->Colors.Secondary, 0.08f), R, Style->Colors.Border,
			Style->Border.BorderWidth));
		BtnStyle.SetPressed(FSlateRoundedBoxBrush(Style->Colors.Primary, R));
		BtnStyle.SetNormalPadding(FMargin(0.0f));
		BtnStyle.SetPressedPadding(FMargin(0.0f));
		ResetStyle = URammsUIStyle::MakeTransparentButtonStyle();
		ResetStyle.SetHovered(FSlateRoundedBoxBrush(Style->Colors.Secondary, R));
		ResetStyle.SetPressed(FSlateRoundedBoxBrush(Style->Colors.Primary, R));
		ResetStyle.SetNormalPadding(FMargin(3.0f, 6.0f));
		ResetStyle.SetPressedPadding(FMargin(3.0f, 6.0f));

		ValueText->SetFont(Style->Typography.Monospace); // constant digit widths
		ValueText->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
		InnerSlider->SetSliderBarColor(FLinearColor::Transparent); // the track and fill under it are drawn here
		const float Thick = Style->Slider.BarThickness;
		const auto	Bar = [Thick](UBorder* Border, const FLinearColor& Colour, float Width) {
			 FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrush(Colour, Thick * 0.5f);
			 Brush.SetImageSize(FVector2D(Width, Thick));
			 URammsUIStyle::ApplyRoundedBrushToBorder(Border, Brush);
		};
		Bar(TrackBar, Style->Slider.TrackColor, Thick);
		Bar(NegFill, Style->Slider.ActiveBarColor, Thick);
		Bar(PosFill, Style->Slider.ActiveBarColor, Thick);
		// The live mark: a short upright tick in the warning colour, apart from the accent of the fill.
		FSlateBrush Tick = URammsUIStyle::MakeRoundedBoxBrush(Style->Colors.Warning, 1.0f);
		Tick.SetImageSize(FVector2D(3.0, Thick + 10.0));
		URammsUIStyle::ApplyRoundedBrushToBorder(LiveMark, Tick);
		// A border has no size of its own: give each bar its height (and the tick its width) by padding.
		TrackBar->SetPadding(FMargin(0.0f, Thick * 0.5f));
		NegFill->SetPadding(FMargin(0.0f, Thick * 0.5f));
		PosFill->SetPadding(FMargin(0.0f, Thick * 0.5f));
		LiveMark->SetPadding(FMargin(1.5f, (Thick + 10.0f) * 0.5f));
	}
	if (ResetButton)
	{
		ResetButton->SetStyle(ResetStyle);
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
	if (bCompactLayout && ResetButton)
	{
		// Here, where the default it names is set, not with each new value: a tooltip is a widget to build.
		FNumberFormattingOptions Fmt;
		Fmt.MinimumFractionalDigits = Config.DecimalPlaces;
		Fmt.MaximumFractionalDigits = Config.DecimalPlaces;
		const float Default = FMath::Clamp(Config.DefaultValue, Config.MinValue, Config.MaxValue);
		ResetButton->SetToolTipText(FText::Format(NSLOCTEXT("RammsAxisControl", "ResetTip", "Click to reset to {0}"), FText::AsNumber(Default, &Fmt)));
		bCompactTipSet = true;
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
	UpdateTrackDecor();
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
	UpdateTrackDecor();
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
