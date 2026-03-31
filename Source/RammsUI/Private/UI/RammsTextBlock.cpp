// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsTextBlock.h"
#include "Blueprint/WidgetTree.h"

void URammsTextBlock::ResetCachedWidgets()
{
	InnerText = nullptr;
}

void URammsTextBlock::BuildWidgetTree()
{
	if (!WidgetTree || InnerText)
		return;

	InnerText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("InnerText"));
	WidgetTree->RootWidget = InnerText;

	InnerText->SetText(Text);
	InnerText->SetJustification(Justification);
	InnerText->SetAutoWrapText(bAutoWrapText);
}

void URammsTextBlock::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (InnerText)
	{
		InnerText->SetText(Text);
		InnerText->SetJustification(Justification);
		InnerText->SetAutoWrapText(bAutoWrapText);
	}

	// Re-apply style so variant/color changes are reflected in designer
	if (Style)
	{
		ApplyStyle();
	}
}

void URammsTextBlock::ApplyStyle_Implementation()
{
	if (!Style || !InnerText)
		return;

	InnerText->SetFont(GetFontForVariant());
	InnerText->SetColorAndOpacity(FSlateColor(GetColorForMode()));
}

// ── Public API ───────────────────────────────────────────────────────

void URammsTextBlock::SetText(FText InText)
{
	Text = InText;
	if (InnerText)
	{
		InnerText->SetText(Text);
	}
}

void URammsTextBlock::SetVariant(ERammsTextVariant InVariant)
{
	Variant = InVariant;
	if (Style && InnerText)
	{
		InnerText->SetFont(GetFontForVariant());
	}
}

void URammsTextBlock::SetTextColorMode(ERammsTextColor InMode)
{
	TextColorMode = InMode;
	if (Style && InnerText)
	{
		InnerText->SetColorAndOpacity(FSlateColor(GetColorForMode()));
	}
}

void URammsTextBlock::SetColorOverride(FLinearColor InColor)
{
	ColorOverride = InColor;
	if (TextColorMode == ERammsTextColor::Custom && InnerText)
	{
		InnerText->SetColorAndOpacity(FSlateColor(ColorOverride));
	}
}

// ── Private helpers ──────────────────────────────────────────────────

FSlateFontInfo URammsTextBlock::GetFontForVariant() const
{
	if (!Style)
		return FSlateFontInfo();

	switch (Variant)
	{
		case ERammsTextVariant::HeadingLarge:
			return Style->Typography.HeadingLarge;
		case ERammsTextVariant::HeadingMedium:
			return Style->Typography.HeadingMedium;
		case ERammsTextVariant::HeadingSmall:
			return Style->Typography.HeadingSmall;
		case ERammsTextVariant::Body:
			return Style->Typography.Body;
		case ERammsTextVariant::Caption:
			return Style->Typography.Caption;
		case ERammsTextVariant::Monospace:
			return Style->Typography.Monospace;
		default:
			return Style->Typography.Body;
	}
}

FLinearColor URammsTextBlock::GetColorForMode() const
{
	if (!Style)
		return FLinearColor::White;

	switch (TextColorMode)
	{
		case ERammsTextColor::Primary:
			return Style->Colors.TextPrimary;
		case ERammsTextColor::Secondary:
			return Style->Colors.TextSecondary;
		case ERammsTextColor::Disabled:
			return Style->Colors.TextDisabled;
		case ERammsTextColor::Accent:
			return Style->Colors.Primary;
		case ERammsTextColor::Success:
			return Style->Colors.Success;
		case ERammsTextColor::Warning:
			return Style->Colors.Warning;
		case ERammsTextColor::Error:
			return Style->Colors.Error;
		case ERammsTextColor::Custom:
			return ColorOverride;
		default:
			return Style->Colors.TextPrimary;
	}
}
