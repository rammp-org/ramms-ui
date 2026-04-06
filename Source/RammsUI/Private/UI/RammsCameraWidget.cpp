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

int32																	 URammsCameraWidget::FocusZOrderCounter = 0;
TMap<TPair<UWidget*, uint8>, TArray<TWeakObjectPtr<URammsCameraWidget>>> URammsCameraWidget::CornerRegistry;

void URammsCameraWidget::ResetCachedWidgets()
{
	CameraBorder = nullptr;
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
}

void URammsCameraWidget::BuildWidgetTree()
{
	if (!WidgetTree || CameraBorder)
		return;

	// Root: InternalSizeBox wraps everything for non-Canvas layout sizing
	InternalSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("InternalSizeBox"));
	WidgetTree->RootWidget = InternalSizeBox;

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));

	if (bMaintainAspectRatio && !bCollapsible)
	{
		// Non-collapsible: AR box wraps entire widget (title overlays on image)
		ImageAspectRatioBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ImageAspectRatioBox"));
		ImageAspectRatioBox->SetMinAspectRatio(AspectRatio);
		ImageAspectRatioBox->SetMaxAspectRatio(AspectRatio);
		InternalSizeBox->AddChild(ImageAspectRatioBox);
		ImageAspectRatioBox->AddChild(RootOverlay);
	}
	else
	{
		InternalSizeBox->AddChild(RootOverlay);
	}

	if (bCollapsible)
	{
		// VBox layout: TitleBar (header for collapse) + CameraSizeBox
		UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainVBox"));
		UOverlaySlot* VBoxSlot = RootOverlay->AddChildToOverlay(MainVBox);
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

		// Collapse button (leftmost, before label)
		CollapseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CollapseButton"));
		CollapseButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		UHorizontalBoxSlot* CollapseBtnSlot = ButtonRow->AddChildToHorizontalBox(CollapseButton);
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

		// View mode toggle button
		ViewModeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ViewModeButton"));
		ViewModeButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		UHorizontalBoxSlot* ViewModeBtnSlot = ButtonRow->AddChildToHorizontalBox(ViewModeButton);
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

		// Data stream option cycling button (hidden until config has options)
		OptionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("OptionButton"));
		OptionButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		UHorizontalBoxSlot* OptBtnSlot = ButtonRow->AddChildToHorizontalBox(OptionButton);
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
		OptionButton->SetVisibility(ESlateVisibility::Collapsed);

		// Display mode cycle button (rightmost)
		DisplayModeCycleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DisplayModeCycleButton"));
		DisplayModeCycleButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		UHorizontalBoxSlot* CycleBtnSlot = ButtonRow->AddChildToHorizontalBox(DisplayModeCycleButton);
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

		// HBox to hold RGB image and optional data image side-by-side
		UHorizontalBox* ImageHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ImageHBox"));
		CameraBorder->AddChild(ImageHBox);

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
		UOverlaySlot* BorderSlot = RootOverlay->AddChildToOverlay(CameraBorder);
		if (BorderSlot)
		{
			BorderSlot->SetHorizontalAlignment(HAlign_Fill);
			BorderSlot->SetVerticalAlignment(VAlign_Fill);
		}

		CameraImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CameraImage"));
		CameraImage->SetColorAndOpacity(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));

		// HBox for side-by-side support
		UHorizontalBox* ImageHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ImageHBox"));
		CameraBorder->AddChild(ImageHBox);

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
			UOverlaySlot* TitleSlot = RootOverlay->AddChildToOverlay(TitleBar);
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

			// View mode button in non-collapsible title bar
			ViewModeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ViewModeButton"));
			ViewModeButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			UHorizontalBoxSlot* VMSlot = ButtonRow->AddChildToHorizontalBox(ViewModeButton);
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

			// Data stream option cycling button (hidden until config has options)
			OptionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("OptionButton"));
			OptionButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			UHorizontalBoxSlot* OptSlot = ButtonRow->AddChildToHorizontalBox(OptionButton);
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
			OptionButton->SetVisibility(ESlateVisibility::Collapsed);

			// Display mode cycle button
			DisplayModeCycleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DisplayModeCycleButton"));
			DisplayModeCycleButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			UHorizontalBoxSlot* CycleBtnSlot = ButtonRow->AddChildToHorizontalBox(DisplayModeCycleButton);
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

			UE_LOG(LogTemp, Log, TEXT("RammsCameraWidget: Found %d providers, subscribed to all. StreamID='%s'"),
				ProviderIfaces.Num(), *StreamID);
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

	// Update initial layout
	UpdateLayout(false);
}

