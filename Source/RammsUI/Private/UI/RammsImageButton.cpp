// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsImageButton.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/ButtonSlot.h"

void URammsImageButton::ResetCachedWidgets()
{
	InnerButton = nullptr;
	ContentImage = nullptr;
	Label = nullptr;
	ActiveBorder = nullptr;
	ImageSizeBox = nullptr;
}

void URammsImageButton::BuildWidgetTree()
{
	if (!WidgetTree || InnerButton)
		return;

	// Root: Button
	InnerButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("InnerButton"));
	WidgetTree->RootWidget = InnerButton;

	// VerticalBox for image + label stacking
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentVBox"));
	InnerButton->AddChild(VBox);
	if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(VBox->Slot))
	{
		ContentSlot->SetPadding(FMargin(ContentPadding));
		ContentSlot->SetHorizontalAlignment(HAlign_Center);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Overlay wraps the image so we can put an active border on it
	UOverlay*		  ImageOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ImageOverlay"));
	UVerticalBoxSlot* OverlaySlot = VBox->AddChildToVerticalBox(ImageOverlay);
	if (OverlaySlot)
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Center);
		OverlaySlot->SetVerticalAlignment(VAlign_Center);
	}

	// Active border (highlight ring when active)
	ActiveBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ActiveBorder"));
	ActiveBorder->SetBrushColor(FLinearColor(0.0f, 0.478f, 0.8f, 0.0f)); // Transparent when inactive
	ActiveBorder->SetPadding(FMargin(ActiveBorderWidth));
	UOverlaySlot* BorderOverlaySlot = ImageOverlay->AddChildToOverlay(ActiveBorder);
	if (BorderOverlaySlot)
	{
		BorderOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		BorderOverlaySlot->SetVerticalAlignment(VAlign_Fill);
	}

	// SizeBox constrains the image to desired dimensions
	ImageSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ImageSizeBox"));
	ImageSizeBox->SetWidthOverride(ImageSize.X);
	ImageSizeBox->SetHeightOverride(ImageSize.Y);
	ActiveBorder->AddChild(ImageSizeBox);

	// The actual image
	ContentImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ContentImage"));
	if (ButtonImage)
	{
		ContentImage->SetBrushFromTexture(ButtonImage);
	}
	else
	{
		ContentImage->SetBrushTintColor(FSlateColor(FLinearColor(0.3f, 0.3f, 0.35f)));
	}
	ImageSizeBox->AddChild(ContentImage);

	// Label below the image
	Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
	Label->SetText(LabelText);
	Label->SetJustification(ETextJustify::Center);
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UVerticalBoxSlot* LabelSlot = VBox->AddChildToVerticalBox(Label);
	if (LabelSlot)
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
	}

	if (!bShowLabel)
	{
		Label->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URammsImageButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsImageButton::NativeConstruct()
{
	Super::NativeConstruct();

	if (InnerButton)
	{
		InnerButton->OnClicked.AddDynamic(this, &URammsImageButton::HandleClicked);
		InnerButton->OnHovered.AddDynamic(this, &URammsImageButton::HandleHovered);
		InnerButton->OnUnhovered.AddDynamic(this, &URammsImageButton::HandleUnhovered);
		InnerButton->OnPressed.AddDynamic(this, &URammsImageButton::HandlePressed);
		InnerButton->OnReleased.AddDynamic(this, &URammsImageButton::HandleReleased);
	}

	UpdateVisualState();
}

void URammsImageButton::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	if (Label)
	{
		Label->SetFont(Style->Typography.Caption);
	}

	UpdateVisualState();
}

void URammsImageButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (Label)
	{
		Label->SetText(LabelText);
		Label->SetVisibility(bShowLabel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (ImageSizeBox)
	{
		ImageSizeBox->SetWidthOverride(ImageSize.X);
		ImageSizeBox->SetHeightOverride(ImageSize.Y);
	}

	if (ContentImage)
	{
		if (ButtonImage)
		{
			ContentImage->SetBrushFromTexture(ButtonImage);
			ContentImage->SetBrushTintColor(FSlateColor(FLinearColor::White));
		}
		else
		{
			ContentImage->SetBrushTintColor(FSlateColor(FLinearColor(0.3f, 0.3f, 0.35f)));
		}
	}

	if (InnerButton)
	{
		InnerButton->SetIsEnabled(bButtonEnabled);
	}

	UpdateVisualState();
}

void URammsImageButton::SetButtonImage(UTexture2D* Texture)
{
	ButtonImage = Texture;
	if (ContentImage)
	{
		if (Texture)
		{
			ContentImage->SetBrushFromTexture(Texture);
			ContentImage->SetBrushTintColor(FSlateColor(FLinearColor::White));
		}
		else
		{
			ContentImage->SetBrushTintColor(FSlateColor(FLinearColor(0.3f, 0.3f, 0.35f)));
		}
	}
}

void URammsImageButton::SetLabelText(FText Text)
{
	LabelText = Text;
	if (Label)
	{
		Label->SetText(LabelText);
	}
}

void URammsImageButton::SetActive(bool bNewActive)
{
	bIsActive = bNewActive;
	UpdateVisualState();
}

void URammsImageButton::SetButtonEnabled(bool bEnabled)
{
	bButtonEnabled = bEnabled;
	if (InnerButton)
	{
		InnerButton->SetIsEnabled(bEnabled);
	}
	UpdateVisualState();
}

void URammsImageButton::SetImageSize(FVector2D NewSize)
{
	ImageSize = NewSize;
	if (ImageSizeBox)
	{
		ImageSizeBox->SetWidthOverride(ImageSize.X);
		ImageSizeBox->SetHeightOverride(ImageSize.Y);
	}
}

void URammsImageButton::HandleClicked()
{
	if (!bButtonEnabled)
		return;

	if (bIsToggle)
	{
		bIsActive = !bIsActive;
		UpdateVisualState();
		OnToggled.Broadcast(bIsActive);
	}

	OnClicked.Broadcast();
}

void URammsImageButton::HandleHovered()
{
	bIsHovered = true;
	UpdateVisualState();
}

void URammsImageButton::HandleUnhovered()
{
	bIsHovered = false;
	bIsPressed = false;
	UpdateVisualState();
}

void URammsImageButton::HandlePressed()
{
	bIsPressed = true;
	UpdateVisualState();
}

void URammsImageButton::HandleReleased()
{
	bIsPressed = false;
	UpdateVisualState();
}

void URammsImageButton::UpdateVisualState()
{
	if (!InnerButton)
		return;

	// Determine colors
	FLinearColor BgColor(0.15f, 0.15f, 0.18f);
	FLinearColor BorderColor(0.0f, 0.478f, 0.8f, 0.0f); // Transparent default
	FLinearColor TextColor = FLinearColor::White;
	float		 Opacity = 1.0f;

	if (Style)
	{
		BgColor = Style->Colors.Surface;
		TextColor = Style->Colors.TextPrimary;
	}

	if (!bButtonEnabled)
	{
		Opacity = 0.4f;
		TextColor = Style ? Style->Colors.TextDisabled : FLinearColor(0.4f, 0.4f, 0.4f);
	}
	else if (bIsPressed)
	{
		Opacity = 0.6f;
	}
	else if (bIsHovered)
	{
		Opacity = 0.85f;
	}

	// Active state: show border highlight
	if (bIsActive && bButtonEnabled)
	{
		FLinearColor ActiveColor = Style ? Style->Colors.Primary : FLinearColor(0.0f, 0.478f, 0.8f);
		BorderColor = ActiveColor;
		BgColor = FLinearColor::LerpUsingHSV(BgColor, ActiveColor, 0.15f);
	}

	InnerButton->SetBackgroundColor(BgColor);
	InnerButton->SetRenderOpacity(Opacity);

	if (ActiveBorder)
	{
		ActiveBorder->SetBrushColor(BorderColor);
	}

	if (Label)
	{
		Label->SetColorAndOpacity(FSlateColor(TextColor));
	}

	// Tint image slightly when disabled
	if (ContentImage && ButtonImage)
	{
		FLinearColor Tint = bButtonEnabled ? FLinearColor::White : FLinearColor(0.5f, 0.5f, 0.5f);
		ContentImage->SetBrushTintColor(FSlateColor(Tint));
	}
}
