// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsButton.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/OverlaySlot.h"

void URammsButton::ResetCachedWidgets()
{
	InnerButton = nullptr;
	ButtonLabel = nullptr;
	ButtonIcon = nullptr;
}

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
	if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(HBox->Slot))
	{
		ContentSlot->SetPadding(FMargin(12.0f, 6.0f));
		ContentSlot->SetHorizontalAlignment(HAlign_Center);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
	}

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
	if (!InnerButton)
		return;

	FLinearColor BackgroundColor;
	FLinearColor TextColor;

	// Use style colors or fallback defaults
	FLinearColor PrimaryColor = Style ? Style->Colors.Primary : FLinearColor(0.0f, 0.478f, 0.8f);
	FLinearColor SecondaryColor = Style ? Style->Colors.Secondary : FLinearColor(0.25f, 0.25f, 0.3f);
	FLinearColor TextPrimaryColor = Style ? Style->Colors.TextPrimary : FLinearColor::White;
	FLinearColor TextDisabledColor = Style ? Style->Colors.TextDisabled : FLinearColor(0.4f, 0.4f, 0.4f);

	switch (CurrentState)
	{
	case ERammsButtonState::Normal:
		BackgroundColor = bUsePrimaryColor ? PrimaryColor : SecondaryColor;
		TextColor = TextPrimaryColor;
		InnerButton->SetRenderOpacity(1.0f);
		break;

	case ERammsButtonState::Hovered:
		BackgroundColor = bUsePrimaryColor ? PrimaryColor : SecondaryColor;
		TextColor = TextPrimaryColor;
		InnerButton->SetRenderOpacity(0.8f);
		break;

	case ERammsButtonState::Pressed:
		BackgroundColor = bUsePrimaryColor ? PrimaryColor : SecondaryColor;
		TextColor = TextPrimaryColor;
		InnerButton->SetRenderOpacity(0.6f);
		break;

	case ERammsButtonState::Disabled:
		BackgroundColor = SecondaryColor;
		TextColor = TextDisabledColor;
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
