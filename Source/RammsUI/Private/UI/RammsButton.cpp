// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsButton.h"
#include "UI/RammsUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/OverlaySlot.h"

void URammsButton::ResetCachedWidgets()
{
	ButtonBorder = nullptr;
	InnerButton = nullptr;
	ButtonLabel = nullptr;
	ButtonIcon = nullptr;
}

void URammsButton::BuildWidgetTree()
{
	if (!WidgetTree || ButtonBorder)
		return;

	// Root: Border — provides rounded corners, clipping, and visual background
	ButtonBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ButtonBorder"));
	ButtonBorder->SetClipping(EWidgetClipping::ClipToBounds);
	WidgetTree->RootWidget = ButtonBorder;

	// Inner: transparent UButton for click/hover/press handling
	InnerButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("InnerButton"));
	ButtonBorder->AddChild(InnerButton);

	// Make button fully transparent — Border handles all visual rendering
	FButtonStyle TransparentStyle;
	TransparentStyle.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
	TransparentStyle.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
	TransparentStyle.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
	TransparentStyle.Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
	InnerButton->SetStyle(TransparentStyle);

	// Content layout: HBox → Icon + Label
	UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonHBox"));
	InnerButton->AddChild(HBox);
	if (UButtonSlot* BtnSlot = Cast<UButtonSlot>(HBox->Slot))
	{
		BtnSlot->SetPadding(ContentPadding);
		BtnSlot->SetHorizontalAlignment(HAlign_Center);
		BtnSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Optional icon (hidden by default)
	ButtonIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ButtonIcon"));
	ButtonIcon->SetVisibility(ESlateVisibility::Collapsed);
	if (UHorizontalBoxSlot* IconSlot = Cast<UHorizontalBoxSlot>(HBox->AddChildToHorizontalBox(ButtonIcon)))
	{
		IconSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetVerticalAlignment(VAlign_Center);
	}

	if (IconTexture)
	{
		ButtonIcon->SetBrushFromTexture(IconTexture);
		ButtonIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// Label
	ButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ButtonLabel"));
	ButtonLabel->SetText(ButtonText);
	if (UHorizontalBoxSlot* LabelSlot = Cast<UHorizontalBoxSlot>(HBox->AddChildToHorizontalBox(ButtonLabel)))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void URammsButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsButton::NativeConstruct()
{
	Super::NativeConstruct();

	if (InnerButton)
	{
		InnerButton->OnClicked.AddUniqueDynamic(this, &URammsButton::OnButtonClicked);
		InnerButton->OnHovered.AddUniqueDynamic(this, &URammsButton::OnButtonHovered);
		InnerButton->OnUnhovered.AddUniqueDynamic(this, &URammsButton::OnButtonUnhovered);
		InnerButton->OnPressed.AddUniqueDynamic(this, &URammsButton::OnButtonPressed);
		InnerButton->OnReleased.AddUniqueDynamic(this, &URammsButton::OnButtonReleased);
	}

	if (ButtonLabel)
	{
		ButtonLabel->SetText(ButtonText);
	}

	UpdateVisualState();
}

void URammsButton::SetIcon(UTexture2D* Texture)
{
	IconTexture = Texture;
	if (ButtonIcon)
	{
		if (Texture)
		{
			ButtonIcon->SetBrushFromTexture(Texture);
			ButtonIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			ButtonIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void URammsButton::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	if (ButtonLabel)
	{
		ButtonLabel->SetFont(Style->Typography.Body);
	}

	UpdateVisualState();
}

void URammsButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (ButtonLabel)
	{
		ButtonLabel->SetText(ButtonText);
	}

	if (ButtonIcon)
	{
		if (IconTexture)
		{
			ButtonIcon->SetBrushFromTexture(IconTexture);
			ButtonIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			ButtonIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (InnerButton)
	{
		InnerButton->SetIsEnabled(bButtonEnabled);
	}

	UpdateVisualState();
}

void URammsButton::SetText(FText Text)
{
	ButtonText = Text;
	if (ButtonLabel)
	{
		ButtonLabel->SetText(ButtonText);
	}
}

void URammsButton::SetEnabled(bool bEnabled)
{
	bButtonEnabled = bEnabled;

	if (InnerButton)
	{
		InnerButton->SetIsEnabled(bButtonEnabled);
	}

	CurrentState = bButtonEnabled ? ERammsButtonState::Normal : ERammsButtonState::Disabled;
	UpdateVisualState();
}

void URammsButton::SetVariant(ERammsButtonVariant NewVariant)
{
	Variant = NewVariant;
	UpdateVisualState();
}

void URammsButton::OnButtonClicked()
{
	if (bButtonEnabled)
	{
		OnClicked.Broadcast();
	}
}

void URammsButton::OnButtonHovered()
{
	if (bButtonEnabled)
	{
		CurrentState = ERammsButtonState::Hovered;
		UpdateVisualState();
	}
}

void URammsButton::OnButtonUnhovered()
{
	if (bButtonEnabled)
	{
		CurrentState = ERammsButtonState::Normal;
		UpdateVisualState();
	}
}

void URammsButton::OnButtonPressed()
{
	if (bButtonEnabled)
	{
		CurrentState = ERammsButtonState::Pressed;
		UpdateVisualState();
	}
}

void URammsButton::OnButtonReleased()
{
	if (bButtonEnabled)
	{
		CurrentState = ERammsButtonState::Hovered;
		UpdateVisualState();
	}
}

FLinearColor URammsButton::GetVariantColor() const
{
	if (!Style)
	{
		// Fallback colors when no style asset
		switch (Variant)
		{
			case ERammsButtonVariant::Primary:
				return FLinearColor(0.0f, 0.478f, 0.8f);
			case ERammsButtonVariant::Success:
				return FLinearColor(0.0f, 0.8f, 0.3f);
			case ERammsButtonVariant::Warning:
				return FLinearColor(1.0f, 0.7f, 0.0f);
			case ERammsButtonVariant::Error:
				return FLinearColor(0.9f, 0.2f, 0.2f);
			case ERammsButtonVariant::Ghost:
				return FLinearColor::Transparent;
			case ERammsButtonVariant::Secondary:
			default:
				return FLinearColor(0.25f, 0.25f, 0.3f);
		}
	}

	switch (Variant)
	{
		case ERammsButtonVariant::Primary:
			return Style->Colors.Primary;
		case ERammsButtonVariant::Success:
			return Style->Colors.Success;
		case ERammsButtonVariant::Warning:
			return Style->Colors.Warning;
		case ERammsButtonVariant::Error:
			return Style->Colors.Error;
		case ERammsButtonVariant::Ghost:
			return FLinearColor::Transparent;
		case ERammsButtonVariant::Secondary:
		default:
			return Style->Colors.Secondary;
	}
}

void URammsButton::UpdateVisualState()
{
	if (!ButtonBorder)
		return;

	FLinearColor BaseColor = GetVariantColor();
	FLinearColor TextColor = Style ? Style->Colors.TextPrimary : FLinearColor::White;
	FLinearColor TextDisabledColor = Style ? Style->Colors.TextDisabled : FLinearColor(0.4f, 0.4f, 0.4f);
	float		 CornerRadius = Style ? Style->Border.CornerRadiusMedium : 4.0f;

	FLinearColor BgColor = BaseColor;
	float		 Opacity = 1.0f;

	switch (CurrentState)
	{
		case ERammsButtonState::Normal:
			BgColor = BaseColor;
			break;

		case ERammsButtonState::Hovered:
			// Lighten toward white
			BgColor = FLinearColor::LerpUsingHSV(BaseColor, FLinearColor::White, 0.15f);
			BgColor.A = BaseColor.A;
			break;

		case ERammsButtonState::Pressed:
			// Darken toward black
			BgColor = FLinearColor::LerpUsingHSV(BaseColor, FLinearColor::Black, 0.2f);
			BgColor.A = BaseColor.A;
			break;

		case ERammsButtonState::Disabled:
			BgColor = BaseColor;
			BgColor.A *= 0.4f;
			TextColor = TextDisabledColor;
			Opacity = 0.6f;
			break;
	}

	// Ghost variant: text uses variant-appropriate color instead of white
	if (Variant == ERammsButtonVariant::Ghost)
	{
		FLinearColor GhostText = Style ? Style->Colors.TextPrimary : FLinearColor::White;
		if (CurrentState == ERammsButtonState::Hovered)
			GhostText.A = 0.8f;
		else if (CurrentState == ERammsButtonState::Pressed)
			GhostText.A = 0.6f;
		else if (CurrentState == ERammsButtonState::Disabled)
			GhostText = TextDisabledColor;
		TextColor = GhostText;
	}

	// Apply rounded background to the border
	FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrush(BgColor, CornerRadius);
	URammsUIStyle::ApplyRoundedBrushToBorder(ButtonBorder, Brush);
	ButtonBorder->SetRenderOpacity(Opacity);

	if (ButtonLabel)
	{
		ButtonLabel->SetColorAndOpacity(FSlateColor(TextColor));
	}

	if (ButtonIcon)
	{
		ButtonIcon->SetColorAndOpacity(TextColor);
	}
}
