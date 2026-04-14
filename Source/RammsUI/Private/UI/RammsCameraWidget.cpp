// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsCameraWidget.h"
#include "RammsCameraProviderComponent.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Widgets/Layout/SBox.h"
#include "EngineUtils.h"
#include "Styling/CoreStyle.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "UI/RammsBoundingBoxOverlay.h"

DEFINE_LOG_CATEGORY_STATIC(LogRammsCameraWidget, Log, All);

namespace
{
	TMap<TWeakObjectPtr<UMaterialInstanceDynamic>, TSet<FName>> GPreviouslyAppliedDynamicScalarParams;
}

int32																	 URammsCameraWidget::FocusZOrderCounter = 0;
TMap<TPair<UWidget*, uint8>, TArray<TWeakObjectPtr<URammsCameraWidget>>> URammsCameraWidget::CornerRegistry;

void URammsCameraWidget::ResetCachedWidgets()
{
	if (PassthroughMID_RGB)
	{
		GPreviouslyAppliedDynamicScalarParams.Remove(PassthroughMID_RGB);
	}
	if (PassthroughMID_Data)
	{
		GPreviouslyAppliedDynamicScalarParams.Remove(PassthroughMID_Data);
	}

	CameraBorder = nullptr;
	CameraRootOverlay = nullptr;
	ImageContainerOverlay = nullptr;
	ImageHBox = nullptr;
	ImageVBox = nullptr;
	CameraImage = nullptr;
	CameraLabel = nullptr;
	CameraLabelWrap = nullptr;
	ButtonRow = nullptr;
	TitleBar = nullptr;
	CollapseButton = nullptr;
	CollapseIcon = nullptr;
	CameraSizeBox = nullptr;
	InternalSizeBox = nullptr;
	DataImage = nullptr;
	BBoxOverlay = nullptr;
	bBBoxOverlayEnabled = false;
	ViewModeButton = nullptr;
	ViewModeLabel = nullptr;
	OptionButton = nullptr;
	OptionLabel = nullptr;
	DisplayModeCycleButton = nullptr;
	DisplayModeCycleLabel = nullptr;
	ImageAspectRatioBox = nullptr;
	PassthroughMID_RGB = nullptr;
	PassthroughMID_Data = nullptr;
	bHeaderNarrowMode = false;
	bSBSVertical = false;
}

void URammsCameraWidget::BuildWidgetTree()
{
	if (!WidgetTree || CameraBorder)
		return;

	UE_LOG(LogRammsCameraWidget, Log, TEXT("[%s] BuildWidgetTree: StreamID='%s' DisplayMode=%d Collapsible=%d MaintainAR=%d AR=%.3f"),
		*GetName(), *StreamID, (int32)DisplayMode, bCollapsible, bMaintainAspectRatio, AspectRatio);

	// Root: InternalSizeBox wraps everything for non-Canvas layout sizing
	InternalSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("InternalSizeBox"));
	WidgetTree->RootWidget = InternalSizeBox;

	CameraRootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));

	if (bMaintainAspectRatio && !bCollapsible)
	{
		// Non-collapsible: AR box wraps entire widget (title overlays on image)
		ImageAspectRatioBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ImageAspectRatioBox"));
		ImageAspectRatioBox->SetMinAspectRatio(AspectRatio);
		ImageAspectRatioBox->SetMaxAspectRatio(AspectRatio);
		InternalSizeBox->AddChild(ImageAspectRatioBox);
		ImageAspectRatioBox->AddChild(CameraRootOverlay);
	}
	else
	{
		InternalSizeBox->AddChild(CameraRootOverlay);
	}

	if (bCollapsible)
	{
		// VBox layout: TitleBar (header for collapse) + CameraSizeBox
		UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainVBox"));
		UOverlaySlot* VBoxSlot = CameraRootOverlay->AddChildToOverlay(MainVBox);
		if (VBoxSlot)
		{
			VBoxSlot->SetHorizontalAlignment(HAlign_Fill);
			VBoxSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// Title bar with collapse button — rounded top corners
		TitleBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TitleBar"));
		TitleBar->Background = URammsUIStyle::MakeRoundedBoxBrushEx(
			FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), FVector4(4.0f, 4.0f, 0.0f, 0.0f));
		TitleBar->SetPadding(FMargin(8.0f, 4.0f));
		UVerticalBoxSlot* TitleSlot = MainVBox->AddChildToVerticalBox(TitleBar);
		if (TitleSlot)
		{
			TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			TitleSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		// VBox inside TitleBar: ButtonRow on top, CameraLabelWrap below (for narrow mode)
		UVerticalBox* TitleVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TitleVBox"));
		TitleBar->AddChild(TitleVBox);

		ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonRow"));
		UVerticalBoxSlot* BtnRowSlot = TitleVBox->AddChildToVerticalBox(ButtonRow);
		if (BtnRowSlot)
		{
			BtnRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			BtnRowSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		// Collapse button (leftmost, before label) — wrapped in SizeBox for touch target
		CollapseBtnSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CollapseBtnSizeBox"));
		CollapseBtnSizeBox->SetMinDesiredWidth(32.0f);
		CollapseBtnSizeBox->SetMinDesiredHeight(32.0f);
		CollapseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CollapseButton"));
		CollapseButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		CollapseBtnSizeBox->AddChild(CollapseButton);
		UHorizontalBoxSlot* CollapseBtnSlot = ButtonRow->AddChildToHorizontalBox(CollapseBtnSizeBox);
		if (CollapseBtnSlot)
		{
			CollapseBtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			CollapseBtnSlot->SetVerticalAlignment(VAlign_Center);
			CollapseBtnSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
		}

		CollapseIcon = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CollapseIcon"));
		CollapseIcon->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		CollapseButton->AddChild(CollapseIcon);

		// Camera label inline (fills remaining space — visible in wide mode)
		CameraLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CameraLabel"));
		FText LabelText = CustomLabel.IsEmpty() ? FText::FromString(StreamID.IsEmpty() ? TEXT("Camera") : StreamID) : CustomLabel;
		CameraLabel->SetText(LabelText);
		CameraLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		CameraLabel->SetAutoWrapText(false);
		UHorizontalBoxSlot* LabelSlot = ButtonRow->AddChildToHorizontalBox(CameraLabel);
		if (LabelSlot)
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		// View mode toggle button — wrapped in SizeBox for touch target
		ViewModeBtnSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ViewModeBtnSizeBox"));
		ViewModeBtnSizeBox->SetMinDesiredWidth(32.0f);
		ViewModeBtnSizeBox->SetMinDesiredHeight(32.0f);
		ViewModeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ViewModeButton"));
		ViewModeButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		ViewModeBtnSizeBox->AddChild(ViewModeButton);
		UHorizontalBoxSlot* ViewModeBtnSlot = ButtonRow->AddChildToHorizontalBox(ViewModeBtnSizeBox);
		if (ViewModeBtnSlot)
		{
			ViewModeBtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			ViewModeBtnSlot->SetVerticalAlignment(VAlign_Center);
			ViewModeBtnSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
		}
		ViewModeLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ViewModeLabel"));
		ViewModeLabel->SetText(FText::FromString(TEXT("RGB")));
		ViewModeLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		ViewModeLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10));
		ViewModeButton->AddChild(ViewModeLabel);

		// Data stream option cycling button (hidden until config has options) — wrapped in SizeBox
		OptionBtnSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("OptionBtnSizeBox"));
		OptionBtnSizeBox->SetMinDesiredWidth(32.0f);
		OptionBtnSizeBox->SetMinDesiredHeight(32.0f);
		OptionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("OptionButton"));
		OptionButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		OptionBtnSizeBox->AddChild(OptionButton);
		UHorizontalBoxSlot* OptBtnSlot = ButtonRow->AddChildToHorizontalBox(OptionBtnSizeBox);
		if (OptBtnSlot)
		{
			OptBtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			OptBtnSlot->SetVerticalAlignment(VAlign_Center);
			OptBtnSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
		}
		OptionLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("OptionLabel"));
		OptionLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.8f, 1.0f)));
		OptionLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10));
		OptionButton->AddChild(OptionLabel);
		OptionBtnSizeBox->SetVisibility(ESlateVisibility::Collapsed);

		// Display mode cycle button (rightmost) — wrapped in SizeBox
		DisplayModeBtnSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DisplayModeBtnSizeBox"));
		DisplayModeBtnSizeBox->SetMinDesiredWidth(32.0f);
		DisplayModeBtnSizeBox->SetMinDesiredHeight(32.0f);
		DisplayModeCycleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DisplayModeCycleButton"));
		DisplayModeCycleButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		DisplayModeBtnSizeBox->AddChild(DisplayModeCycleButton);
		UHorizontalBoxSlot* CycleBtnSlot = ButtonRow->AddChildToHorizontalBox(DisplayModeBtnSizeBox);
		if (CycleBtnSlot)
		{
			CycleBtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			CycleBtnSlot->SetVerticalAlignment(VAlign_Center);
			CycleBtnSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
		}
		DisplayModeCycleLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DisplayModeCycleLabel"));
		DisplayModeCycleLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.9f, 0.7f)));
		DisplayModeCycleLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10));
		DisplayModeCycleButton->AddChild(DisplayModeCycleLabel);

		// Wrapped label row (shown in narrow mode, below buttons)
		CameraLabelWrap = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CameraLabelWrap"));
		CameraLabelWrap->SetText(LabelText);
		CameraLabelWrap->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		CameraLabelWrap->SetAutoWrapText(true);
		CameraLabelWrap->SetVisibility(ESlateVisibility::Collapsed);
		UVerticalBoxSlot* WrapLabelSlot = TitleVBox->AddChildToVerticalBox(CameraLabelWrap);
		if (WrapLabelSlot)
		{
			WrapLabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			WrapLabelSlot->SetHorizontalAlignment(HAlign_Fill);
			WrapLabelSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
		}

		// SizeBox wrapping camera content(Fill = takes remaining space, for collapse animation)
		CameraSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CameraSizeBox"));
		CameraSizeBox->SetClipping(EWidgetClipping::ClipToBounds);
		UVerticalBoxSlot* SizeBoxSlot = MainVBox->AddChildToVerticalBox(CameraSizeBox);
		if (SizeBoxSlot)
		{
			SizeBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			SizeBoxSlot->SetHorizontalAlignment(HAlign_Fill);
			SizeBoxSlot->SetVerticalAlignment(VAlign_Fill);
		}

		CameraBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CameraBorder"));
		// Transparent background — corner masking is handled by the material shader
		CameraBorder->Background = URammsUIStyle::MakeRoundedBoxBrushEx(
			FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), FVector4(0.0f, 0.0f, 4.0f, 4.0f));
		CameraBorder->SetPadding(FMargin(BorderThickness));
		CameraBorder->SetClipping(EWidgetClipping::ClipToBounds);

		CameraSizeBox->AddChild(CameraBorder);

		// Overlay wrapping the image area — bbox overlay is hosted here
		ImageContainerOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ImageContainerOverlay"));
		CameraBorder->AddChild(ImageContainerOverlay);

		// HBox to hold RGB image and optional data image side-by-side (portrait SBS / fullscreen)
		ImageHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ImageHBox"));
		UOverlaySlot* ImageHBoxSlot = ImageContainerOverlay->AddChildToOverlay(ImageHBox);
		if (ImageHBoxSlot)
		{
			ImageHBoxSlot->SetHorizontalAlignment(HAlign_Fill);
			ImageHBoxSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// VBox for top/bottom image stacking (landscape SBS) — starts collapsed
		ImageVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ImageVBox"));
		ImageVBox->SetVisibility(ESlateVisibility::Collapsed);
		UOverlaySlot* ImageVBoxSlot = ImageContainerOverlay->AddChildToOverlay(ImageVBox);
		if (ImageVBoxSlot)
		{
			ImageVBoxSlot->SetHorizontalAlignment(HAlign_Fill);
			ImageVBoxSlot->SetVerticalAlignment(VAlign_Fill);
		}

		CameraImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CameraImage"));
		CameraImage->SetColorAndOpacity(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));
		UHorizontalBoxSlot* RGBSlot = ImageHBox->AddChildToHorizontalBox(CameraImage);
		if (RGBSlot)
		{
			RGBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			RGBSlot->SetHorizontalAlignment(HAlign_Fill);
			RGBSlot->SetVerticalAlignment(VAlign_Fill);
		}

		DataImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DataImage"));
		DataImage->SetColorAndOpacity(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));
		DataImage->SetVisibility(ESlateVisibility::Collapsed); // hidden unless SideBySide
		UHorizontalBoxSlot* DataSlot = ImageHBox->AddChildToHorizontalBox(DataImage);
		if (DataSlot)
		{
			DataSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			DataSlot->SetHorizontalAlignment(HAlign_Fill);
			DataSlot->SetVerticalAlignment(VAlign_Fill);
			DataSlot->SetPadding(FMargin(2.0f, 0.0f, 0.0f, 0.0f));
		}

		// Initial collapsed state: HeightOverride(0) + HitTestInvisible (stays in layout for width)
		if (bCameraCollapsed)
		{
			SetCameraSizeBoxSlotFill(false);
			CameraSizeBox->SetHeightOverride(0.0f);
			CameraSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
			CameraSizeBox->SetRenderOpacity(0.0f);
			CollapseProgress = 0.0f;
			CollapseTarget = 0.0f;
			// Clear aspect ratio so collapsed header isn't forced to image AR
			if (ImageAspectRatioBox && bMaintainAspectRatio)
			{
				ImageAspectRatioBox->ClearMinAspectRatio();
				ImageAspectRatioBox->ClearMaxAspectRatio();
			}
		}
	}
	else
	{
		// Original Overlay layout: CameraBorder fills, TitleBar overlaps at top
		CameraBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CameraBorder"));
		// Transparent background — corner masking is handled by the material shader
		CameraBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(
			FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 4.0f);
		CameraBorder->SetPadding(FMargin(BorderThickness));
		CameraBorder->SetClipping(EWidgetClipping::ClipToBounds);
		UOverlaySlot* BorderSlot = CameraRootOverlay->AddChildToOverlay(CameraBorder);
		if (BorderSlot)
		{
			BorderSlot->SetHorizontalAlignment(HAlign_Fill);
			BorderSlot->SetVerticalAlignment(VAlign_Fill);
		}

		CameraImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CameraImage"));
		CameraImage->SetColorAndOpacity(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));

		// Overlay wrapping the image area — bbox overlay is hosted here
		ImageContainerOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ImageContainerOverlay"));
		CameraBorder->AddChild(ImageContainerOverlay);

		// HBox for side-by-side support (portrait SBS / fullscreen)
		ImageHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ImageHBox"));
		UOverlaySlot* ImageHBoxSlot = ImageContainerOverlay->AddChildToOverlay(ImageHBox);
		if (ImageHBoxSlot)
		{
			ImageHBoxSlot->SetHorizontalAlignment(HAlign_Fill);
			ImageHBoxSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// VBox for top/bottom stacking (landscape SBS) — starts collapsed
		ImageVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ImageVBox"));
		ImageVBox->SetVisibility(ESlateVisibility::Collapsed);
		UOverlaySlot* ImageVBoxSlot = ImageContainerOverlay->AddChildToOverlay(ImageVBox);
		if (ImageVBoxSlot)
		{
			ImageVBoxSlot->SetHorizontalAlignment(HAlign_Fill);
			ImageVBoxSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UHorizontalBoxSlot* RGBSlot = ImageHBox->AddChildToHorizontalBox(CameraImage);
		if (RGBSlot)
		{
			RGBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			RGBSlot->SetHorizontalAlignment(HAlign_Fill);
			RGBSlot->SetVerticalAlignment(VAlign_Fill);
		}

		DataImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DataImage"));
		DataImage->SetColorAndOpacity(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));
		DataImage->SetVisibility(ESlateVisibility::Collapsed);
		UHorizontalBoxSlot* DataSlot = ImageHBox->AddChildToHorizontalBox(DataImage);
		if (DataSlot)
		{
			DataSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			DataSlot->SetHorizontalAlignment(HAlign_Fill);
			DataSlot->SetVerticalAlignment(VAlign_Fill);
			DataSlot->SetPadding(FMargin(2.0f, 0.0f, 0.0f, 0.0f));
		}

		if (bShowLabel)
		{
			TitleBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TitleBar"));
			// Inner radius = outer - border inset to match image corners
			float InnerR = FMath::Max(4.0f - BorderThickness, 0.0f);
			TitleBar->Background = URammsUIStyle::MakeRoundedBoxBrushEx(
				FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), FVector4(InnerR, InnerR, 0.0f, 0.0f));
			TitleBar->SetPadding(FMargin(8.0f, 4.0f));
			UOverlaySlot* TitleSlot = CameraRootOverlay->AddChildToOverlay(TitleBar);
			if (TitleSlot)
			{
				TitleSlot->SetHorizontalAlignment(HAlign_Fill);
				TitleSlot->SetVerticalAlignment(VAlign_Top);
			}

			// VBox inside TitleBar: ButtonRow on top, CameraLabelWrap below (for narrow mode)
			UVerticalBox* TitleVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TitleVBox"));
			TitleBar->AddChild(TitleVBox);

			ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonRow"));
			UVerticalBoxSlot* BtnRowSlot = TitleVBox->AddChildToVerticalBox(ButtonRow);
			if (BtnRowSlot)
			{
				BtnRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				BtnRowSlot->SetHorizontalAlignment(HAlign_Fill);
			}

			CameraLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CameraLabel"));
			FText LabelText = CustomLabel.IsEmpty() ? FText::FromString(StreamID.IsEmpty() ? TEXT("Camera") : StreamID) : CustomLabel;
			CameraLabel->SetText(LabelText);
			CameraLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			CameraLabel->SetAutoWrapText(false);
			UHorizontalBoxSlot* LabelSlot = ButtonRow->AddChildToHorizontalBox(CameraLabel);
			if (LabelSlot)
			{
				LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				LabelSlot->SetVerticalAlignment(VAlign_Center);
			}

			// View mode button in non-collapsible title bar — wrapped in SizeBox
			ViewModeBtnSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ViewModeBtnSizeBox"));
			ViewModeBtnSizeBox->SetMinDesiredWidth(32.0f);
			ViewModeBtnSizeBox->SetMinDesiredHeight(32.0f);
			ViewModeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ViewModeButton"));
			ViewModeButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			ViewModeBtnSizeBox->AddChild(ViewModeButton);
			UHorizontalBoxSlot* VMSlot = ButtonRow->AddChildToHorizontalBox(ViewModeBtnSizeBox);
			if (VMSlot)
			{
				VMSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				VMSlot->SetVerticalAlignment(VAlign_Center);
				VMSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
			}
			ViewModeLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ViewModeLabel"));
			ViewModeLabel->SetText(FText::FromString(TEXT("RGB")));
			ViewModeLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			ViewModeLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10));
			ViewModeButton->AddChild(ViewModeLabel);

			// Data stream option cycling button (hidden until config has options) — wrapped in SizeBox
			OptionBtnSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("OptionBtnSizeBox"));
			OptionBtnSizeBox->SetMinDesiredWidth(32.0f);
			OptionBtnSizeBox->SetMinDesiredHeight(32.0f);
			OptionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("OptionButton"));
			OptionButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			OptionBtnSizeBox->AddChild(OptionButton);
			UHorizontalBoxSlot* OptSlot = ButtonRow->AddChildToHorizontalBox(OptionBtnSizeBox);
			if (OptSlot)
			{
				OptSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				OptSlot->SetVerticalAlignment(VAlign_Center);
				OptSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
			}
			OptionLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("OptionLabel"));
			OptionLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.8f, 1.0f)));
			OptionLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10));
			OptionButton->AddChild(OptionLabel);
			OptionBtnSizeBox->SetVisibility(ESlateVisibility::Collapsed);

			// Display mode cycle button — wrapped in SizeBox
			DisplayModeBtnSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DisplayModeBtnSizeBox"));
			DisplayModeBtnSizeBox->SetMinDesiredWidth(32.0f);
			DisplayModeBtnSizeBox->SetMinDesiredHeight(32.0f);
			DisplayModeCycleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DisplayModeCycleButton"));
			DisplayModeCycleButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			DisplayModeBtnSizeBox->AddChild(DisplayModeCycleButton);
			UHorizontalBoxSlot* CycleBtnSlot = ButtonRow->AddChildToHorizontalBox(DisplayModeBtnSizeBox);
			if (CycleBtnSlot)
			{
				CycleBtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				CycleBtnSlot->SetVerticalAlignment(VAlign_Center);
				CycleBtnSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
			}
			DisplayModeCycleLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DisplayModeCycleLabel"));
			DisplayModeCycleLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.9f, 0.7f)));
			DisplayModeCycleLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10));
			DisplayModeCycleButton->AddChild(DisplayModeCycleLabel);

			// Wrapped label row (shown in narrow mode, below buttons)
			CameraLabelWrap = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CameraLabelWrap"));
			CameraLabelWrap->SetText(LabelText);
			CameraLabelWrap->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			CameraLabelWrap->SetAutoWrapText(true);
			CameraLabelWrap->SetVisibility(ESlateVisibility::Collapsed);
			UVerticalBoxSlot* WrapLabelSlot = TitleVBox->AddChildToVerticalBox(CameraLabelWrap);
			if (WrapLabelSlot)
			{
				WrapLabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				WrapLabelSlot->SetHorizontalAlignment(HAlign_Fill);
				WrapLabelSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
			}
		}
	}
}

void URammsCameraWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Build widget tree early so it exists before Slate representation is created
	// (required for widgets added as children of other panels)
	BuildWidgetTree();
}

void URammsCameraWidget::NativeConstruct()
{
	SetIsFocusable(true);

	UE_LOG(LogRammsCameraWidget, Log, TEXT("[%s] NativeConstruct: StreamID='%s' Provider=%s Slot=%s Parent=%s Visibility=%d"),
		*GetName(), *StreamID,
		CameraProvider.GetObject() ? *CameraProvider.GetObject()->GetName() : TEXT("null"),
		Slot ? *Slot->GetClass()->GetName() : TEXT("no-slot"),
		GetParent() ? *GetParent()->GetName() : TEXT("no-parent"),
		(int32)GetVisibility());

	Super::NativeConstruct();

	// SBox with aspect ratio constraint centers child vertically when VAlign=Fill.
	// Force VAlign_Top so the widget anchors at the top of its slot.
	if (ImageAspectRatioBox && bMaintainAspectRatio)
	{
		if (TSharedPtr<SBox> SlateSizeBox = StaticCastSharedPtr<SBox>(ImageAspectRatioBox->GetCachedWidget()))
		{
			SlateSizeBox->SetVAlign(VAlign_Top);
		}
	}

	// Bind collapse button
	if (CollapseButton && bCollapsible)
	{
		CollapseButton->OnClicked.AddUniqueDynamic(this, &URammsCameraWidget::OnCollapseClicked);
	}
	UpdateCollapseIcon();

	// Bind view mode button
	if (ViewModeButton)
	{
		ViewModeButton->OnClicked.AddUniqueDynamic(this, &URammsCameraWidget::OnViewModeClicked);
	}

	// Bind option cycling button
	if (OptionButton)
	{
		OptionButton->OnClicked.AddUniqueDynamic(this, &URammsCameraWidget::OnOptionClicked);
	}
	UpdateOptionButton();
	ApplyViewModeLayout();

	// Bind display mode cycle button
	if (DisplayModeCycleButton)
	{
		DisplayModeCycleButton->OnClicked.AddUniqueDynamic(this, &URammsCameraWidget::OnDisplayModeCycleClicked);
	}
	UpdateDisplayModeCycleButton();

	// Cache expanded slot size for Canvas Panel parents
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CachedExpandedSlotSize = CanvasSlot->GetSize();
	}

	// Auto-find camera providers if none pre-assigned
	if (!CameraProvider.GetInterface() && bAutoFindProvider)
	{
		if (UWorld* World = GetWorld())
		{
			// Collect all providers (actor-based and component-based)
			TArray<UObject*>			  ProviderObjs;
			TArray<IRammsCameraProvider*> ProviderIfaces;

			for (TActorIterator<AActor> It(World); It; ++It)
			{
				// Actor-based providers
				if (It->GetClass()->ImplementsInterface(URammsCameraProvider::StaticClass()))
				{
					ProviderObjs.Add(*It);
					ProviderIfaces.Add(Cast<IRammsCameraProvider>(*It));
				}

				// Component-based providers
				if (URammsCameraProviderComponent* Comp = It->FindComponentByClass<URammsCameraProviderComponent>())
				{
					if (!ProviderObjs.Contains(Comp))
					{
						ProviderObjs.Add(Comp);
						ProviderIfaces.Add(static_cast<IRammsCameraProvider*>(Comp));
					}
				}
			}

			// Prefer a provider that already has our StreamID registered
			for (int32 i = 0; i < ProviderIfaces.Num(); ++i)
			{
				for (const auto& Info : ProviderIfaces[i]->GetAvailableStreams())
				{
					if (Info.StreamID == StreamID)
					{
						CameraProvider.SetObject(ProviderObjs[i]);
						CameraProvider.SetInterface(ProviderIfaces[i]);
						break;
					}
				}
				if (CameraProvider.GetInterface())
					break;
			}

			// Fall back to first provider if none had the stream yet
			if (!CameraProvider.GetInterface() && ProviderIfaces.Num() > 0)
			{
				CameraProvider.SetObject(ProviderObjs[0]);
				CameraProvider.SetInterface(ProviderIfaces[0]);
			}

			// Subscribe to ALL discovered providers' frame delegates
			for (int32 i = 0; i < ProviderIfaces.Num(); ++i)
			{
				FProviderSubscription Sub;
				Sub.Object = ProviderObjs[i];
				Sub.Interface = ProviderIfaces[i];
				Sub.Handle = ProviderIfaces[i]->OnCameraFrameReady().AddUObject(
					this, &URammsCameraWidget::OnCameraFrameReady);
				ProviderSubscriptions.Add(Sub);
			}

			UE_LOG(LogRammsCameraWidget, Log, TEXT("[%s] AutoFind: Found %d providers, subscribed to all. StreamID='%s'"),
				*GetName(), ProviderIfaces.Num(), *StreamID);
		}
	}

	// Ensure preassigned CameraProvider is always subscribed
	if (CameraProvider.GetInterface() && ProviderSubscriptions.IsEmpty())
	{
		FProviderSubscription Sub;
		Sub.Object = CameraProvider.GetObject();
		Sub.Interface = CameraProvider.GetInterface();
		Sub.Handle = CameraProvider.GetInterface()->OnCameraFrameReady().AddUObject(
			this, &URammsCameraWidget::OnCameraFrameReady);
		ProviderSubscriptions.Add(Sub);
	}

	// Start camera stream if provider and stream ID are set
	if (CameraProvider.GetInterface() && !StreamID.IsEmpty())
	{
		StartStream();
	}
	else
	{
		UE_LOG(LogRammsCameraWidget, Warning, TEXT("[%s] NativeConstruct: NOT starting stream — Provider=%s StreamID='%s'"),
			*GetName(),
			CameraProvider.GetInterface() ? TEXT("set") : TEXT("null"),
			*StreamID);
	}

	// Auto-show bounding box overlay if configured
	if (bShowDetectionOverlay)
	{
		FName EffectiveTag = DetectionSourceTag;
		if (EffectiveTag.IsNone() && !StreamID.IsEmpty())
		{
			EffectiveTag = FName(*StreamID);
		}
		ShowBoundingBoxOverlay(EffectiveTag);
	}

	// Update initial layout
	UpdateLayout(false);
}

void URammsCameraWidget::NativeDestruct()
{
	UE_LOG(LogRammsCameraWidget, Log, TEXT("[%s] NativeDestruct: StreamID='%s' Subscriptions=%d"),
		*GetName(), *StreamID, ProviderSubscriptions.Num());

	UnregisterCorner();
	StopStream();

	// Unsubscribe from all providers' frame delegates
	for (auto& Sub : ProviderSubscriptions)
	{
		if (Sub.Object.IsValid() && Sub.Interface)
		{
			Sub.Interface->OnCameraFrameReady().Remove(Sub.Handle);
		}
	}
	ProviderSubscriptions.Empty();

	Super::NativeDestruct();
}

void URammsCameraWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// Push property changes from editor Details panel to visual widgets
	if (CameraLabel)
	{
		FText LabelText = CustomLabel.IsEmpty()
			? FText::FromString(StreamID.IsEmpty() ? TEXT("Camera") : StreamID)
			: CustomLabel;
		CameraLabel->SetText(LabelText);
		if (CameraLabelWrap)
		{
			CameraLabelWrap->SetText(LabelText);
		}
		// Label text changed — force header layout re-evaluation
		CachedHeaderCheckWidth = -1.0f;
	}

	// Sync view mode label
	ApplyViewModeLayout();

	// Sync collapse icon
	if (CollapseIcon)
	{
		UpdateCollapseIcon();
	}

	// Ensure SBox VAlign_Top for AR constraint (designer + runtime)
	if (ImageAspectRatioBox && bMaintainAspectRatio)
	{
		if (TSharedPtr<SBox> SlateSizeBox = StaticCastSharedPtr<SBox>(ImageAspectRatioBox->GetCachedWidget()))
		{
			SlateSizeBox->SetVAlign(VAlign_Top);
		}
	}

	// Update layout in designer so CornerSize/WindowedSize/AR are reflected
	UpdateLayout(false);
}

void URammsCameraWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Check if header needs to switch between single-line and two-row layout
	// (only when the title bar width actually changes — avoids per-frame GetDesiredSize calls)
	{
		float CurrentWidth = 0.0f;
		if (TitleBar)
		{
			CurrentWidth = TitleBar->GetCachedGeometry().GetLocalSize().X;
			if (CurrentWidth <= 0.0f)
				CurrentWidth = CachedExpandedSlotSize.X;
		}
		if (!FMath::IsNearlyEqual(CurrentWidth, CachedHeaderCheckWidth, 1.0f))
		{
			CachedHeaderCheckWidth = CurrentWidth;
			UpdateHeaderLayout();
		}
	}

	// Re-layout when the viewport size changes (handles Linux Vulkan late initialization,
	// window resizes, and any other viewport change after the initial NativeConstruct layout).
	// Also triggers when the viewport transitions from invalid (0,0) to a valid size.
	{
		FVector2D CurrentViewportSize(0, 0);
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(CurrentViewportSize);
		}
		if (CurrentViewportSize.X > 0.0f && CurrentViewportSize.Y > 0.0f
			&& !CurrentViewportSize.Equals(CachedLayoutViewportSize, 1.0f))
		{
			UpdateLayout(false);
		}
	}

	// Display mode transition animation (smooth size/position interpolation)
	if (bDisplayModeTransitioning)
	{
		float Speed = 4.0f; // ~0.25s
		DisplayModeTransitionProgress += Speed * InDeltaTime;
		DisplayModeTransitionProgress = FMath::Clamp(DisplayModeTransitionProgress, 0.0f, 1.0f);

		float Alpha = DisplayModeTransitionProgress * DisplayModeTransitionProgress
			* (3.0f - 2.0f * DisplayModeTransitionProgress); // smoothstep

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			FVector2D CurPos = FMath::Lerp(TransitionStartPos, TransitionTargetPos, Alpha);
			FVector2D CurSize = FMath::Lerp(TransitionStartSize, TransitionTargetSize, Alpha);
			CanvasSlot->SetPosition(CurPos);
			CanvasSlot->SetSize(CurSize);
		}

		if (DisplayModeTransitionProgress >= 1.0f)
		{
			bDisplayModeTransitioning = false;
			// Restore the target mode's actual anchor/alignment/position/size
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
			{
				CanvasSlot->SetAnchors(TransitionTargetAnchors);
				CanvasSlot->SetAlignment(TransitionTargetAlignment);
				CanvasSlot->SetPosition(TransitionTargetSlotPos);
				CanvasSlot->SetSize(TransitionTargetSlotSize);
				CachedExpandedSlotSize = TransitionTargetSlotSize;
			}
		}
	}

	// Collapse animation
	if (!bCollapseAnimating || !CameraSizeBox)
		return;

	// Advance collapse animation
	float Direction = (CollapseTarget > CollapseProgress) ? 1.0f : -1.0f;
	float Speed = 4.0f; // ~0.25s duration
	CollapseProgress += Direction * Speed * InDeltaTime;
	CollapseProgress = FMath::Clamp(CollapseProgress, 0.0f, 1.0f);

	float Alpha = CollapseProgress * CollapseProgress * (3.0f - 2.0f * CollapseProgress); // smoothstep

	// Animate content height via HeightOverride (Auto slot during animation)
	float ContentH = CachedContentHeight;
	if (ContentH <= 0.0f)
		ContentH = 200.0f;
	CameraSizeBox->SetHeightOverride(ContentH * Alpha);
	CameraSizeBox->SetRenderOpacity(Alpha);

	// Also animate Canvas Panel slot if applicable
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
	float			  TitleH = TitleBar ? TitleBar->GetDesiredSize().Y : 24.0f;
	if (TitleH <= 0.0f)
		TitleH = 24.0f;
	// Minimum slot height = title bar (CameraBorder is inside CameraSizeBox which collapses to 0)
	float MinSlotH = TitleH;

	if (CanvasSlot && CachedExpandedSlotSize.Y > 0.0f)
	{
		float SlotContentH = CachedExpandedSlotSize.Y - MinSlotH;
		float NewH = FMath::Max(MinSlotH + SlotContentH * Alpha, MinSlotH);
		CanvasSlot->SetSize(FVector2D(CachedExpandedSlotSize.X, NewH));
	}

	if (FMath::IsNearlyEqual(CollapseProgress, CollapseTarget, 0.01f))
	{
		CollapseProgress = CollapseTarget;
		bCollapseAnimating = false;

		if (CollapseTarget >= 1.0f)
		{
			// Fully expanded: Fill slot + clear HeightOverride for proper layout
			SetCameraSizeBoxSlotFill(true);
			CameraSizeBox->ClearHeightOverride();
			CameraSizeBox->SetRenderOpacity(1.0f);
			CameraSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			if (CanvasSlot && CachedExpandedSlotSize.Y > 0.0f)
			{
				CanvasSlot->SetSize(CachedExpandedSlotSize);
			}
			// Restore aspect ratio constraint now that fully expanded
			if (ImageAspectRatioBox && bMaintainAspectRatio)
			{
				float EffAR = GetEffectiveAspectRatio();
				ImageAspectRatioBox->SetMinAspectRatio(EffAR);
				ImageAspectRatioBox->SetMaxAspectRatio(EffAR);
			}
		}
		else
		{
			// Fully collapsed: Auto slot + HeightOverride(0) — stays in layout for width
			SetCameraSizeBoxSlotFill(false);
			CameraSizeBox->SetHeightOverride(0.0f);
			CameraSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
			CameraSizeBox->SetRenderOpacity(0.0f);
			// Clamp canvas slot to minimum header size
			if (CanvasSlot)
			{
				FVector2D SlotSize = CanvasSlot->GetSize();
				if (SlotSize.Y < MinSlotH)
				{
					CanvasSlot->SetSize(FVector2D(SlotSize.X, MinSlotH));
				}
			}
		}
	}
}

void URammsCameraWidget::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	float Radius = Style->Border.CornerRadiusMedium;
	float BorderW = Style->Border.BorderWidth;

	// Apply border styling with rounded corners
	if (CameraBorder)
	{
		if (bCollapsible)
		{
			// Bottom corners only — header has top corners
			FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrushEx(Style->Colors.Border, FVector4(0.0f, 0.0f, Radius, Radius));
			URammsUIStyle::ApplyRoundedBrushToBorder(CameraBorder, Brush);
		}
		else
		{
			FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrush(Style->Colors.Border, Radius);
			URammsUIStyle::ApplyRoundedBrushToBorder(CameraBorder, Brush);
		}
		CameraBorder->SetPadding(FMargin(BorderThickness));
	}

	// Update image corner radii to match inner border curve and layout mode
	UpdateImageCornerRadii();

	// Update title bar corner radii based on collapse state
	UpdateHeaderCornerRadii();

	// Apply label styling
	if (CameraLabel)
	{
		CameraLabel->SetFont(Style->Typography.Caption);
		CameraLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}
	if (CameraLabelWrap)
	{
		CameraLabelWrap->SetFont(Style->Typography.Caption);
		CameraLabelWrap->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}

	// Style the collapse icon to match other headers
	if (CollapseIcon)
	{
		CollapseIcon->SetFont(Style->Interaction.GetHeaderButtonFont(Style->Typography.Caption));
		CollapseIcon->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
	}

	// Style header action buttons (view mode / option)
	if (ViewModeLabel)
	{
		ViewModeLabel->SetFont(Style->Interaction.GetHeaderButtonFont(Style->Typography.Caption));
		ViewModeLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}

	if (OptionLabel)
	{
		OptionLabel->SetFont(Style->Interaction.GetHeaderButtonFont(Style->Typography.Caption));
		OptionLabel->SetColorAndOpacity(FSlateColor(Style->Colors.Info));
	}

	if (DisplayModeCycleLabel)
	{
		DisplayModeCycleLabel->SetFont(Style->Interaction.GetHeaderButtonFont(Style->Typography.Caption));
	}

	// Apply style-driven min touch target sizes to header button SizeBoxes
	{
		const float MinSz = Style->Interaction.HeaderButtonMinSize;
		if (CollapseBtnSizeBox)
		{
			CollapseBtnSizeBox->SetMinDesiredWidth(MinSz);
			CollapseBtnSizeBox->SetMinDesiredHeight(MinSz);
		}
		if (ViewModeBtnSizeBox)
		{
			ViewModeBtnSizeBox->SetMinDesiredWidth(MinSz);
			ViewModeBtnSizeBox->SetMinDesiredHeight(MinSz);
		}
		if (OptionBtnSizeBox)
		{
			OptionBtnSizeBox->SetMinDesiredWidth(MinSz);
			OptionBtnSizeBox->SetMinDesiredHeight(MinSz);
		}
		if (DisplayModeBtnSizeBox)
		{
			DisplayModeBtnSizeBox->SetMinDesiredWidth(MinSz);
			DisplayModeBtnSizeBox->SetMinDesiredHeight(MinSz);
		}
	}

	// Update title bar visibility
	if (TitleBar)
	{
		TitleBar->SetVisibility(bShowLabel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

		// Set label text
		FText LabelText = CustomLabel.IsEmpty() ? FText::FromString(StreamID.IsEmpty() ? TEXT("Camera") : StreamID) : CustomLabel;
		if (CameraLabel)
		{
			CameraLabel->SetText(LabelText);
		}
		if (CameraLabelWrap)
		{
			CameraLabelWrap->SetText(LabelText);
		}
		// Label/visibility changed — force header layout re-evaluation
		CachedHeaderCheckWidth = -1.0f;
	}
}

void URammsCameraWidget::SetCameraProvider(TScriptInterface<IRammsCameraProvider> Provider)
{
	// Stop existing streams
	StopStream();

	// Set new provider
	CameraProvider = Provider;

	// Add subscription if this provider isn't already tracked
	if (IRammsCameraProvider* Iface = Provider.GetInterface())
	{
		bool bAlreadySubscribed = false;
		for (const auto& Sub : ProviderSubscriptions)
		{
			if (Sub.Object.Get() == Provider.GetObject())
			{
				bAlreadySubscribed = true;
				break;
			}
		}
		if (!bAlreadySubscribed)
		{
			FProviderSubscription Sub;
			Sub.Object = Provider.GetObject();
			Sub.Interface = Iface;
			Sub.Handle = Iface->OnCameraFrameReady().AddUObject(
				this, &URammsCameraWidget::OnCameraFrameReady);
			ProviderSubscriptions.Add(Sub);
		}
	}

	// Start new stream if we have a stream ID
	if (!StreamID.IsEmpty())
	{
		StartStream();
	}
}

void URammsCameraWidget::SetStreamID(const FString& NewStreamID)
{
	if (StreamID == NewStreamID)
		return;

	// Stop existing stream
	StopStream();

	// Update stream ID
	StreamID = NewStreamID;

	// Update label
	if (CameraLabel && CustomLabel.IsEmpty())
	{
		CameraLabel->SetText(FText::FromString(StreamID));
	}
	if (CameraLabelWrap && CustomLabel.IsEmpty())
	{
		CameraLabelWrap->SetText(FText::FromString(StreamID));
	}

	// Start new stream
	if (CameraProvider.GetInterface())
	{
		StartStream();
	}
}

void URammsCameraWidget::SetDisplayMode(ERammsCameraDisplayMode NewMode, bool bAnimateTransition)
{
	if (DisplayMode == NewMode)
		return;

	UE_LOG(LogRammsCameraWidget, Log, TEXT("[%s] SetDisplayMode: %d -> %d (animate=%d)"),
		*GetName(), (int32)DisplayMode, (int32)NewMode, bAnimateTransition);

	// Cancel any active drag
	if (bIsDragging)
	{
		bIsDragging = false;
	}

	FVector2D CanvasSize = GetCanvasSize();

	// Cache current ABSOLUTE position + size for smooth transition
	if (bAnimateTransition)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			TransitionStartPos = ComputeSlotAbsoluteTopLeft(CanvasSlot, CanvasSize);
			TransitionStartSize = ComputeSlotAbsoluteSize(CanvasSlot, CanvasSize);
		}
	}

	DisplayMode = NewMode;
	ApplyViewModeLayout(); // Re-evaluate SBS orientation for new display mode
	UpdateLayout(false);   // Apply layout immediately (sets anchors, alignment, size)

	// Start smooth transition from old geometry to new (in absolute coordinates)
	if (bAnimateTransition)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			// Cache the target layout properties so we can restore them after animation
			TransitionTargetAnchors = CanvasSlot->GetAnchors();
			TransitionTargetAlignment = CanvasSlot->GetAlignment();
			TransitionTargetSlotPos = CanvasSlot->GetPosition();
			TransitionTargetSlotSize = CanvasSlot->GetSize();

			// Compute target absolute rect
			TransitionTargetPos = ComputeSlotAbsoluteTopLeft(CanvasSlot, CanvasSize);
			TransitionTargetSize = ComputeSlotAbsoluteSize(CanvasSlot, CanvasSize);

			// Switch to temporary point anchors at (0,0) for absolute-coordinate animation
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
			CanvasSlot->SetPosition(TransitionStartPos);
			CanvasSlot->SetSize(TransitionStartSize);

			bDisplayModeTransitioning = true;
			DisplayModeTransitionProgress = 0.0f;
		}
	}

	UpdateDisplayModeCycleButton();
}

void URammsCameraWidget::SetTexture(UTexture* Texture)
{
	CurrentTexture = Texture;
	UpdateDisplayedImages();
}

void URammsCameraWidget::SetDataTexture(UTexture* Texture)
{
	CurrentDataTexture = Texture;
	UpdateDisplayedImages();
}

