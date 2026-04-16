// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsImageButton.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/ButtonSlot.h"

void URammsImageButton::ResetCachedWidgets()
{
	RootSizeBox = nullptr;
	ButtonBorder = nullptr;
	InnerButton = nullptr;
	ContentImage = nullptr;
	Label = nullptr;
	ActiveBorder = nullptr;
	ImageSizeBox = nullptr;
}

void URammsImageButton::BuildWidgetTree()
{
	if (!WidgetTree || RootSizeBox)
		return;

	// Root: SizeBox — allows optional fixed sizing
	RootSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RootSizeBox"));
	WidgetTree->RootWidget = RootSizeBox;

	// Border — provides visual background, rounded corners, clipping
	ButtonBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ButtonBorder"));
	ButtonBorder->SetClipping(EWidgetClipping::ClipToBounds);
	RootSizeBox->AddChild(ButtonBorder);

	// Inner: transparent UButton for click/hover/press handling
	InnerButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("InnerButton"));
	ButtonBorder->AddChild(InnerButton);

	FButtonStyle TransparentStyle;
	TransparentStyle.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
	TransparentStyle.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
	TransparentStyle.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
	TransparentStyle.Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
	InnerButton->SetStyle(TransparentStyle);

	// VerticalBox for image + label stacking
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentVBox"));
	InnerButton->AddChild(VBox);
	if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(VBox->Slot))
	{
		ContentSlot->SetPadding(FMargin(ContentPadding));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
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
		FLinearColor Tint = (bApplyIconTint && Style) ? Style->Colors.IconTint : FLinearColor::White;
		ContentImage->SetBrushTintColor(FSlateColor(Tint));
	}
	else
	{
		FLinearColor PlaceholderTint = Style ? Style->Colors.TextDisabled : FLinearColor(0.3f, 0.3f, 0.35f);
		ContentImage->SetBrushTintColor(FSlateColor(PlaceholderTint));
	}
	ImageSizeBox->AddChild(ContentImage);

	// Label below the image
	Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
	Label->SetText(LabelText);
	Label->SetJustification(ETextJustify::Center);
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Label->SetAutoWrapText(bAutoWrapLabel);
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

	ApplySizeOverrides();
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
		InnerButton->OnClicked.AddUniqueDynamic(this, &URammsImageButton::HandleClicked);
		InnerButton->OnHovered.AddUniqueDynamic(this, &URammsImageButton::HandleHovered);
		InnerButton->OnUnhovered.AddUniqueDynamic(this, &URammsImageButton::HandleUnhovered);
		InnerButton->OnPressed.AddUniqueDynamic(this, &URammsImageButton::HandlePressed);
		InnerButton->OnReleased.AddUniqueDynamic(this, &URammsImageButton::HandleReleased);
	}

	UpdateVisualState();
}

void URammsImageButton::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	if (Label)
	{
		Label->SetFont(Style->Interaction.GetActionButtonFont(Style->Typography.Caption));
	}

	UpdateVisualState();
}

void URammsImageButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (Label)
	{
		Label->SetText(LabelText);
		Label->SetAutoWrapText(bAutoWrapLabel);
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
			FLinearColor Tint = (bApplyIconTint && Style) ? Style->Colors.IconTint : FLinearColor::White;
			ContentImage->SetBrushTintColor(FSlateColor(Tint));
		}
		else
		{
			FLinearColor DisabledTint = Style ? Style->Colors.TextDisabled : FLinearColor(0.3f, 0.3f, 0.35f);
			ContentImage->SetBrushTintColor(FSlateColor(DisabledTint));
		}
	}

	if (InnerButton)
	{
		InnerButton->SetIsEnabled(bButtonEnabled);
	}

	ApplySizeOverrides();
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
			FLinearColor Tint = (bApplyIconTint && Style) ? Style->Colors.IconTint : FLinearColor::White;
			ContentImage->SetBrushTintColor(FSlateColor(Tint));
		}
		else
		{
			FLinearColor DisabledTint = Style ? Style->Colors.TextDisabled : FLinearColor(0.3f, 0.3f, 0.35f);
			ContentImage->SetBrushTintColor(FSlateColor(DisabledTint));
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

void URammsImageButton::SetShowLabel(bool bShow)
{
	bShowLabel = bShow;
	if (Label)
	{
		Label->SetVisibility(bShowLabel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void URammsImageButton::SetLabelAutoWrap(bool bAutoWrap)
{
	bAutoWrapLabel = bAutoWrap;
	if (Label)
	{
		Label->SetAutoWrapText(bAutoWrap);
	}
}

void URammsImageButton::SetUseCustomSize(bool bUseCustom)
{
	bUseCustomSize = bUseCustom;
	ApplySizeOverrides();
}

void URammsImageButton::SetCustomSize(FVector2D NewSize)
{
	bUseCustomSize = true;
	CustomSize = NewSize;
	ApplySizeOverrides();
}

void URammsImageButton::SetApplyIconTint(bool bApply)
{
	if (bApplyIconTint == bApply)
		return;
	bApplyIconTint = bApply;
	UpdateVisualState();
}

void URammsImageButton::ApplySizeOverrides()
{
	if (!RootSizeBox)
		return;

	if (bUseCustomSize)
	{
		RootSizeBox->SetWidthOverride(CustomSize.X);
		RootSizeBox->SetHeightOverride(CustomSize.Y);
	}
	else
	{
		RootSizeBox->ClearWidthOverride();
		RootSizeBox->ClearHeightOverride();
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
	if (!ButtonBorder)
		return;

	// Determine colors — use Style properties with fallback defaults
	FLinearColor BgColor = Style ? Style->Colors.Surface : FLinearColor(0.15f, 0.15f, 0.18f);
	FLinearColor BorderColor = FLinearColor::Transparent;
	FLinearColor TextColor = Style ? Style->Colors.TextPrimary : FLinearColor::White;
	float		 Opacity = 1.0f;

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

	ButtonBorder->SetBrushColor(BgColor);
	ButtonBorder->SetRenderOpacity(Opacity);

	if (ActiveBorder)
	{
		ActiveBorder->SetBrushColor(BorderColor);
	}

	if (Label)
	{
		Label->SetColorAndOpacity(FSlateColor(TextColor));
	}

	// Tint image: use style IconTint when enabled (and opted in), desaturated tint when disabled
	if (ContentImage && ButtonImage)
	{
		FLinearColor Tint = FLinearColor::White;
		if (!bButtonEnabled)
		{
			Tint = Style ? FLinearColor(Style->Colors.TextDisabled.R, Style->Colors.TextDisabled.G, Style->Colors.TextDisabled.B)
						 : FLinearColor(0.5f, 0.5f, 0.5f);
		}
		else if (bApplyIconTint && Style)
		{
			Tint = Style->Colors.IconTint;
		}
		ContentImage->SetBrushTintColor(FSlateColor(Tint));
	}
}