void URammsCameraWidget::NativeDestruct()
{
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
				ImageAspectRatioBox->SetMinAspectRatio(AspectRatio);
				ImageAspectRatioBox->SetMaxAspectRatio(AspectRatio);
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
		CollapseIcon->SetFont(Style->Typography.Caption);
		CollapseIcon->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
	}

	// Style header action buttons (view mode / option)
	if (ViewModeLabel)
	{
		ViewModeLabel->SetFont(Style->Typography.Caption);
		ViewModeLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}

	if (OptionLabel)
	{
		OptionLabel->SetFont(Style->Typography.Caption);
		OptionLabel->SetColorAndOpacity(FSlateColor(Style->Colors.Info));
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
	UpdateLayout(false); // Apply layout immediately (sets anchors, alignment, size)

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

	// Title bar height to reserve when collapsible (header sits above image)
	float TitleH = 0.0f;
	if (bCollapsible && TitleBar)
	{
		FVector2D TitleSize = TitleBar->GetDesiredSize();
		TitleH = (TitleSize.Y > 0.0f) ? TitleSize.Y : 24.0f;
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
			if (CanvasSlot)
			{
				// Stretch anchors fill entire Canvas Panel
				CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				CanvasSlot->SetOffsets(FMargin(0, 0, 0, 0));
			}
			else if (bInLayoutContainer)
			{
				// In layout container: fill available space (clear size overrides)
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
				SizeInUnits.X = FitW;
				SizeInUnits.Y = FitH + TitleH;
			}
			else
			{
				SizeInUnits = MaxBounds;
			}

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
				WidgetSize.X = FitW;
				WidgetSize.Y = FitH + TitleH;
			}
			else
			{
				WidgetSize = MaxBounds;
			}

			// Compute anchor and alignment from corner alignment enums
			float	  AnchorX = (CornerHAlign == HAlign_Right) ? 1.0f : (CornerHAlign == HAlign_Center ? 0.5f : 0.0f);
			float	  AnchorY = (CornerVAlign == VAlign_Bottom) ? 1.0f : (CornerVAlign == VAlign_Center ? 0.5f : 0.0f);
			FVector2D Alignment(AnchorX, AnchorY);

			// Padding offset: positive = inward from edge
			FVector2D PadOffset(
				(AnchorX > 0.5f) ? -CornerPadding.X : (AnchorX < 0.5f ? CornerPadding.X : 0.0f),
				(AnchorY > 0.5f) ? -CornerPadding.Y : (AnchorY < 0.5f ? CornerPadding.Y : 0.0f));

			// Corner stacking: offset widgets that share the same corner
			int32 StackIndex = GetCornerStackIndex();
			if (StackIndex > 0)
			{
				// Landscape widgets (wider than tall) stack vertically; portrait stack horizontally
				bool bStackVertically = (AspectRatio >= 1.0f || !bMaintainAspectRatio);
				if (bStackVertically)
				{
					float StackDir = (AnchorY > 0.5f) ? -1.0f : 1.0f;
					PadOffset.Y += StackDir * StackIndex * (WidgetSize.Y + CornerStackGap);
				}
				else
				{
					float StackDir = (AnchorX > 0.5f) ? -1.0f : 1.0f;
					PadOffset.X += StackDir * StackIndex * (WidgetSize.X + CornerStackGap);
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
				ImageAspectRatioBox->SetMinAspectRatio(AspectRatio);
				ImageAspectRatioBox->SetMaxAspectRatio(AspectRatio);
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
					if (ImageAspectRatioBox)
					{
						ImageAspectRatioBox->SetMinAspectRatio(AspectRatio);
						ImageAspectRatioBox->SetMaxAspectRatio(AspectRatio);
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

		// Auto-detect depth format from provider on the first data frame.
		// Once set, the format is cached for the lifetime of the stream
		// subscription; it resets when the stream is re-subscribed.
		if (CachedDataDepthFormat == ERammsDepthFormat::Unknown && !DataStreamID.IsEmpty())
		{
			for (const auto& Sub : ProviderSubscriptions)
			{
				if (Sub.Object.IsValid() && Sub.Interface)
				{
					FRammsCameraStreamInfo Info;
					if (Sub.Interface->GetStreamInfo(DataStreamID, Info) && Info.DepthFormat != ERammsDepthFormat::Unknown)
					{
						CachedDataDepthFormat = Info.DepthFormat;
						UpdateDataMaterialParams();
						break;
					}
				}
			}
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
		UE_LOG(LogTemp, Log, TEXT("URammsCameraWidget: StartStream('%s') %s"), *StreamID,
			bStarted ? TEXT("succeeded") : TEXT("not yet registered (will receive when available)"));
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

	if (DataStreamConfig.Options.Num() > 0 && DataStreamConfig.OptionParamName.IsValid())
	{
		OptionButton->SetVisibility(ESlateVisibility::Visible);
		if (OptionLabel)
		{
			int32 SafeIdx = FMath::Clamp(CurrentOptionIndex, 0, DataStreamConfig.Options.Num() - 1);
			OptionLabel->SetText(DataStreamConfig.Options[SafeIdx].DisplayName);
		}
	}
	else
	{
		OptionButton->SetVisibility(ESlateVisibility::Collapsed);
	}
}

FString URammsCameraWidget::GetDisplayModeShortLabel(ERammsCameraDisplayMode Mode)
{
	switch (Mode)
	{
		case ERammsCameraDisplayMode::Fullscreen:
			return TEXT("\u2922"); // ⤢ (expand arrows)
		case ERammsCameraDisplayMode::Windowed:
			return TEXT("\u25A1"); // □ (window)
		case ERammsCameraDisplayMode::Corner:
			return TEXT("\u25F0"); // ◰ (corner)
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

	bool bVisible = bShowDisplayModeCycleButton && AllowedDisplayModes.Num() > 1;
	DisplayModeCycleButton->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (DisplayModeCycleLabel)
	{
		DisplayModeCycleLabel->SetText(FText::FromString(GetDisplayModeShortLabel(DisplayMode)));
	}
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
			ImageAspectRatioBox->SetMinAspectRatio(AspectRatio);
			ImageAspectRatioBox->SetMaxAspectRatio(AspectRatio);
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

	if (ImageAspectRatioBox && bMaintainAspectRatio)
	{
		ImageAspectRatioBox->SetMinAspectRatio(AspectRatio);
		ImageAspectRatioBox->SetMaxAspectRatio(AspectRatio);
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

	// Show/hide data image for side-by-side mode
	if (DataImage)
	{
		DataImage->SetVisibility(ViewMode == ERammsCameraViewMode::SideBySide
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
	}

	// Adjust corner radii based on whether side-by-side or single image
	UpdateImageCornerRadii();
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
		if (bSideBySide)
		{
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
		if (bSideBySide)
		{
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
		TitleBar->SetPadding(FMargin(Style->Spacing.Medium, Style->Spacing.Small));
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
		UpdateDataMaterialParams();
	}

	// Create overlay blend MID if material is assigned and MID doesn't exist
	if (DataStreamConfig.OverlayMaterial && !OverlayMID)
	{
		OverlayMID = UMaterialInstanceDynamic::Create(DataStreamConfig.OverlayMaterial, this);
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

	// Apply all scalar params from config, then depth format params, to both materials
	auto ApplyParams = [&](UMaterialInstanceDynamic* MID) {
		if (!MID)
			return;
		for (const auto& Pair : DataStreamConfig.ScalarParams)
		{
			MID->SetScalarParameterValue(Pair.Key, Pair.Value);
		}
		// Auto-injected depth format params (materials can use these to normalize depth)
		MID->SetScalarParameterValue(FName("DepthUnnormalize"), DepthUnnormalize);
		MID->SetScalarParameterValue(FName("DepthScaleToCM"), DepthScaleToCM);
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
		return;

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
	FVector2D& LastSize = (Image == DataImage) ? LastMaterialImageSize_Data : LastMaterialImageSize_RGB;
	// Radii are already cached and only change in ApplyStyle — no per-frame check needed.
	// Size changes when widget resizes, which is infrequent.
	if (Size.X > 0.0f && Size.Y > 0.0f && FMath::IsNearlyEqual(Size.X, LastSize.X, 0.5f) && FMath::IsNearlyEqual(Size.Y, LastSize.Y, 0.5f))
	{
		return; // Neither radii nor size changed
	}
	LastSize = Size;

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