void URammsCameraWidget::UpdateLayout(bool bAnimate)
{
	// Get viewport size in pixels
	FVector2D ViewportSize(1920, 1080);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
	if (ViewportScale <= 0.0f)
		ViewportScale = 1.0f;

	// Canvas Panel coordinate space = viewport pixels / DPI scale
	FVector2D CanvasSize = ViewportSize / ViewportScale;
	// Only cache valid viewport sizes so the NativeTick change-detection
	// can trigger a re-layout when the viewport goes from 0→valid (Linux Vulkan late init)
	if (ViewportSize.X > 0.0f && ViewportSize.Y > 0.0f)
	{
		CachedLayoutViewportSize = ViewportSize;
	}

	UE_LOG(LogRammsCameraWidget, Verbose, TEXT("[%s] UpdateLayout: Mode=%d ViewportSize=%.0fx%.0f Scale=%.2f CanvasSize=%.0fx%.0f Slot=%s IsInViewport=%d"),
		*GetName(), (int32)DisplayMode,
		ViewportSize.X, ViewportSize.Y, ViewportScale,
		CanvasSize.X, CanvasSize.Y,
		Slot ? *Slot->GetClass()->GetName() : TEXT("null"),
		IsInViewport());

	// Title bar height to reserve when collapsible (header sits above image)
	float TitleH = 0.0f;
	if (bCollapsible && TitleBar)
	{
		// Force Slate to measure the title bar subtree so GetDesiredSize returns
		// accurate values even on the very first layout pass (before any Slate tick).
		TitleBar->ForceLayoutPrepass();

		FVector2D TitleSize = TitleBar->GetDesiredSize();
		// Use style button size as fallback height before first Slate layout pass
		const float FallbackH = (Style ? Style->Interaction.HeaderButtonMinSize : 32.0f);
		TitleH = (TitleSize.Y > 0.0f) ? TitleSize.Y : FallbackH;
	}

	// Minimum width so header buttons never overflow the widget edge
	float MinHeaderWidth = 0.0f;
	if (TitleBar)
	{
		float BtnsW = 0.0f;
		// Use style-configured button min size as floor in case ForceLayoutPrepass
		// still returned zero (e.g. widget tree not yet fully realized).
		const float BtnMinSz = (Style ? Style->Interaction.HeaderButtonMinSize : 32.0f);
		auto		AddBtn = [&BtnsW, BtnMinSz](USizeBox* Wrapper, UWidget* Btn) {
			   // Prefer wrapper SizeBox (controls actual visibility/min-size); fall back to raw button
			   UWidget* W = Wrapper ? static_cast<UWidget*>(Wrapper) : Btn;
			   if (W && W->GetVisibility() != ESlateVisibility::Collapsed)
				   BtnsW += FMath::Max(W->GetDesiredSize().X, BtnMinSz) + 4.0f;
		};
		AddBtn(CollapseBtnSizeBox, CollapseButton);
		AddBtn(ViewModeBtnSizeBox, ViewModeButton);
		AddBtn(OptionBtnSizeBox, OptionButton);
		AddBtn(DisplayModeBtnSizeBox, DisplayModeCycleButton);
		// 16 = title bar horizontal padding (8px each side), 20 = minimum label space
		MinHeaderWidth = BtnsW + 16.0f + 20.0f;
	}

	// Detect if we're inside a Canvas Panel or added directly to viewport
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);

	// Determine if we're in a non-Canvas layout container (NamedSlot, Overlay, etc.)
	bool bInLayoutContainer = (!CanvasSlot && Slot != nullptr);

	switch (DisplayMode)
	{
		case ERammsCameraDisplayMode::Fullscreen:
		{
			UnregisterCorner();

			// Compute effective fullscreen bounds (screen percentage + padding)
			const bool bCustomFullscreen = (FullscreenSize.X < 1.0f || FullscreenSize.Y < 1.0f
				|| FullscreenPadding.X > 0.0f || FullscreenPadding.Y > 0.0f);

			if (!bCustomFullscreen)
			{
				// Default: stretch anchors fill entire canvas (original behavior)
				if (CanvasSlot)
				{
					CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
					CanvasSlot->SetOffsets(FMargin(0, 0, 0, 0));
				}
				else if (bInLayoutContainer)
				{
					if (InternalSizeBox)
					{
						InternalSizeBox->ClearWidthOverride();
						InternalSizeBox->ClearHeightOverride();
					}
				}
				else
				{
					SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
					SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
					SetPositionInViewport(FVector2D::ZeroVector, false);
					SetDesiredSizeInViewport(FVector2D::ZeroVector);
				}
			}
			else
			{
				// Custom fullscreen: apply size percentage and padding
				FVector2D MaxBounds;
				MaxBounds.X = CanvasSize.X * FullscreenSize.X - 2.0f * FullscreenPadding.X;
				MaxBounds.Y = CanvasSize.Y * FullscreenSize.Y - 2.0f * FullscreenPadding.Y;
				MaxBounds.X = FMath::Max(MaxBounds.X, 1.0f);
				MaxBounds.Y = FMath::Max(MaxBounds.Y, 1.0f);

				FVector2D SizeInUnits;
				if (bMaintainAspectRatio && AspectRatio > 0.0f)
				{
					const bool		bSBS = (ViewMode == ERammsCameraViewMode::SideBySide);
					constexpr float SBSGap = 2.0f;

					float AvailH = MaxBounds.Y - TitleH;
					if (AvailH < 1.0f)
						AvailH = 1.0f;

					float FitW, FitH;
					if (MaxBounds.X / AspectRatio <= AvailH)
					{
						FitW = MaxBounds.X;
						FitH = MaxBounds.X / AspectRatio;
					}
					else
					{
						FitH = AvailH;
						FitW = AvailH * AspectRatio;
					}

					if (bSBS && bSBSVertical)
					{
						float NeededH = 2.0f * FitH + SBSGap;
						float MaxH = FMath::Max(MaxBounds.Y - TitleH, SBSGap + 2.0f);
						if (NeededH > MaxH)
						{
							FitH = FMath::Max((MaxH - SBSGap) * 0.5f, 1.0f);
							FitW = FitH * AspectRatio;
						}
						SizeInUnits.X = FitW;
						SizeInUnits.Y = 2.0f * FitH + SBSGap + TitleH;
					}
					else if (bSBS && !bSBSVertical)
					{
						float NeededW = 2.0f * FitW + SBSGap;
						if (NeededW > MaxBounds.X)
						{
							FitW = FMath::Max((MaxBounds.X - SBSGap) * 0.5f, 1.0f);
							FitH = FitW / AspectRatio;
						}
						SizeInUnits.X = 2.0f * FitW + SBSGap;
						SizeInUnits.Y = FitH + TitleH;
					}
					else
					{
						SizeInUnits.X = FitW;
						SizeInUnits.Y = FitH + TitleH;
					}
				}
				else
				{
					SizeInUnits = MaxBounds;
				}

				SizeInUnits = ClampToMinHeaderWidth(SizeInUnits, MinHeaderWidth, TitleH);

				if (CanvasSlot)
				{
					CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
					CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
					CanvasSlot->SetPosition(FVector2D::ZeroVector);
					CanvasSlot->SetSize(SizeInUnits);
					CachedExpandedSlotSize = SizeInUnits;
				}
				else if (bInLayoutContainer)
				{
					if (InternalSizeBox)
					{
						InternalSizeBox->SetWidthOverride(SizeInUnits.X);
						InternalSizeBox->SetHeightOverride(SizeInUnits.Y);
					}
					CachedExpandedSlotSize = SizeInUnits;
				}
				else
				{
					FVector2D Position = (CanvasSize - SizeInUnits) * 0.5f;
					SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
					SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
					SetDesiredSizeInViewport(SizeInUnits);
					SetPositionInViewport(Position, false);
				}
			}

			WidgetPosition = FVector2D::ZeroVector;
			if (bAnimate)
				ScaleIn();
			break;
		}

		case ERammsCameraDisplayMode::Windowed:
		{
			UnregisterCorner();
			FVector2D MaxBounds;
			MaxBounds.X = CanvasSize.X * WindowedSize.X;
			MaxBounds.Y = CanvasSize.Y * WindowedSize.Y;

			FVector2D SizeInUnits;
			if (bMaintainAspectRatio && AspectRatio > 0.0f)
			{
				const bool		bSBS = (ViewMode == ERammsCameraViewMode::SideBySide);
				constexpr float SBSGap = 2.0f;

				float AvailH = MaxBounds.Y - TitleH;
				if (AvailH < 1.0f)
					AvailH = 1.0f;

				// Fit one image within max bounds
				float FitW, FitH;
				if (MaxBounds.X / AspectRatio <= AvailH)
				{
					FitW = MaxBounds.X;
					FitH = MaxBounds.X / AspectRatio;
				}
				else
				{
					FitH = AvailH;
					FitW = AvailH * AspectRatio;
				}

				if (bSBS && bSBSVertical)
				{
					// Vertical stack: double height, clamp to screen
					float NeededH = 2.0f * FitH + SBSGap;
					float MaxH = FMath::Max(CanvasSize.Y - TitleH, SBSGap + 2.0f);
					if (NeededH > MaxH)
					{
						FitH = FMath::Max((MaxH - SBSGap) * 0.5f, 1.0f);
						FitW = FitH * AspectRatio;
					}
					SizeInUnits.X = FitW;
					SizeInUnits.Y = 2.0f * FitH + SBSGap + TitleH;
				}
				else if (bSBS && !bSBSVertical)
				{
					// Horizontal stack: double width, clamp to screen
					float NeededW = 2.0f * FitW + SBSGap;
					if (NeededW > CanvasSize.X)
					{
						FitW = FMath::Max((CanvasSize.X - SBSGap) * 0.5f, 1.0f);
						FitH = FitW / AspectRatio;
					}
					SizeInUnits.X = 2.0f * FitW + SBSGap;
					SizeInUnits.Y = FitH + TitleH;
				}
				else
				{
					SizeInUnits.X = FitW;
					SizeInUnits.Y = FitH + TitleH;
				}
			}
			else
			{
				SizeInUnits = MaxBounds;
			}

			// Enforce minimum width so header buttons stay within bounds
			SizeInUnits = ClampToMinHeaderWidth(SizeInUnits, MinHeaderWidth, TitleH);

			if (CanvasSlot)
			{
				// Use point anchors (center of canvas) so drag moves position, not offsets
				CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
				CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				CanvasSlot->SetSize(SizeInUnits);
				// Preserve any previously dragged position across layout refreshes.
				// A default WidgetPosition of ZeroVector still keeps the initial centered behavior.
				if (!bIsDragging)
				{
					CanvasSlot->SetPosition(WidgetPosition);
				}
				CachedExpandedSlotSize = SizeInUnits;
			}
			else if (bInLayoutContainer)
			{
				// In layout container: constrain size via InternalSizeBox
				if (InternalSizeBox)
				{
					InternalSizeBox->SetWidthOverride(SizeInUnits.X);
					InternalSizeBox->SetHeightOverride(SizeInUnits.Y);
				}
				CachedExpandedSlotSize = SizeInUnits;
			}
			else
			{
				// Viewport-direct: center on screen
				FVector2D Position = (CanvasSize - SizeInUnits) * 0.5f;

				SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
				SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
				SetDesiredSizeInViewport(SizeInUnits);
				SetPositionInViewport(Position, false);
				WidgetPosition = Position;
			}
			if (bAnimate)
				ScaleIn();
			break;
		}

		case ERammsCameraDisplayMode::Corner:
		{
			RegisterCorner();

			FVector2D MaxBounds;
			MaxBounds.X = CanvasSize.X * CornerSize.X;
			MaxBounds.Y = CanvasSize.Y * CornerSize.Y;

			FVector2D WidgetSize;
			if (bMaintainAspectRatio && AspectRatio > 0.0f)
			{
				const bool		bSBS = (ViewMode == ERammsCameraViewMode::SideBySide);
				constexpr float SBSGap = 2.0f;

				// Available space for image = max bounds minus title bar
				float AvailH = MaxBounds.Y - TitleH;
				if (AvailH < 1.0f)
					AvailH = 1.0f;

				// Fit AR within MaxBounds.X x AvailH
				float FitW, FitH;
				if (MaxBounds.X / AspectRatio <= AvailH)
				{
					FitW = MaxBounds.X;
					FitH = MaxBounds.X / AspectRatio;
				}
				else
				{
					FitH = AvailH;
					FitW = AvailH * AspectRatio;
				}

				if (bSBS && bSBSVertical)
				{
					// Vertical stack: double height, clamp to screen
					float NeededH = 2.0f * FitH + SBSGap;
					float MaxH = FMath::Max(CanvasSize.Y - TitleH, SBSGap + 2.0f);
					if (NeededH > MaxH)
					{
						FitH = FMath::Max((MaxH - SBSGap) * 0.5f, 1.0f);
						FitW = FitH * AspectRatio;
					}
					WidgetSize.X = FitW;
					WidgetSize.Y = 2.0f * FitH + SBSGap + TitleH;
				}
				else if (bSBS && !bSBSVertical)
				{
					// Horizontal stack: double width, clamp to screen
					float NeededW = 2.0f * FitW + SBSGap;
					if (NeededW > CanvasSize.X)
					{
						FitW = FMath::Max((CanvasSize.X - SBSGap) * 0.5f, 1.0f);
						FitH = FitW / AspectRatio;
					}
					WidgetSize.X = 2.0f * FitW + SBSGap;
					WidgetSize.Y = FitH + TitleH;
				}
				else
				{
					WidgetSize.X = FitW;
					WidgetSize.Y = FitH + TitleH;
				}
			}
			else
			{
				WidgetSize = MaxBounds;
			}

			// Enforce minimum width so header buttons stay within bounds
			WidgetSize = ClampToMinHeaderWidth(WidgetSize, MinHeaderWidth, TitleH);

			// Compute anchor and alignment from corner alignment enums
			float	  AnchorX = (CornerHAlign == HAlign_Right) ? 1.0f : (CornerHAlign == HAlign_Center ? 0.5f : 0.0f);
			float	  AnchorY = (CornerVAlign == VAlign_Bottom) ? 1.0f : (CornerVAlign == VAlign_Center ? 0.5f : 0.0f);
			FVector2D Alignment(AnchorX, AnchorY);

			// Padding offset: positive = inward from edge
			FVector2D PadOffset(
				(AnchorX > 0.5f) ? -CornerPadding.X : (AnchorX < 0.5f ? CornerPadding.X : 0.0f),
				(AnchorY > 0.5f) ? -CornerPadding.Y : (AnchorY < 0.5f ? CornerPadding.Y : 0.0f));

			// Corner stacking: offset based on cumulative size of preceding widgets
			int32 StackIndex = GetCornerStackIndex();
			if (StackIndex > 0)
			{
				bool  bStackVertically = !WidgetSize.IsNearlyZero() ? (WidgetSize.X >= WidgetSize.Y) : (AspectRatio >= 1.0f || !bMaintainAspectRatio);
				float CumulativeOffset = 0.0f;

				const TArray<TWeakObjectPtr<URammsCameraWidget>>* Stack = CornerRegistry.Find(RegisteredCornerKey);
				if (Stack)
				{
					for (int32 i = 0; i < StackIndex && i < Stack->Num(); ++i)
					{
						URammsCameraWidget* Prev = (*Stack)[i].Get();
						if (!Prev)
							continue;
						FVector2D PrevSize = Prev->CachedExpandedSlotSize;
						if (PrevSize.IsNearlyZero())
							PrevSize = WidgetSize; // Fallback to own size
						if (bStackVertically)
							CumulativeOffset += PrevSize.Y + CornerStackGap;
						else
							CumulativeOffset += PrevSize.X + CornerStackGap;
					}
				}

				if (bStackVertically)
				{
					float StackDir = (AnchorY > 0.5f) ? -1.0f : 1.0f;
					PadOffset.Y += StackDir * CumulativeOffset;
				}
				else
				{
					float StackDir = (AnchorX > 0.5f) ? -1.0f : 1.0f;
					PadOffset.X += StackDir * CumulativeOffset;
				}
			}

			if (CanvasSlot)
			{
				CanvasSlot->SetAnchors(FAnchors(AnchorX, AnchorY, AnchorX, AnchorY));
				CanvasSlot->SetAlignment(Alignment);
				CanvasSlot->SetPosition(PadOffset);
				CanvasSlot->SetSize(WidgetSize);
				CachedExpandedSlotSize = WidgetSize;
			}
			else if (bInLayoutContainer)
			{
				// In layout container: constrain size via InternalSizeBox
				if (InternalSizeBox)
				{
					InternalSizeBox->SetWidthOverride(WidgetSize.X);
					InternalSizeBox->SetHeightOverride(WidgetSize.Y);
				}
				CachedExpandedSlotSize = WidgetSize;
			}
			else
			{
				// Compute absolute position for viewport-direct
				FVector2D AnchorPos = CanvasSize * FVector2D(AnchorX, AnchorY);
				FVector2D Position = AnchorPos - WidgetSize * Alignment + PadOffset;

				SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
				SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
				SetDesiredSizeInViewport(WidgetSize);
				SetPositionInViewport(Position, false);
				WidgetPosition = Position;
			}

			float SlideDir = (AnchorX > 0.5f) ? 200.0f : -200.0f;
			if (bAnimate)
				SlideIn(FVector2D(SlideDir, 0));
			break;
		}

		case ERammsCameraDisplayMode::Widget:
		{
			UnregisterCorner();
			// Widget mode: no viewport-percentage sizing.
			// Size is determined by parent layout slot or explicit SizeBox overrides.
			if (InternalSizeBox)
			{
				InternalSizeBox->ClearWidthOverride();
				InternalSizeBox->ClearHeightOverride();
			}

			// Apply aspect ratio constraint via ImageAspectRatioBox if available
			if (ImageAspectRatioBox && bMaintainAspectRatio && AspectRatio > 0.0f)
			{
				const float EffectiveAR = GetEffectiveAspectRatio();
				ImageAspectRatioBox->SetMinAspectRatio(EffectiveAR);
				ImageAspectRatioBox->SetMaxAspectRatio(EffectiveAR);
			}

			// Cache current size for collapse animation
			if (CanvasSlot)
			{
				CachedExpandedSlotSize = CanvasSlot->GetSize();
			}

			if (bAnimate)
				ScaleIn();
			break;
		}
	}

	UpdateDisplayModeCycleButton();
	ApplyZOrder();
}

