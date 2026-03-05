// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsButton.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/OverlaySlot.h"

void URammsButton::BuildWidgetTree()
{
	if (!WidgetTree || InnerButton)
		return; // Already built or no tree

	// Root: Button
	InnerButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("InnerButton"));
	WidgetTree->RootWidget = InnerButton;

	// HorizontalBox inside the button for icon + label layout
	UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonHBox"));
	InnerButton->AddChild(HBox);

	// Optional icon (hidden by default)
	ButtonIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ButtonIcon"));
	ButtonIcon->SetVisibility(ESlateVisibility::Collapsed);
	UHorizontalBoxSlot* IconSlot = Cast<UHorizontalBoxSlot>(HBox->AddChildToHorizontalBox(ButtonIcon));
	if (IconSlot)
	{
		IconSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Show icon if texture was set in editor
	if (IconTexture)
	{
		ButtonIcon->SetBrushFromTexture(IconTexture);
		ButtonIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// Label
	ButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ButtonLabel"));
	ButtonLabel->SetText(ButtonText);
	UHorizontalBoxSlot* LabelSlot = Cast<UHorizontalBoxSlot>(HBox->AddChildToHorizontalBox(ButtonLabel));
	if (LabelSlot)
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void URammsButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Build widget tree early so it exists before Slate representation is created
	// (required for widgets added as children of other panels)
	BuildWidgetTree();
}

void URammsButton::NativeConstruct()
{
	Super::NativeConstruct();

	if (InnerButton)
	{
		InnerButton->OnClicked.AddDynamic(this, &URammsButton::OnButtonClicked);
		InnerButton->OnHovered.AddDynamic(this, &URammsButton::OnButtonHovered);
		InnerButton->OnUnhovered.AddDynamic(this, &URammsButton::OnButtonUnhovered);
		InnerButton->OnPressed.AddDynamic(this, &URammsButton::OnButtonPressed);
		InnerButton->OnReleased.AddDynamic(this, &URammsButton::OnButtonReleased);
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

	// Apply label styling
	if (ButtonLabel)
	{
		ButtonLabel->SetFont(Style->Typography.Body);
	}

	// Update visual state with new style
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

void URammsButton::UpdateVisualState()
{
	if (!Style || !InnerButton)
		return;

	FLinearColor BackgroundColor;
	FLinearColor TextColor;

	switch (CurrentState)
	{
	case ERammsButtonState::Normal:
		BackgroundColor = bUsePrimaryColor ? Style->Colors.Primary : Style->Colors.Secondary;
		TextColor = Style->Colors.TextPrimary;
		InnerButton->SetRenderOpacity(1.0f);
		break;

	case ERammsButtonState::Hovered:
		BackgroundColor = bUsePrimaryColor ? Style->Colors.Primary : Style->Colors.Secondary;
		TextColor = Style->Colors.TextPrimary;
		InnerButton->SetRenderOpacity(0.8f);
		break;

	case ERammsButtonState::Pressed:
		BackgroundColor = bUsePrimaryColor ? Style->Colors.Primary : Style->Colors.Secondary;
		TextColor = Style->Colors.TextPrimary;
		InnerButton->SetRenderOpacity(0.6f);
		break;

	case ERammsButtonState::Disabled:
		BackgroundColor = Style->Colors.Secondary;
		TextColor = Style->Colors.TextDisabled;
		InnerButton->SetRenderOpacity(0.5f);
		break;
	}

	// Apply colors
	InnerButton->SetBackgroundColor(BackgroundColor);
	
	if (ButtonLabel)
	{
		ButtonLabel->SetColorAndOpacity(FSlateColor(TextColor));
	}
}