void URammsCameraWidget::SetZOrder(int32 NewBaseZOrder)
{
	BaseZOrder = NewBaseZOrder;
	ApplyZOrder();
}

int32 URammsCameraWidget::GetEffectiveZOrder() const
{
	int32 Z = BaseZOrder + FocusZOrderBoost;
	if (DisplayMode == ERammsCameraDisplayMode::Fullscreen)
	{
		Z += FullscreenZOrderBoost;
	}
	return Z;
}

// ── Bounding Box Overlay ─────────────────────────────────────────

void URammsCameraWidget::ShowBoundingBoxOverlay(FName SourceTag)
{
	if (!BBoxOverlay && ImageContainerOverlay)
	{
		APlayerController* PC = GetOwningPlayer();
		BBoxOverlay = PC
			? CreateWidget<URammsBoundingBoxOverlay>(PC)
			: CreateWidget<URammsBoundingBoxOverlay>(this);
		if (BBoxOverlay)
		{
			// Configure before adding to tree (NativeConstruct reads these)
			if (!SourceTag.IsNone())
			{
				BBoxOverlay->SourceTagFilter = SourceTag;
				BBoxOverlay->bAutoSubscribe = true;
			}

			// Forward lifetime setting
			BBoxOverlay->DetectionLifetime = DetectionLifetime;

			// Set pane count based on current view mode
			BBoxOverlay->PaneCount = (ViewMode == ERammsCameraViewMode::SideBySide) ? 2 : 1;

			// Propagate style
			if (Style)
			{
				BBoxOverlay->SetStyle(Style);
			}

			// Insert into the image container overlay (covers only image area, not header)
			UOverlaySlot* OverlaySlot = ImageContainerOverlay->AddChildToOverlay(BBoxOverlay);
			if (OverlaySlot)
			{
				OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
				OverlaySlot->SetVerticalAlignment(VAlign_Fill);
			}
		}
	}
	else if (BBoxOverlay)
	{
		// Overlay already exists — update source tag and ensure subscription
		if (!SourceTag.IsNone())
		{
			const bool bTagChanged = BBoxOverlay->SourceTagFilter != SourceTag;
			BBoxOverlay->SourceTagFilter = SourceTag;
			BBoxOverlay->bAutoSubscribe = true;
			if (bTagChanged)
			{
				BBoxOverlay->UnsubscribeFromSubsystem();
				BBoxOverlay->ClearDetections();
			}
		}
		// Re-subscribe (safe if already subscribed — uses AddUniqueDynamic)
		if (BBoxOverlay->bAutoSubscribe)
		{
			BBoxOverlay->SubscribeToSubsystem();
		}
	}

	bBBoxOverlayEnabled = true;
	if (BBoxOverlay)
	{
		BBoxOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void URammsCameraWidget::HideBoundingBoxOverlay()
{
	bBBoxOverlayEnabled = false;
	if (BBoxOverlay)
	{
		BBoxOverlay->ClearDetections();
		BBoxOverlay->UnsubscribeFromSubsystem();
		BBoxOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URammsCameraWidget::SetDetections(const TArray<FRammsBoundingBox>& InBoxes)
{
	if (!BBoxOverlay)
	{
		ShowBoundingBoxOverlay();
	}

	if (BBoxOverlay)
	{
		BBoxOverlay->SetDetections(InBoxes);
	}
}

void URammsCameraWidget::ClearDetections()
{
	if (BBoxOverlay)
	{
		BBoxOverlay->ClearDetections();
	}
}

bool URammsCameraWidget::IsBoundingBoxOverlayVisible() const
{
	return BBoxOverlay && BBoxOverlay->GetVisibility() != ESlateVisibility::Collapsed;
}

void URammsCameraWidget::ApplyZOrder()
{
	int32 EffectiveZ = GetEffectiveZOrder();

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CanvasSlot->SetZOrder(EffectiveZ);
	}
	else if (IsInViewport() && EffectiveZ != CachedAppliedZOrder)
	{
		// Update viewport z-order via GameViewportSubsystem to avoid
		// RemoveFromParent/AddToViewport which triggers NativeDestruct/NativeConstruct
		if (UWorld* World = GetWorld())
		{
			if (UGameViewportSubsystem* Subsystem = UGameViewportSubsystem::Get(World))
			{
				FGameViewportWidgetSlot SlotInfo = Subsystem->GetWidgetSlot(this);
				SlotInfo.ZOrder = EffectiveZ;
				Subsystem->SetWidgetSlot(this, SlotInfo);
			}
		}
	}

	CachedAppliedZOrder = EffectiveZ;
}

FVector2D URammsCameraWidget::GetCanvasSize() const
{
	FVector2D ViewportSize(1920, 1080);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
	if (ViewportScale <= 0.0f)
		ViewportScale = 1.0f;
	return ViewportSize / ViewportScale;
}

FVector2D URammsCameraWidget::ComputeSlotAbsoluteTopLeft(UCanvasPanelSlot* CanvasSlot, const FVector2D& CanvasSize) const
{
	if (!CanvasSlot)
		return FVector2D::ZeroVector;

	FAnchors Anchors = CanvasSlot->GetAnchors();
	bool	 bStretch = !FMath::IsNearlyEqual(Anchors.Minimum.X, Anchors.Maximum.X)
		|| !FMath::IsNearlyEqual(Anchors.Minimum.Y, Anchors.Maximum.Y);

	if (bStretch)
	{
		FMargin Offsets = CanvasSlot->GetOffsets();
		return FVector2D(
			CanvasSize.X * Anchors.Minimum.X + Offsets.Left,
			CanvasSize.Y * Anchors.Minimum.Y + Offsets.Top);
	}
	else
	{
		FVector2D Anchor(Anchors.Minimum.X, Anchors.Minimum.Y);
		FVector2D Pos = CanvasSlot->GetPosition();
		FVector2D Size = CanvasSlot->GetSize();
		FVector2D Align = CanvasSlot->GetAlignment();
		return CanvasSize * Anchor + Pos - Size * Align;
	}
}

FVector2D URammsCameraWidget::ComputeSlotAbsoluteSize(UCanvasPanelSlot* CanvasSlot, const FVector2D& CanvasSize) const
{
	if (!CanvasSlot)
		return FVector2D::ZeroVector;

	FAnchors Anchors = CanvasSlot->GetAnchors();
	bool	 bStretch = !FMath::IsNearlyEqual(Anchors.Minimum.X, Anchors.Maximum.X)
		|| !FMath::IsNearlyEqual(Anchors.Minimum.Y, Anchors.Maximum.Y);

	if (bStretch)
	{
		FMargin Offsets = CanvasSlot->GetOffsets();
		return FVector2D(
			CanvasSize.X * (Anchors.Maximum.X - Anchors.Minimum.X) - Offsets.Left - Offsets.Right,
			CanvasSize.Y * (Anchors.Maximum.Y - Anchors.Minimum.Y) - Offsets.Top - Offsets.Bottom);
	}
	else
	{
		return CanvasSlot->GetSize();
	}
}

uint8 URammsCameraWidget::MakeCornerKey(EHorizontalAlignment H, EVerticalAlignment V)
{
	return static_cast<uint8>(H) * 4 + static_cast<uint8>(V);
}

void URammsCameraWidget::RegisterCorner()
{
	uint8				   CornerKey = MakeCornerKey(CornerHAlign, CornerVAlign);
	UWidget*			   ParentWidget = GetParent();
	TPair<UWidget*, uint8> Key(ParentWidget, CornerKey);

	// Already registered in the correct slot
	if (RegisteredCornerKey == Key)
		return;

	// Unregister from old slot if any
	UnregisterCorner();

	TArray<TWeakObjectPtr<URammsCameraWidget>>& Stack = CornerRegistry.FindOrAdd(Key);
	Stack.Add(this);
	RegisteredCornerKey = Key;
}

void URammsCameraWidget::UnregisterCorner()
{
	if (RegisteredCornerKey.Value == 0xFF)
		return;

	TPair<UWidget*, uint8> OldKey = RegisteredCornerKey;

	if (TArray<TWeakObjectPtr<URammsCameraWidget>>* Stack = CornerRegistry.Find(OldKey))
	{
		Stack->RemoveAll([this](const TWeakObjectPtr<URammsCameraWidget>& W) {
			return !W.IsValid() || W.Get() == this;
		});

		// Refresh layout for remaining widgets in this corner so they recompute stacking offsets
		for (const TWeakObjectPtr<URammsCameraWidget>& Sibling : *Stack)
		{
			if (Sibling.IsValid() && Sibling.Get() != this)
			{
				Sibling->UpdateLayout(false);
			}
		}

		if (Stack->Num() == 0)
		{
			CornerRegistry.Remove(OldKey);
		}
	}
	RegisteredCornerKey = { nullptr, 0xFF };
}

int32 URammsCameraWidget::GetCornerStackIndex() const
{
	if (RegisteredCornerKey.Value == 0xFF)
		return 0;

	const TArray<TWeakObjectPtr<URammsCameraWidget>>* Stack = CornerRegistry.Find(RegisteredCornerKey);
	if (!Stack)
		return 0;

	for (int32 i = 0; i < Stack->Num(); ++i)
	{
		if ((*Stack)[i].Get() == this)
			return i;
	}
	return 0;
}

void URammsCameraWidget::OnCameraFrameReady(const FString& InStreamID, UTexture* Texture, int64 Timestamp)
{
	if (InStreamID == StreamID)
	{
		if (!CurrentTexture && Texture)
		{
			UE_LOG(LogRammsCameraWidget, Log, TEXT("[%s] First RGB frame: StreamID='%s' Texture=%s %ux%u Visibility=%d CameraImage=%s"),
				*GetName(), *InStreamID,
				*Texture->GetName(),
				(uint32)Texture->GetSurfaceWidth(), (uint32)Texture->GetSurfaceHeight(),
				(int32)GetVisibility(),
				CameraImage ? TEXT("valid") : TEXT("null"));
		}
		CurrentTexture = Texture;

		// Auto-detect aspect ratio from incoming RGB texture
		if (bMaintainAspectRatio && bAutoDetectAspectRatio && Texture)
		{
			float TexW = static_cast<float>(Texture->GetSurfaceWidth());
			float TexH = static_cast<float>(Texture->GetSurfaceHeight());
			if (TexH > 0.0f)
			{
				float DetectedAR = TexW / TexH;
				if (!FMath::IsNearlyEqual(AspectRatio, DetectedAR, 0.01f))
				{
					AspectRatio = DetectedAR;
					if (ViewMode == ERammsCameraViewMode::SideBySide)
					{
						ApplyViewModeLayout();
					}
					if (ImageAspectRatioBox)
					{
						float EffAR = GetEffectiveAspectRatio();
						ImageAspectRatioBox->SetMinAspectRatio(EffAR);
						ImageAspectRatioBox->SetMaxAspectRatio(EffAR);
					}
					// Re-compute widget size now that AR changed
					UpdateLayout(false);
				}
			}
		}

		UpdateDisplayedImages();
	}
	else if (InStreamID == DataStreamID)
	{
		CurrentDataTexture = Texture;

		// Auto-detect depth format and refresh material params from provider
		// using lightweight accessors (avoids full FRammsCameraStreamInfo copy).
		bool bNeedParamUpdate = false;
		if (!DataStreamID.IsEmpty())
		{
			for (const auto& Sub : ProviderSubscriptions)
			{
				if (Sub.Object.IsValid() && Sub.Interface)
				{
					// Check if this provider serves our DataStreamID
					const TMap<FName, float>* StreamParams = Sub.Interface->GetStreamMaterialParams(DataStreamID);
					if (!StreamParams)
					{
						// Provider doesn't have this stream — try next provider
						continue;
					}

					// Depth format: detect once, cache for stream lifetime
					if (CachedDataDepthFormat == ERammsDepthFormat::Unknown)
					{
						ERammsDepthFormat Fmt = Sub.Interface->GetStreamDepthFormat(DataStreamID);
						if (Fmt != ERammsDepthFormat::Unknown)
						{
							CachedDataDepthFormat = Fmt;
							FString PixFmt = Sub.Interface->GetStreamPixelFormat(DataStreamID);
							UE_LOG(LogRammsCameraWidget, Log, TEXT("[%s] Auto-detected depth format=%d PixelFmt='%s' for DataStream='%s'"),
								*GetName(), static_cast<int32>(Fmt), *PixFmt, *DataStreamID);
							bNeedParamUpdate = true;
						}
					}

					// Material params: refresh whenever they differ (including clearing)
					if (!StreamParams->OrderIndependentCompareEqual(CachedStreamMaterialParams))
					{
						CachedStreamMaterialParams = *StreamParams;
						bNeedParamUpdate = true;
					}
					break;
				}
			}
		}
		if (bNeedParamUpdate)
		{
			UpdateDataMaterialParams();
		}

		UpdateDisplayedImages();
	}
}

void URammsCameraWidget::StartStream()
{
	// Delegate subscriptions are managed in NativeConstruct/NativeDestruct (widget lifetime).
	// Here we just tell providers to activate the stream.

	// Try StartStream on ALL subscribed providers (any of them might serve our stream)
	if (!StreamID.IsEmpty())
	{
		// If no subscriptions exist but CameraProvider is set, add it as a fallback
		if (ProviderSubscriptions.IsEmpty() && CameraProvider.GetInterface())
		{
			FProviderSubscription Sub;
			Sub.Object = CameraProvider.GetObject();
			Sub.Interface = CameraProvider.GetInterface();
			Sub.Handle = CameraProvider.GetInterface()->OnCameraFrameReady().AddUObject(
				this, &URammsCameraWidget::OnCameraFrameReady);
			ProviderSubscriptions.Add(Sub);
		}

		bool bStarted = false;
		for (auto& Sub : ProviderSubscriptions)
		{
			if (Sub.Object.IsValid() && Sub.Interface)
			{
				if (Sub.Interface->StartStream(StreamID))
				{
					bStarted = true;
				}
			}
		}
		UE_LOG(LogRammsCameraWidget, Log, TEXT("[%s] StartStream('%s') %s — %d subscriptions"), *GetName(), *StreamID,
			bStarted ? TEXT("succeeded") : TEXT("not yet registered (will receive when available)"),
			ProviderSubscriptions.Num());
	}

	// Start data stream if configured
	if (!DataStreamID.IsEmpty())
	{
		for (auto& Sub : ProviderSubscriptions)
		{
			if (Sub.Object.IsValid() && Sub.Interface)
			{
				Sub.Interface->StartStream(DataStreamID);
			}
		}
	}
}

void URammsCameraWidget::StopStream()
{
	// Stop streams on all providers (but keep delegate subscriptions alive)
	for (auto& Sub : ProviderSubscriptions)
	{
		if (Sub.Object.IsValid() && Sub.Interface)
		{
			if (!StreamID.IsEmpty())
			{
				Sub.Interface->StopStream(StreamID);
			}
			if (!DataStreamID.IsEmpty())
			{
				Sub.Interface->StopStream(DataStreamID);
			}
		}
	}
}

FReply URammsCameraWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Focus-on-click: bring windowed widget to front when clicked
	if (bFocusOnClick && DisplayMode == ERammsCameraDisplayMode::Windowed
		&& InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		FocusZOrderBoost = ++FocusZOrderCounter;
		ApplyZOrder();
	}

	// Only allow drag in Windowed mode (other modes compute position automatically)
	bool bCanDrag = bEnableDrag
		&& DisplayMode == ERammsCameraDisplayMode::Windowed;

	if (bCanDrag && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		bIsDragging = true;
		DragStartMousePos = InMouseEvent.GetScreenSpacePosition();

		// Read current position from canvas slot or tracked position
		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
		if (CanvasSlot)
		{
			DragStartWidgetPos = CanvasSlot->GetPosition();
		}
		else
		{
			DragStartWidgetPos = WidgetPosition;
		}

		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URammsCameraWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsDragging)
	{
		bIsDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply URammsCameraWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsDragging)
	{
		FVector2D CurrentMousePos = InMouseEvent.GetScreenSpacePosition();
		FVector2D Delta = CurrentMousePos - DragStartMousePos;

		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
		if (CanvasSlot)
		{
			// Mouse delta is in screen pixels; Canvas slot position is in DPI-scaled units
			float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
			if (Scale <= 0.0f)
				Scale = 1.0f;
			FVector2D NewPosition = DragStartWidgetPos + Delta / Scale;
			CanvasSlot->SetPosition(NewPosition);
			WidgetPosition = NewPosition;
		}
		else
		{
			FVector2D NewPosition = DragStartWidgetPos + Delta;
			SetPositionInViewport(NewPosition, true);
			WidgetPosition = NewPosition;
		}

		return FReply::Handled();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void URammsCameraWidget::ToggleCollapse()
{
	SetCameraCollapsed(!bCameraCollapsed);
}

void URammsCameraWidget::SetCameraCollapsed(bool bCollapsed)
{
	if (!bCollapsible || bCollapsed == bCameraCollapsed)
		return;

	bCameraCollapsed = bCollapsed;
	CollapseTarget = bCollapsed ? 0.0f : 1.0f;
	bCollapseAnimating = true;

	// Cache slot size only when collapsing (when expanding, keep the cached expanded size)
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		if (bCollapsed)
		{
			// About to collapse — cache the current (expanded) slot size
			FVector2D CurrentSize = CanvasSlot->GetSize();
			if (CurrentSize.Y > 0.0f)
			{
				CachedExpandedSlotSize = CurrentSize;
			}
		}
	}

	// Cache content height only when collapsing (SizeBox has valid geometry)
	if (CameraSizeBox)
	{
		if (bCollapsed)
		{
			float ActualH = CameraSizeBox->GetCachedGeometry().GetLocalSize().Y;
			if (ActualH > 0.0f)
			{
				CachedContentHeight = ActualH;
			}
			else if (CameraBorder)
			{
				FVector2D DesiredSize = CameraBorder->GetDesiredSize();
				if (DesiredSize.Y > 0.0f)
				{
					CachedContentHeight = DesiredSize.Y;
				}
			}
		}
		if (CachedContentHeight <= 0.0f)
		{
			CachedContentHeight = 200.0f;
		}

		// Switch to Auto slot for animation (HeightOverride controls actual space)
		SetCameraSizeBoxSlotFill(false);
		CameraSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		if (bCollapsed)
		{
			// Starting to collapse from expanded
			CameraSizeBox->SetHeightOverride(CachedContentHeight);
			CollapseProgress = 1.0f;
		}
		else
		{
			// Starting to expand from collapsed
			CameraSizeBox->SetHeightOverride(0.0f);
			CameraSizeBox->SetRenderOpacity(0.0f);
			CollapseProgress = 0.0f;
		}
	}

	UpdateCollapseIcon();
	UpdateHeaderCornerRadii();

	// Hide bounding box overlay while collapsed, restore only if intentionally enabled
	if (BBoxOverlay)
	{
		if (bCollapsed)
		{
			BBoxOverlay->UnsubscribeFromSubsystem();
			BBoxOverlay->SetVisibility(ESlateVisibility::Collapsed);
		}
		else if (bBBoxOverlayEnabled)
		{
			if (BBoxOverlay->bAutoSubscribe)
			{
				BBoxOverlay->SubscribeToSubsystem();
			}
			BBoxOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	// When collapsing: clear aspect ratio constraint immediately so the header
	// isn't forced to an image-sized AR.
	// When expanding: defer AR restore until animation completes (NativeTick)
	// to avoid the widget flashing to full expanded size before animating.
	if (ImageAspectRatioBox && bMaintainAspectRatio && bCollapsed)
	{
		ImageAspectRatioBox->ClearMinAspectRatio();
		ImageAspectRatioBox->ClearMaxAspectRatio();
	}

	// When expanding, set the canvas slot to the collapsed size so the animation
	// starts from the current (collapsed) size rather than jumping to zero or full.
	if (!bCollapsed)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			float TitleH = TitleBar ? TitleBar->GetDesiredSize().Y : 24.0f;
			if (TitleH <= 0.0f)
				TitleH = 24.0f;
			CanvasSlot->SetSize(FVector2D(CachedExpandedSlotSize.X, TitleH));
		}
	}
}

void URammsCameraWidget::OnCollapseClicked()
{
	ToggleCollapse();
}

void URammsCameraWidget::UpdateCollapseIcon()
{
	if (!CollapseIcon)
		return;

	CollapseIcon->SetText(FText::FromString(bCameraCollapsed ? TEXT("\u25B6") : TEXT("\u25BC")));
	CachedHeaderCheckWidth = -1.0f;
}

// --- View mode support ---

void URammsCameraWidget::SetViewMode(ERammsCameraViewMode NewViewMode)
{
	if (ViewMode == NewViewMode)
		return;

	ViewMode = NewViewMode;
	ApplyViewModeLayout();
	UpdateDisplayedImages();
}

void URammsCameraWidget::SetDataStreamConfig(const FRammsDataStreamMaterialConfig& Config)
{
	DataStreamConfig = Config;
	CurrentOptionIndex = 0;

	// Force recreation of dynamic material instances
	DataMID = nullptr;
	OverlayMID = nullptr;

	EnsureDataMaterials();
	UpdateDataMaterialParams();
	UpdateOptionButton();
	ApplyViewModeLayout();
	UpdateDisplayedImages();
}

void URammsCameraWidget::SetMaterialScalarParam(FName ParamName, float Value)
{
	DataStreamConfig.ScalarParams.Add(ParamName, Value);
	UpdateDataMaterialParams();
	UpdateDisplayedImages();
}

void URammsCameraWidget::SetOverlayBlendAlpha(float Alpha)
{
	OverlayBlendAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	UpdateDataMaterialParams();
	UpdateDisplayedImages();
}

void URammsCameraWidget::SetDataStreamID(const FString& NewDataStreamID)
{
	if (DataStreamID == NewDataStreamID)
		return;

	// Stop existing data stream on all providers
	if (!DataStreamID.IsEmpty())
	{
		for (auto& Sub : ProviderSubscriptions)
		{
			if (Sub.Object.IsValid() && Sub.Interface)
			{
				Sub.Interface->StopStream(DataStreamID);
			}
		}
	}

	DataStreamID = NewDataStreamID;
	CurrentDataTexture = nullptr;
	CachedDataDepthFormat = ERammsDepthFormat::Unknown;
	CachedStreamMaterialParams.Empty();
	bOverlayDiagLogged = false;

	// Start new data stream
	if (!DataStreamID.IsEmpty())
	{
		for (auto& Sub : ProviderSubscriptions)
		{
			if (Sub.Object.IsValid() && Sub.Interface)
			{
				Sub.Interface->StartStream(DataStreamID);
			}
		}
	}
}

void URammsCameraWidget::SetStreams(const FString& RGBStreamID, const FString& NewDataStreamID)
{
	SetStreamID(RGBStreamID);
	SetDataStreamID(NewDataStreamID);
}

void URammsCameraWidget::ConfigureStreams(const FString& RGBStreamID, const FString& NewDataStreamID,
	const FRammsDataStreamMaterialConfig& Config)
{
	SetDataStreamConfig(Config);
	SetStreams(RGBStreamID, NewDataStreamID);
}

void URammsCameraWidget::OnViewModeClicked()
{
	// Cycle: RGB -> Data -> SideBySide -> Overlay -> RGB
	switch (ViewMode)
	{
		case ERammsCameraViewMode::RGB:
			SetViewMode(ERammsCameraViewMode::Data);
			break;
		case ERammsCameraViewMode::Data:
			SetViewMode(ERammsCameraViewMode::SideBySide);
			break;
		case ERammsCameraViewMode::SideBySide:
			SetViewMode(ERammsCameraViewMode::Overlay);
			break;
		case ERammsCameraViewMode::Overlay:
			SetViewMode(ERammsCameraViewMode::RGB);
			break;
	}
}

void URammsCameraWidget::OnOptionClicked()
{
	if (DataStreamConfig.Options.Num() == 0)
		return;

	int32 NextIndex = (CurrentOptionIndex + 1) % DataStreamConfig.Options.Num();
	SetOptionIndex(NextIndex);
}

void URammsCameraWidget::SetOptionIndex(int32 Index)
{
	if (DataStreamConfig.Options.Num() == 0 || !DataStreamConfig.OptionParamName.IsValid())
		return;

	CurrentOptionIndex = FMath::Clamp(Index, 0, DataStreamConfig.Options.Num() - 1);
	const FRammsDataStreamOption& Opt = DataStreamConfig.Options[CurrentOptionIndex];

	// Push the option value into ScalarParams and update materials
	DataStreamConfig.ScalarParams.Add(DataStreamConfig.OptionParamName, Opt.Value);
	UpdateDataMaterialParams();
	UpdateDisplayedImages();
	UpdateOptionButton();
}

void URammsCameraWidget::UpdateOptionButton()
{
	if (!OptionButton)
		return;

	USizeBox* WrapperBox = OptionBtnSizeBox;
	if (DataStreamConfig.Options.Num() > 0 && DataStreamConfig.OptionParamName.IsValid())
	{
		if (WrapperBox)
			WrapperBox->SetVisibility(ESlateVisibility::Visible);
		else
			OptionButton->SetVisibility(ESlateVisibility::Visible);
		if (OptionLabel)
		{
			int32 SafeIdx = FMath::Clamp(CurrentOptionIndex, 0, DataStreamConfig.Options.Num() - 1);
			OptionLabel->SetText(DataStreamConfig.Options[SafeIdx].DisplayName);
		}
	}
	else
	{
		if (WrapperBox)
			WrapperBox->SetVisibility(ESlateVisibility::Collapsed);
		else
			OptionButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	// Button visibility changed — force header layout re-evaluation
	CachedHeaderCheckWidth = -1.0f;
}

FString URammsCameraWidget::GetDisplayModeShortLabel(ERammsCameraDisplayMode Mode)
{
	switch (Mode)
	{
		case ERammsCameraDisplayMode::Fullscreen:
			return TEXT("\u25A0"); // ■ (fullscreen)
		case ERammsCameraDisplayMode::Windowed:
			return TEXT("\u25A1"); // □ (window)
		case ERammsCameraDisplayMode::Corner:
			return TEXT("\u250C"); // ┌ (corner)
		case ERammsCameraDisplayMode::Widget:
			return TEXT("\u25A3"); // ▣ (widget)
		default:
			return TEXT("?");
	}
}

void URammsCameraWidget::UpdateDisplayModeCycleButton()
{
	if (!DisplayModeCycleButton)
		return;

	bool			 bVisible = bShowDisplayModeCycleButton && AllowedDisplayModes.Num() > 1;
	ESlateVisibility Vis = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (DisplayModeBtnSizeBox)
		DisplayModeBtnSizeBox->SetVisibility(Vis);
	else
		DisplayModeCycleButton->SetVisibility(Vis);

	if (DisplayModeCycleLabel)
	{
		DisplayModeCycleLabel->SetText(FText::FromString(GetDisplayModeShortLabel(DisplayMode)));
	}
	// Button visibility/text changed — force header layout re-evaluation
	CachedHeaderCheckWidth = -1.0f;
}

void URammsCameraWidget::OnDisplayModeCycleClicked()
{
	CycleDisplayMode();
}

void URammsCameraWidget::CycleDisplayMode()
{
	if (AllowedDisplayModes.Num() <= 1)
		return;

	int32 CurrentIdx = AllowedDisplayModes.IndexOfByKey(DisplayMode);
	if (CurrentIdx == INDEX_NONE)
		CurrentIdx = 0;

	int32 NextIdx = (CurrentIdx + 1) % AllowedDisplayModes.Num();
	SetDisplayMode(AllowedDisplayModes[NextIdx], true);
}

void URammsCameraWidget::UpdateHeaderLayout()
{
	if (!TitleBar || !ButtonRow || !CameraLabel)
		return;

	// Use the actual rendered width, not the desired/ideal width
	FGeometry TitleGeo = TitleBar->GetCachedGeometry();
	float	  TitleBarWidth = TitleGeo.GetLocalSize().X;
	if (TitleBarWidth <= 0.0f)
	{
		// Fallback to cached slot size if geometry isn't available yet
		if (CachedExpandedSlotSize.X > 0.0f)
		{
			TitleBarWidth = CachedExpandedSlotSize.X;
		}
		else
		{
			return; // Can't determine width yet
		}
	}

	// Measure the minimum width needed by buttons (all Auto-sized children except the Fill label)
	float ButtonsMinWidth = 0.0f;
	if (CollapseButton && CollapseButton->GetVisibility() != ESlateVisibility::Collapsed)
	{
		ButtonsMinWidth += CollapseButton->GetDesiredSize().X + 4.0f;
	}
	if (ViewModeButton && ViewModeButton->GetVisibility() != ESlateVisibility::Collapsed)
	{
		ButtonsMinWidth += ViewModeButton->GetDesiredSize().X + 4.0f;
	}
	if (OptionButton && OptionButton->GetVisibility() != ESlateVisibility::Collapsed)
	{
		ButtonsMinWidth += OptionButton->GetDesiredSize().X + 4.0f;
	}
	if (DisplayModeCycleButton && DisplayModeCycleButton->GetVisibility() != ESlateVisibility::Collapsed)
	{
		ButtonsMinWidth += DisplayModeCycleButton->GetDesiredSize().X + 4.0f;
	}

	// Account for title bar padding (8px each side)
	float AvailableForLabel = TitleBarWidth - ButtonsMinWidth - 16.0f;

	// Compare against the actual desired width of the label text
	float LabelDesiredWidth = CameraLabel->GetDesiredSize().X;
	if (LabelDesiredWidth < 20.0f)
		LabelDesiredWidth = 20.0f; // Floor to prevent flicker with empty labels
	bool bShouldBeNarrow = (AvailableForLabel < LabelDesiredWidth);

	if (bShouldBeNarrow != bHeaderNarrowMode)
	{
		bHeaderNarrowMode = bShouldBeNarrow;

		if (bHeaderNarrowMode)
		{
			// Switch to narrow: hide inline label text but keep it as a Fill spacer
			// so buttons stay right-justified. Show wrapped label below.
			CameraLabel->SetVisibility(ESlateVisibility::Hidden);
			if (CameraLabelWrap)
			{
				CameraLabelWrap->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
		}
		else
		{
			// Switch to wide: show inline label, hide wrapped label
			CameraLabel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			if (CameraLabelWrap)
			{
				CameraLabelWrap->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

void URammsCameraWidget::SetMaintainAspectRatio(bool bMaintain)
{
	if (bMaintainAspectRatio == bMaintain)
		return;

	bMaintainAspectRatio = bMaintain;

	if (ImageAspectRatioBox)
	{
		if (bMaintainAspectRatio)
		{
			float EffAR = GetEffectiveAspectRatio();
			ImageAspectRatioBox->SetMinAspectRatio(EffAR);
			ImageAspectRatioBox->SetMaxAspectRatio(EffAR);
		}
		else
		{
			ImageAspectRatioBox->ClearMinAspectRatio();
			ImageAspectRatioBox->ClearMaxAspectRatio();
		}
	}
	UpdateLayout(false);
}

void URammsCameraWidget::SetAspectRatio(float NewAspectRatio)
{
	bAutoDetectAspectRatio = false;
	AspectRatio = FMath::Clamp(NewAspectRatio, 0.1f, 10.0f);

	if (ViewMode == ERammsCameraViewMode::SideBySide)
	{
		ApplyViewModeLayout();
	}

	if (ImageAspectRatioBox && bMaintainAspectRatio)
	{
		float EffAR = GetEffectiveAspectRatio();
		ImageAspectRatioBox->SetMinAspectRatio(EffAR);
		ImageAspectRatioBox->SetMaxAspectRatio(EffAR);
	}
	UpdateLayout(false);
}

void URammsCameraWidget::ApplyViewModeLayout()
{
	// Update button label
	if (ViewModeLabel)
	{
		switch (ViewMode)
		{
			case ERammsCameraViewMode::RGB:
				ViewModeLabel->SetText(FText::FromString(TEXT("RGB")));
				break;
			case ERammsCameraViewMode::Data:
			{
				FText Label = DataStreamConfig.DisplayLabel.IsEmpty()
					? FText::FromString(TEXT("Data"))
					: DataStreamConfig.DisplayLabel;
				ViewModeLabel->SetText(Label);
				break;
			}
			case ERammsCameraViewMode::SideBySide:
				ViewModeLabel->SetText(FText::FromString(TEXT("SbS")));
				break;
			case ERammsCameraViewMode::Overlay:
				ViewModeLabel->SetText(FText::FromString(TEXT("Ovly")));
				break;
		}
	}

	const bool bSBS = (ViewMode == ERammsCameraViewMode::SideBySide);
	bool	   bLayoutChanged = false; // Track if sizing/orientation actually changed

	// Determine SBS orientation: landscape → vertical stack, portrait → horizontal
	// Fullscreen always uses horizontal (current behavior)
	bool bWantVertical = false;
	if (bSBS && DisplayMode != ERammsCameraDisplayMode::Fullscreen)
	{
		bWantVertical = (AspectRatio >= 1.0f);
	}

	// Reparent images between HBox and VBox when SBS orientation changes
	if (bSBS && CameraImage && DataImage && ImageHBox && ImageVBox)
	{
		if (bWantVertical != bSBSVertical || DataImage->GetVisibility() == ESlateVisibility::Collapsed)
		{
			bLayoutChanged = true;
			// Remove images from their current parents
			if (CameraImage->GetParent())
				CameraImage->RemoveFromParent();
			if (DataImage->GetParent())
				DataImage->RemoveFromParent();

			if (bWantVertical)
			{
				// Vertical stack: CameraImage on top, DataImage below
				UVerticalBoxSlot* RGBSlot = ImageVBox->AddChildToVerticalBox(CameraImage);
				if (RGBSlot)
				{
					RGBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
					RGBSlot->SetHorizontalAlignment(HAlign_Fill);
					RGBSlot->SetVerticalAlignment(VAlign_Fill);
				}
				UVerticalBoxSlot* DataSlot = ImageVBox->AddChildToVerticalBox(DataImage);
				if (DataSlot)
				{
					DataSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
					DataSlot->SetHorizontalAlignment(HAlign_Fill);
					DataSlot->SetVerticalAlignment(VAlign_Fill);
					DataSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
				}
				ImageVBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				ImageHBox->SetVisibility(ESlateVisibility::Collapsed);
			}
			else
			{
				// Horizontal stack: CameraImage left, DataImage right
				UHorizontalBoxSlot* RGBSlot = ImageHBox->AddChildToHorizontalBox(CameraImage);
				if (RGBSlot)
				{
					RGBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
					RGBSlot->SetHorizontalAlignment(HAlign_Fill);
					RGBSlot->SetVerticalAlignment(VAlign_Fill);
				}
				UHorizontalBoxSlot* DataSlot = ImageHBox->AddChildToHorizontalBox(DataImage);
				if (DataSlot)
				{
					DataSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
					DataSlot->SetHorizontalAlignment(HAlign_Fill);
					DataSlot->SetVerticalAlignment(VAlign_Fill);
					DataSlot->SetPadding(FMargin(2.0f, 0.0f, 0.0f, 0.0f));
				}
				ImageHBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				ImageVBox->SetVisibility(ESlateVisibility::Collapsed);
			}

			DataImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			bSBSVertical = bWantVertical;
		}
	}
	else if (!bSBS)
	{
		// Detect layout change: DataImage transitioning from visible (SBS) to collapsed
		if (DataImage && DataImage->GetVisibility() != ESlateVisibility::Collapsed)
			bLayoutChanged = true;

		// Leaving SBS — move images back to HBox if they're in VBox
		if (bSBSVertical && CameraImage && DataImage && ImageHBox && ImageVBox)
		{
			bLayoutChanged = true;
			if (CameraImage->GetParent())
				CameraImage->RemoveFromParent();
			if (DataImage->GetParent())
				DataImage->RemoveFromParent();

			UHorizontalBoxSlot* RGBSlot = ImageHBox->AddChildToHorizontalBox(CameraImage);
			if (RGBSlot)
			{
				RGBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				RGBSlot->SetHorizontalAlignment(HAlign_Fill);
				RGBSlot->SetVerticalAlignment(VAlign_Fill);
			}
			UHorizontalBoxSlot* DataSlot = ImageHBox->AddChildToHorizontalBox(DataImage);
			if (DataSlot)
			{
				DataSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				DataSlot->SetHorizontalAlignment(HAlign_Fill);
				DataSlot->SetVerticalAlignment(VAlign_Fill);
				DataSlot->SetPadding(FMargin(2.0f, 0.0f, 0.0f, 0.0f));
			}
			bSBSVertical = false;
		}

		// Show HBox, hide VBox; collapse DataImage
		if (ImageHBox)
			ImageHBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (ImageVBox)
			ImageVBox->SetVisibility(ESlateVisibility::Collapsed);
		if (DataImage)
			DataImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Update bbox overlay pane count for split view
	if (BBoxOverlay)
	{
		const int32 NewPaneCount = bSBS ? 2 : 1;
		if (BBoxOverlay->PaneCount != NewPaneCount)
		{
			BBoxOverlay->PaneCount = NewPaneCount;
			BBoxOverlay->InvalidateLayoutAndVolatility();
		}
	}

	// Only refresh layout + siblings when SBS orientation or visibility actually changed
	if (bLayoutChanged)
	{
		// Adjust corner radii based on whether side-by-side or single image
		UpdateImageCornerRadii();

		// Update aspect ratio box for SBS-adjusted AR
		if (ImageAspectRatioBox && bMaintainAspectRatio && AspectRatio > 0.0f)
		{
			float EffAR = GetEffectiveAspectRatio();
			ImageAspectRatioBox->SetMinAspectRatio(EffAR);
			ImageAspectRatioBox->SetMaxAspectRatio(EffAR);
		}

		// SBS may change widget sizing — refresh layout
		UpdateLayout(false);

		// SBS size change may affect corner siblings — refresh them
		if (DisplayMode == ERammsCameraDisplayMode::Corner && RegisteredCornerKey.Value != 0xFF)
		{
			if (const TArray<TWeakObjectPtr<URammsCameraWidget>>* Stack = CornerRegistry.Find(RegisteredCornerKey))
			{
				for (const TWeakObjectPtr<URammsCameraWidget>& Sibling : *Stack)
				{
					if (Sibling.IsValid() && Sibling.Get() != this)
					{
						Sibling->UpdateLayout(false);
					}
				}
			}
		}
	}

	// Header content changed — force header layout re-evaluation
	CachedHeaderCheckWidth = -1.0f;
}

float URammsCameraWidget::GetEffectiveAspectRatio() const
{
	bool bSBS = (ViewMode == ERammsCameraViewMode::SideBySide);
	if (bSBS && bSBSVertical)
		return AspectRatio / 2.0f;
	if (bSBS)
		return AspectRatio * 2.0f;
	return AspectRatio;
}

FVector2D URammsCameraWidget::ClampToMinHeaderWidth(FVector2D Size, float MinWidth, float InTitleH) const
{
	if (MinWidth <= 0.0f || Size.X >= MinWidth)
		return Size;

	if (!bMaintainAspectRatio || AspectRatio <= 0.0f)
	{
		Size.X = MinWidth;
		return Size;
	}

	const bool		bSBS = (ViewMode == ERammsCameraViewMode::SideBySide);
	constexpr float SBSGap = 2.0f;

	if (bSBS && bSBSVertical)
	{
		// Vertical stack: widget width = single image width
		float FitW = MinWidth;
		float FitH = FitW / AspectRatio;
		Size.X = FitW;
		Size.Y = 2.0f * FitH + SBSGap + InTitleH;
	}
	else if (bSBS && !bSBSVertical)
	{
		// Horizontal stack: widget width = 2 * image width + gap
		float FitW = FMath::Max((MinWidth - SBSGap) * 0.5f, 1.0f);
		float FitH = FitW / AspectRatio;
		Size.X = MinWidth;
		Size.Y = FitH + InTitleH;
	}
	else
	{
		// Single view
		Size.X = MinWidth;
		Size.Y = MinWidth / AspectRatio + InTitleH;
	}

	return Size;
}

void URammsCameraWidget::UpdateImageCornerRadii()
{
	// Corner masking is handled by materials via CornerRadii vector parameter.
	// This function computes per-corner radii based on layout mode and sets them
	// on the appropriate MIDs. The shader uses these to mask corners.
	float OuterRadius = Style ? Style->Border.CornerRadiusMedium : 4.0f;
	float R = FMath::Max(OuterRadius - BorderThickness, 0.0f);
	bool  bSideBySide = (ViewMode == ERammsCameraViewMode::SideBySide);

	// FVector4 CornerRadii: X=TopLeft, Y=TopRight, Z=BottomRight, W=BottomLeft
	FVector4 RGBRadii, DataRadii;

	if (bCollapsible)
	{
		// Collapsible: header above, so top corners are always 0
		if (bSideBySide && bSBSVertical)
		{
			// Vertical stack: RGB on top (no corners), Data on bottom (bottom corners)
			RGBRadii = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
			DataRadii = FVector4(0.0f, 0.0f, R, R);
		}
		else if (bSideBySide)
		{
			// Horizontal stack: RGB left (bottom-left), Data right (bottom-right)
			RGBRadii = FVector4(0.0f, 0.0f, 0.0f, R);
			DataRadii = FVector4(0.0f, 0.0f, R, 0.0f);
		}
		else
		{
			RGBRadii = FVector4(0.0f, 0.0f, R, R);
			DataRadii = FVector4(0.0f, 0.0f, R, R);
		}
	}
	else
	{
		// Non-collapsible: title overlays on image, all corners available
		if (bSideBySide && bSBSVertical)
		{
			// Vertical stack: RGB on top (top corners), Data on bottom (bottom corners)
			RGBRadii = FVector4(R, R, 0.0f, 0.0f);
			DataRadii = FVector4(0.0f, 0.0f, R, R);
		}
		else if (bSideBySide)
		{
			// Horizontal stack: RGB left (left corners), Data right (right corners)
			RGBRadii = FVector4(R, 0.0f, 0.0f, R);
			DataRadii = FVector4(0.0f, R, R, 0.0f);
		}
		else
		{
			RGBRadii = FVector4(R, R, R, R);
			DataRadii = FVector4(R, R, R, R);
		}
	}

	// Store for use in UpdateMaterialCornerParams
	CachedRGBCornerRadii = RGBRadii;
	CachedDataCornerRadii = DataRadii;
	// Invalidate material param caches so next UpdateMaterialCornerParams pushes new radii
	LastMaterialImageSize_RGB = FVector2D::ZeroVector;
	LastMaterialImageSize_Data = FVector2D::ZeroVector;
}

void URammsCameraWidget::UpdateHeaderCornerRadii()
{
	if (!TitleBar)
		return;

	float OuterRadius = Style ? Style->Border.CornerRadiusMedium : 4.0f;
	float R = FMath::Max(OuterRadius - BorderThickness, 0.0f);

	FVector4 Radii;
	if (bCollapsible)
	{
		// Collapsible: header is a separate row above the image
		// Collapsed = header IS the whole widget, needs all corners
		// Expanded = header on top, only top corners
		if (bCameraCollapsed)
			Radii = FVector4(R, R, R, R);
		else
			Radii = FVector4(R, R, 0.0f, 0.0f);
	}
	else
	{
		// Non-collapsible: header overlays at top of image, always top corners only
		Radii = FVector4(R, R, 0.0f, 0.0f);
	}

	FLinearColor TitleBg = Style ? Style->Colors.Background : FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);
	if (Style)
		TitleBg.A = 0.7f;
	FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrushEx(TitleBg, Radii);
	URammsUIStyle::ApplyRoundedBrushToBorder(TitleBar, Brush);
	if (Style)
	{
		TitleBar->SetPadding(Style->Interaction.HeaderPadding);
	}
}

void URammsCameraWidget::EnsureDataMaterials()
{
	// Create passthrough MIDs for RGB display (with material-based corner masking)
	if (PassthroughMaterial && !PassthroughMID_RGB)
	{
		PassthroughMID_RGB = UMaterialInstanceDynamic::Create(PassthroughMaterial, this);
	}
	if (PassthroughMaterial && !PassthroughMID_Data)
	{
		PassthroughMID_Data = UMaterialInstanceDynamic::Create(PassthroughMaterial, this);
	}

	// Create data visualization MID if material is assigned and MID doesn't exist
	if (DataStreamConfig.VisualizationMaterial && !DataMID)
	{
		DataMID = UMaterialInstanceDynamic::Create(DataStreamConfig.VisualizationMaterial, this);
		UE_LOG(LogRammsCameraWidget, Log, TEXT("[%s] Created DataMID from '%s', DepthFormat=%d"),
			*GetName(), *DataStreamConfig.VisualizationMaterial->GetName(), static_cast<int32>(CachedDataDepthFormat));
		UpdateDataMaterialParams();
	}

	// Create overlay blend MID if material is assigned and MID doesn't exist
	if (DataStreamConfig.OverlayMaterial && !OverlayMID)
	{
		OverlayMID = UMaterialInstanceDynamic::Create(DataStreamConfig.OverlayMaterial, this);
		UE_LOG(LogRammsCameraWidget, Log, TEXT("[%s] Created OverlayMID from '%s', DepthFormat=%d, RGBParam='%s' DataParam='%s' BlendParam='%s'"),
			*GetName(), *DataStreamConfig.OverlayMaterial->GetName(), static_cast<int32>(CachedDataDepthFormat),
			*DataStreamConfig.RGBTextureParam.ToString(), *DataStreamConfig.DataTextureParam.ToString(),
			*DataStreamConfig.BlendAlphaParam.ToString());
		UpdateDataMaterialParams();
	}
}

void URammsCameraWidget::UpdateDataMaterialParams()
{
	// Compute depth format params from auto-detected format
	float DepthUnnormalize = 1.0f;
	float DepthScaleToCM = 1.0f;
	if (CachedDataDepthFormat == ERammsDepthFormat::Uint16MM)
	{
		DepthUnnormalize = 65535.0f; // G16 texture is GPU-normalized to [0,1]
		DepthScaleToCM = 0.1f;		 // mm → cm
	}

	UE_LOG(LogRammsCameraWidget, Verbose, TEXT("[%s] UpdateDataMaterialParams: DepthFormat=%d Unnorm=%.1f ScaleToCM=%.3f DataTex=%s RGBTex=%s"),
		*GetName(), static_cast<int32>(CachedDataDepthFormat), DepthUnnormalize, DepthScaleToCM,
		CurrentDataTexture ? *CurrentDataTexture->GetName() : TEXT("null"),
		CurrentTexture ? *CurrentTexture->GetName() : TEXT("null"));

	// Apply all scalar params from config, then depth format params, to both materials
	auto ApplyParams = [&](UMaterialInstanceDynamic* MID) {
		if (!MID)
		{
			return;
		}

		for (auto It = GPreviouslyAppliedDynamicScalarParams.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
		}

		TSet<FName> CurrentDynamicKeys;
		for (const auto& Pair : CachedStreamMaterialParams)
		{
			CurrentDynamicKeys.Add(Pair.Key);
		}

		const TSet<FName>* PreviouslyAppliedKeys = GPreviouslyAppliedDynamicScalarParams.Find(MID);
		if (PreviouslyAppliedKeys)
		{
			for (const FName& PreviouslyAppliedKey : *PreviouslyAppliedKeys)
			{
				if (!CurrentDynamicKeys.Contains(PreviouslyAppliedKey))
				{
					if (const float* DefaultValue = DataStreamConfig.ScalarParams.Find(PreviouslyAppliedKey))
					{
						MID->SetScalarParameterValue(PreviouslyAppliedKey, *DefaultValue);
					}
					else
					{
						MID->SetScalarParameterValue(PreviouslyAppliedKey, 0.0f);
					}
				}
			}
		}

		// Static config params (widget-level defaults)
		for (const auto& Pair : DataStreamConfig.ScalarParams)
		{
			MID->SetScalarParameterValue(Pair.Key, Pair.Value);
		}
		// Dynamic per-stream params from metadata (override static config)
		for (const auto& Pair : CachedStreamMaterialParams)
		{
			MID->SetScalarParameterValue(Pair.Key, Pair.Value);
		}
		// Auto-injected depth format params (materials can use these to normalize depth)
		MID->SetScalarParameterValue(FName("DepthUnnormalize"), DepthUnnormalize);
		MID->SetScalarParameterValue(FName("DepthScaleToCM"), DepthScaleToCM);

		GPreviouslyAppliedDynamicScalarParams.Add(MID, MoveTemp(CurrentDynamicKeys));
	};

	if (DataMID)
	{
		ApplyParams(DataMID);
		if (CurrentDataTexture)
		{
			DataMID->SetTextureParameterValue(DataStreamConfig.DataTextureParam, CurrentDataTexture);
		}
	}

	if (OverlayMID)
	{
		ApplyParams(OverlayMID);
		OverlayMID->SetScalarParameterValue(DataStreamConfig.BlendAlphaParam, OverlayBlendAlpha);
		if (CurrentTexture)
		{
			OverlayMID->SetTextureParameterValue(DataStreamConfig.RGBTextureParam, CurrentTexture);
		}
		if (CurrentDataTexture)
		{
			OverlayMID->SetTextureParameterValue(DataStreamConfig.DataTextureParam, CurrentDataTexture);
		}
	}
}

void URammsCameraWidget::SetImageBrushFromTexture(UImage* Image, UTexture* Texture)
{
	if (!Image || !Texture)
	{
		UE_LOG(LogRammsCameraWidget, Verbose, TEXT("[%s] SetImageBrushFromTexture: skipped — Image=%s Texture=%s"),
			*GetName(), Image ? TEXT("valid") : TEXT("null"), Texture ? TEXT("valid") : TEXT("null"));
		return;
	}

	EnsureDataMaterials();

	// If passthrough material is available, route through it for post-processing
	// compatibility and shader-based corner masking
	UMaterialInstanceDynamic* MID = (Image == DataImage) ? PassthroughMID_Data : PassthroughMID_RGB;
	if (MID)
	{
		MID->SetTextureParameterValue(TEXT("Texture"), Texture);
		UpdateMaterialCornerParams(MID, Image);

		// Only rebuild the brush if the resource or image dimensions changed
		const FSlateBrush& CurrentBrush = Image->GetBrush();
		float			   TexW = static_cast<float>(Texture->GetSurfaceWidth());
		float			   TexH = static_cast<float>(Texture->GetSurfaceHeight());
		if (CurrentBrush.GetResourceObject() == MID
			&& FMath::IsNearlyEqual(CurrentBrush.ImageSize.X, TexW, 0.5f)
			&& FMath::IsNearlyEqual(CurrentBrush.ImageSize.Y, TexH, 0.5f))
		{
			return; // Brush is already configured — MID texture param update above is sufficient
		}

		SetImageBrushFromMaterial(Image, MID, Texture, DataRT);
		return;
	}

	// Fallback: direct texture binding (no corner masking, may break with post-processing)
	const FSlateBrush& CurrentBrush = Image->GetBrush();
	float			   TexW = static_cast<float>(Texture->GetSurfaceWidth());
	float			   TexH = static_cast<float>(Texture->GetSurfaceHeight());
	if (CurrentBrush.GetResourceObject() == Texture
		&& FMath::IsNearlyEqual(CurrentBrush.ImageSize.X, TexW, 0.5f)
		&& FMath::IsNearlyEqual(CurrentBrush.ImageSize.Y, TexH, 0.5f))
	{
		return; // Same texture, same dimensions — nothing to rebuild
	}

	FSlateBrush Brush = CurrentBrush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetResourceObject(Texture);
	if (TexW > 0.0f && TexH > 0.0f)
	{
		Brush.ImageSize = FVector2D(TexW, TexH);
	}
	Image->SetBrush(Brush);
}

void URammsCameraWidget::SetImageBrushFromMaterial(UImage* Image, UMaterialInstanceDynamic* MID,
	UTexture* SizeSource, TObjectPtr<UTextureRenderTarget2D>& RenderTarget)
{
	if (!Image || !MID)
		return;

	// Set corner/size params for shader-based rounded masking
	UpdateMaterialCornerParams(MID, Image);

	// Skip brush rebuild if the resource and image size haven't changed
	const FSlateBrush& CurrentBrush = Image->GetBrush();
	float			   TexW = 0.0f, TexH = 0.0f;
	if (SizeSource)
	{
		TexW = static_cast<float>(SizeSource->GetSurfaceWidth());
		TexH = static_cast<float>(SizeSource->GetSurfaceHeight());
	}
	if (CurrentBrush.GetResourceObject() == MID
		&& (TexW <= 0.0f || (FMath::IsNearlyEqual(CurrentBrush.ImageSize.X, TexW, 0.5f) && FMath::IsNearlyEqual(CurrentBrush.ImageSize.Y, TexH, 0.5f))))
	{
		return; // Brush already configured with same MID and dimensions
	}

	FSlateBrush Brush = CurrentBrush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetResourceObject(MID);

	if (TexW > 0.0f && TexH > 0.0f)
	{
		Brush.ImageSize = FVector2D(TexW, TexH);
	}

	Image->SetBrush(Brush);
}

void URammsCameraWidget::UpdateMaterialCornerParams(UMaterialInstanceDynamic* MID, UImage* Image)
{
	if (!MID)
		return;

	// Select per-corner radii based on which image widget this is
	FVector4 Radii = (Image == DataImage) ? CachedDataCornerRadii : CachedRGBCornerRadii;

	// Get current widget pixel size for UV-to-pixel mapping
	FVector2D Size = FVector2D::ZeroVector;
	if (Image)
	{
		Size = Image->GetCachedGeometry().GetLocalSize();
	}

	// Check if anything actually changed — skip redundant material param sets
	FVector2D&								  LastSize = (Image == DataImage) ? LastMaterialImageSize_Data : LastMaterialImageSize_RGB;
	TWeakObjectPtr<UMaterialInstanceDynamic>& LastMID = (Image == DataImage) ? LastCornerMID_Data : LastCornerMID_RGB;
	// Bypass cache when MID changed (e.g., switching between PassthroughMID_RGB and DataMID)
	bool bMIDChanged = (LastMID.Get() != MID);
	if (!bMIDChanged && Size.X > 0.0f && Size.Y > 0.0f && FMath::IsNearlyEqual(Size.X, LastSize.X, 0.5f) && FMath::IsNearlyEqual(Size.Y, LastSize.Y, 0.5f))
	{
		return; // Same MID, same size — nothing changed
	}
	LastSize = Size;
	LastMID = MID;

	MID->SetVectorParameterValue(TEXT("CornerRadii"), FLinearColor(Radii.X, Radii.Y, Radii.Z, Radii.W));

	// Individual scalar params (works with any Custom node input setup)
	MID->SetScalarParameterValue(TEXT("CornerTL"), Radii.X);
	MID->SetScalarParameterValue(TEXT("CornerTR"), Radii.Y);
	MID->SetScalarParameterValue(TEXT("CornerBR"), Radii.Z);
	MID->SetScalarParameterValue(TEXT("CornerBL"), Radii.W);

	// Also set uniform CornerRadius as max of the four (convenience for simple materials)
	float MaxR = FMath::Max(FMath::Max(Radii.X, Radii.Y), FMath::Max(Radii.Z, Radii.W));
	MID->SetScalarParameterValue(TEXT("CornerRadius"), MaxR);

	// Pass widget pixel size so the shader can compute UV-to-pixel mapping
	if (Size.X > 0.0f && Size.Y > 0.0f)
	{
		MID->SetVectorParameterValue(TEXT("ImageSize"), FLinearColor(Size.X, Size.Y, 0.0f, 0.0f));
	}
}

void URammsCameraWidget::SetCameraSizeBoxSlotFill(bool bFill)
{
	if (!CameraSizeBox)
		return;

	if (UVerticalBoxSlot* VBSlot = Cast<UVerticalBoxSlot>(CameraSizeBox->Slot))
	{
		VBSlot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
	}
}

void URammsCameraWidget::UpdateDisplayedImages()
{
	EnsureDataMaterials();

	switch (ViewMode)
	{
		case ERammsCameraViewMode::RGB:
			if (CameraImage && CurrentTexture)
			{
				SetImageBrushFromTexture(CameraImage, CurrentTexture);
				CameraImage->SetColorAndOpacity(FLinearColor::White);
			}
			break;

		case ERammsCameraViewMode::Data:
			if (CameraImage && CurrentDataTexture)
			{
				if (DataMID)
				{
					DataMID->SetTextureParameterValue(DataStreamConfig.DataTextureParam, CurrentDataTexture);
					SetImageBrushFromMaterial(CameraImage, DataMID, CurrentDataTexture, DataRT);
				}
				else
				{
					SetImageBrushFromTexture(CameraImage, CurrentDataTexture);
				}
				CameraImage->SetColorAndOpacity(FLinearColor::White);
			}
			break;

		case ERammsCameraViewMode::SideBySide:
			if (CameraImage && CurrentTexture)
			{
				SetImageBrushFromTexture(CameraImage, CurrentTexture);
				CameraImage->SetColorAndOpacity(FLinearColor::White);
			}
			if (DataImage && CurrentDataTexture)
			{
				if (DataMID)
				{
					DataMID->SetTextureParameterValue(DataStreamConfig.DataTextureParam, CurrentDataTexture);
					SetImageBrushFromMaterial(DataImage, DataMID, CurrentDataTexture, DataRT);
				}
				else
				{
					SetImageBrushFromTexture(DataImage, CurrentDataTexture);
				}
				DataImage->SetColorAndOpacity(FLinearColor::White);
			}
			break;

		case ERammsCameraViewMode::Overlay:
			if (CameraImage && CurrentTexture)
			{
				if (OverlayMID && CurrentDataTexture)
				{
					// One-time diagnostic for overlay material debugging (Vulkan depth issues)
					if (!bOverlayDiagLogged)
					{
						bOverlayDiagLogged = true;
						UTexture2D* DataTex2D = Cast<UTexture2D>(CurrentDataTexture);
						UTexture2D* RGBTex2D = Cast<UTexture2D>(CurrentTexture);
						UE_LOG(LogRammsCameraWidget, Log,
							TEXT("[%s] Overlay first render: DepthFormat=%d RGBTex=%s(%dx%d PF=%d) DataTex=%s(%dx%d PF=%d) BlendAlpha=%.2f Material='%s'"),
							*GetName(), static_cast<int32>(CachedDataDepthFormat),
							CurrentTexture ? *CurrentTexture->GetName() : TEXT("null"),
							RGBTex2D ? RGBTex2D->GetSizeX() : 0, RGBTex2D ? RGBTex2D->GetSizeY() : 0,
							RGBTex2D ? static_cast<int32>(RGBTex2D->GetPixelFormat()) : -1,
							CurrentDataTexture ? *CurrentDataTexture->GetName() : TEXT("null"),
							DataTex2D ? DataTex2D->GetSizeX() : 0, DataTex2D ? DataTex2D->GetSizeY() : 0,
							DataTex2D ? static_cast<int32>(DataTex2D->GetPixelFormat()) : -1,
							OverlayBlendAlpha,
							*DataStreamConfig.OverlayMaterial->GetName());
					}

					OverlayMID->SetTextureParameterValue(DataStreamConfig.RGBTextureParam, CurrentTexture);
					OverlayMID->SetTextureParameterValue(DataStreamConfig.DataTextureParam, CurrentDataTexture);
					OverlayMID->SetScalarParameterValue(DataStreamConfig.BlendAlphaParam, OverlayBlendAlpha);
					SetImageBrushFromMaterial(CameraImage, OverlayMID, CurrentTexture, OverlayRT);
				}
				else
				{
					SetImageBrushFromTexture(CameraImage, CurrentTexture);
				}
				CameraImage->SetColorAndOpacity(FLinearColor::White);
			}
			break;
	}
}
