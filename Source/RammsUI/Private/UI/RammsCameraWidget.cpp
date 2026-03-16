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
#include "EngineUtils.h"
#include "Styling/CoreStyle.h"
#include "Kismet/KismetRenderingLibrary.h"

void URammsCameraWidget::ResetCachedWidgets()
{
	CameraBorder = nullptr;
	CameraImage = nullptr;
	CameraLabel = nullptr;
	TitleBar = nullptr;
	CollapseButton = nullptr;
	CollapseIcon = nullptr;
	CameraSizeBox = nullptr;
	DepthImage = nullptr;
	ViewModeButton = nullptr;
	ViewModeLabel = nullptr;
}

void URammsCameraWidget::BuildWidgetTree()
{
	if (!WidgetTree || CameraBorder)
		return;

	// Root: Overlay
	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
	WidgetTree->RootWidget = RootOverlay;

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

		UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleRow"));
		TitleBar->AddChild(TitleRow);

		CameraLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CameraLabel"));
		FText LabelText = CustomLabel.IsEmpty() ? FText::FromString(StreamID.IsEmpty() ? TEXT("Camera") : StreamID) : CustomLabel;
		CameraLabel->SetText(LabelText);
		CameraLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		UHorizontalBoxSlot* LabelSlot = TitleRow->AddChildToHorizontalBox(CameraLabel);
		if (LabelSlot)
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		CollapseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CollapseButton"));
		CollapseButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		UHorizontalBoxSlot* CollapseBtnSlot = TitleRow->AddChildToHorizontalBox(CollapseButton);
		if (CollapseBtnSlot)
		{
			CollapseBtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			CollapseBtnSlot->SetVerticalAlignment(VAlign_Center);
			CollapseBtnSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
		}

		CollapseIcon = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CollapseIcon"));
		CollapseIcon->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		CollapseButton->AddChild(CollapseIcon);

		// View mode toggle button (between collapse button and label)
		ViewModeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ViewModeButton"));
		ViewModeButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		UHorizontalBoxSlot* ViewModeBtnSlot = TitleRow->AddChildToHorizontalBox(ViewModeButton);
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
		// Bottom corners only — header has top corners
		CameraBorder->Background = URammsUIStyle::MakeRoundedBoxBrushEx(
			FLinearColor(0.1f, 0.1f, 0.1f, 1.0f), FVector4(0.0f, 0.0f, 4.0f, 4.0f));
		CameraBorder->SetPadding(FMargin(BorderThickness));
		CameraBorder->SetClipping(EWidgetClipping::ClipToBounds);
		CameraSizeBox->AddChild(CameraBorder);

		// HBox to hold RGB image and optional depth image side-by-side
		UHorizontalBox* ImageHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ImageHBox"));
		CameraBorder->AddChild(ImageHBox);

		CameraImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CameraImage"));
		CameraImage->SetColorAndOpacity(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));
		// Render texture with rounded corners — bottom only since header is above
		{
			FSlateBrush ImgBrush = CameraImage->GetBrush();
			ImgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
			float InnerR = FMath::Max(4.0f - BorderThickness, 0.0f);
			ImgBrush.OutlineSettings.CornerRadii = FVector4(0.0f, 0.0f, InnerR, InnerR);
			ImgBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			ImgBrush.OutlineSettings.Width = 0.0f;
			CameraImage->SetBrush(ImgBrush);
		}
		UHorizontalBoxSlot* RGBSlot = ImageHBox->AddChildToHorizontalBox(CameraImage);
		if (RGBSlot)
		{
			RGBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			RGBSlot->SetHorizontalAlignment(HAlign_Fill);
			RGBSlot->SetVerticalAlignment(VAlign_Fill);
		}

		DepthImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DepthImage"));
		DepthImage->SetColorAndOpacity(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));
		{
			FSlateBrush ImgBrush = DepthImage->GetBrush();
			ImgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
			float InnerR = FMath::Max(4.0f - BorderThickness, 0.0f);
			ImgBrush.OutlineSettings.CornerRadii = FVector4(0.0f, 0.0f, InnerR, InnerR);
			ImgBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			ImgBrush.OutlineSettings.Width = 0.0f;
			DepthImage->SetBrush(ImgBrush);
		}
		DepthImage->SetVisibility(ESlateVisibility::Collapsed); // hidden unless SideBySide
		UHorizontalBoxSlot* DepthSlot = ImageHBox->AddChildToHorizontalBox(DepthImage);
		if (DepthSlot)
		{
			DepthSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			DepthSlot->SetHorizontalAlignment(HAlign_Fill);
			DepthSlot->SetVerticalAlignment(VAlign_Fill);
			DepthSlot->SetPadding(FMargin(2.0f, 0.0f, 0.0f, 0.0f));
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
		}
	}
	else
	{
		// Original Overlay layout: CameraBorder fills, TitleBar overlaps at top
		CameraBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CameraBorder"));
		CameraBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(
			FLinearColor(0.1f, 0.1f, 0.1f, 1.0f), 4.0f);
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
		// Render texture with rounded corners — all corners since title overlays on image
		{
			FSlateBrush ImgBrush = CameraImage->GetBrush();
			ImgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
			float InnerR = FMath::Max(4.0f - BorderThickness, 0.0f);
			ImgBrush.OutlineSettings.CornerRadii = FVector4(InnerR, InnerR, InnerR, InnerR);
			ImgBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			ImgBrush.OutlineSettings.Width = 0.0f;
			CameraImage->SetBrush(ImgBrush);
		}

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

		DepthImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DepthImage"));
		DepthImage->SetColorAndOpacity(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));
		{
			FSlateBrush ImgBrush = DepthImage->GetBrush();
			ImgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
			float InnerR = FMath::Max(4.0f - BorderThickness, 0.0f);
			ImgBrush.OutlineSettings.CornerRadii = FVector4(InnerR, InnerR, InnerR, InnerR);
			ImgBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			ImgBrush.OutlineSettings.Width = 0.0f;
			DepthImage->SetBrush(ImgBrush);
		}
		DepthImage->SetVisibility(ESlateVisibility::Collapsed);
		UHorizontalBoxSlot* DepthSlot = ImageHBox->AddChildToHorizontalBox(DepthImage);
		if (DepthSlot)
		{
			DepthSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			DepthSlot->SetHorizontalAlignment(HAlign_Fill);
			DepthSlot->SetVerticalAlignment(VAlign_Fill);
			DepthSlot->SetPadding(FMargin(2.0f, 0.0f, 0.0f, 0.0f));
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

			UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleRow"));
			TitleBar->AddChild(TitleRow);

			CameraLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CameraLabel"));
			FText LabelText = CustomLabel.IsEmpty() ? FText::FromString(StreamID.IsEmpty() ? TEXT("Camera") : StreamID) : CustomLabel;
			CameraLabel->SetText(LabelText);
			CameraLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			UHorizontalBoxSlot* LabelSlot = TitleRow->AddChildToHorizontalBox(CameraLabel);
			if (LabelSlot)
			{
				LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				LabelSlot->SetVerticalAlignment(VAlign_Center);
			}

			// View mode button in non-collapsible title bar
			ViewModeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ViewModeButton"));
			ViewModeButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			UHorizontalBoxSlot* VMSlot = TitleRow->AddChildToHorizontalBox(ViewModeButton);
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

	// Bind collapse button
	if (CollapseButton && bCollapsible)
	{
		CollapseButton->OnClicked.AddDynamic(this, &URammsCameraWidget::OnCollapseClicked);
	}
	UpdateCollapseIcon();

	// Bind view mode button
	if (ViewModeButton)
	{
		ViewModeButton->OnClicked.AddDynamic(this, &URammsCameraWidget::OnViewModeClicked);
	}
	ApplyViewModeLayout();

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
	}

	// Sync view mode label
	ApplyViewModeLayout();

	// Sync collapse icon
	if (CollapseIcon)
	{
		UpdateCollapseIcon();
	}
}

void URammsCameraWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

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

	if (CanvasSlot && CachedExpandedSlotSize.Y > 0.0f)
	{
		float SlotContentH = CachedExpandedSlotSize.Y - TitleH;
		CanvasSlot->SetSize(FVector2D(CachedExpandedSlotSize.X, TitleH + SlotContentH * Alpha));
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
		}
		else
		{
			// Fully collapsed: Auto slot + HeightOverride(0) — stays in layout for width
			SetCameraSizeBoxSlotFill(false);
			CameraSizeBox->SetHeightOverride(0.0f);
			CameraSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
			CameraSizeBox->SetRenderOpacity(0.0f);
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

	DisplayMode = NewMode;
	UpdateLayout(bAnimateTransition);
}

void URammsCameraWidget::SetTexture(UTexture* Texture)
{
	CurrentTexture = Texture;
	UpdateDisplayedImages();
}

void URammsCameraWidget::SetDepthTexture(UTexture* Texture)
{
	CurrentDepthTexture = Texture;
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

	// Detect if we're inside a Canvas Panel or added directly to viewport
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);

	switch (DisplayMode)
	{
		case ERammsCameraDisplayMode::Fullscreen:
		{
			if (CanvasSlot)
			{
				// Stretch anchors fill entire Canvas Panel
				CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				CanvasSlot->SetOffsets(FMargin(0, 0, 0, 0));
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
			if (CanvasSlot)
			{
				// Don't override Canvas Panel layout in Windowed mode —
				// let the user's designer/anchor settings control position and size
				CachedExpandedSlotSize = CanvasSlot->GetSize();
			}
			else
			{
				// Viewport-direct: center on screen
				FVector2D SizeInUnits = CanvasSize * WindowedSize;
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
			FVector2D WidgetSize = CanvasSize * CornerSize;

			// Compute anchor and alignment from corner alignment enums
			float	  AnchorX = (CornerHAlign == HAlign_Right) ? 1.0f : (CornerHAlign == HAlign_Center ? 0.5f : 0.0f);
			float	  AnchorY = (CornerVAlign == VAlign_Bottom) ? 1.0f : (CornerVAlign == VAlign_Center ? 0.5f : 0.0f);
			FVector2D Alignment(AnchorX, AnchorY);

			// Padding offset: positive = inward from edge
			FVector2D PadOffset(
				(AnchorX > 0.5f) ? -CornerPadding.X : (AnchorX < 0.5f ? CornerPadding.X : 0.0f),
				(AnchorY > 0.5f) ? -CornerPadding.Y : (AnchorY < 0.5f ? CornerPadding.Y : 0.0f));

			if (CanvasSlot)
			{
				CanvasSlot->SetAnchors(FAnchors(AnchorX, AnchorY, AnchorX, AnchorY));
				CanvasSlot->SetAlignment(Alignment);
				CanvasSlot->SetPosition(PadOffset);
				CanvasSlot->SetSize(WidgetSize);
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
	}
}

void URammsCameraWidget::OnCameraFrameReady(const FString& InStreamID, UTexture* Texture, int64 Timestamp)
{
	if (InStreamID == StreamID)
	{
		CurrentTexture = Texture;
		UpdateDisplayedImages();
	}
	else if (InStreamID == DepthStreamID)
	{
		CurrentDepthTexture = Texture;
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

	// Start depth stream if configured
	if (!DepthStreamID.IsEmpty())
	{
		for (auto& Sub : ProviderSubscriptions)
		{
			if (Sub.Object.IsValid() && Sub.Interface)
			{
				Sub.Interface->StartStream(DepthStreamID);
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
			if (!DepthStreamID.IsEmpty())
			{
				Sub.Interface->StopStream(DepthStreamID);
			}
		}
	}
}

FReply URammsCameraWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bEnableDrag && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
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

void URammsCameraWidget::SetDepthColormap(ERammsDepthColormap NewColormap)
{
	DepthColormap = NewColormap;
	UpdateDepthMaterialParams();
	UpdateDisplayedImages();
}

void URammsCameraWidget::SetDepthRange(float MinDepth, float MaxDepth)
{
	DepthRange = FVector2D(MinDepth, MaxDepth);
	UpdateDepthMaterialParams();
	UpdateDisplayedImages();
}

void URammsCameraWidget::SetDepthStreamID(const FString& NewDepthStreamID)
{
	if (DepthStreamID == NewDepthStreamID)
		return;

	// Stop existing depth stream
	IRammsCameraProvider* Provider = CameraProvider.GetInterface();
	if (Provider && !DepthStreamID.IsEmpty())
	{
		Provider->StopStream(DepthStreamID);
	}

	DepthStreamID = NewDepthStreamID;
	CurrentDepthTexture = nullptr;

	// Start new depth stream
	if (Provider && !DepthStreamID.IsEmpty())
	{
		Provider->StartStream(DepthStreamID);
	}
}

void URammsCameraWidget::OnViewModeClicked()
{
	// Cycle: RGB -> Depth -> SideBySide -> Overlay -> RGB
	switch (ViewMode)
	{
		case ERammsCameraViewMode::RGB:
			SetViewMode(ERammsCameraViewMode::Depth);
			break;
		case ERammsCameraViewMode::Depth:
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
			case ERammsCameraViewMode::Depth:
				ViewModeLabel->SetText(FText::FromString(TEXT("Depth")));
				break;
			case ERammsCameraViewMode::SideBySide:
				ViewModeLabel->SetText(FText::FromString(TEXT("SbS")));
				break;
			case ERammsCameraViewMode::Overlay:
				ViewModeLabel->SetText(FText::FromString(TEXT("Ovly")));
				break;
		}
	}

	// Show/hide depth image for side-by-side mode
	if (DepthImage)
	{
		DepthImage->SetVisibility(ViewMode == ERammsCameraViewMode::SideBySide
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
	}

	// Adjust corner radii based on whether side-by-side or single image
	UpdateImageCornerRadii();
}

void URammsCameraWidget::UpdateImageCornerRadii()
{
	// Compute inner radius from style or default
	float OuterRadius = Style ? Style->Border.CornerRadiusMedium : 4.0f;
	float R = FMath::Max(OuterRadius - BorderThickness, 0.0f);
	bool  bSideBySide = (ViewMode == ERammsCameraViewMode::SideBySide);

	// FVector4 CornerRadii: X=TopLeft, Y=TopRight, Z=BottomRight, W=BottomLeft
	FVector4 RGBRadii, DepthRadii;

	if (bCollapsible)
	{
		// Header is above image — only bottom corners need rounding
		if (bSideBySide)
		{
			RGBRadii = FVector4(0.0f, 0.0f, 0.0f, R);	// bottom-left only
			DepthRadii = FVector4(0.0f, 0.0f, R, 0.0f); // bottom-right only
		}
		else
		{
			RGBRadii = FVector4(0.0f, 0.0f, R, R); // bottom corners
			DepthRadii = FVector4(0.0f, 0.0f, R, R);
		}
	}
	else
	{
		// Header overlays on image — all outer corners need rounding
		if (bSideBySide)
		{
			RGBRadii = FVector4(R, 0.0f, 0.0f, R);	 // left corners
			DepthRadii = FVector4(0.0f, R, R, 0.0f); // right corners
		}
		else
		{
			RGBRadii = FVector4(R, R, R, R); // all corners
			DepthRadii = FVector4(R, R, R, R);
		}
	}

	auto ApplyRadii = [](UImage* Img, const FVector4& Radii) {
		if (!Img)
			return;
		FSlateBrush B = Img->GetBrush();
		B.OutlineSettings.CornerRadii = Radii;
		Img->SetBrush(B);
	};

	ApplyRadii(CameraImage, RGBRadii);
	ApplyRadii(DepthImage, DepthRadii);
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
}

void URammsCameraWidget::EnsureDepthMaterials()
{
	// Create depth colormap MID if material is assigned and MID doesn't exist
	if (DepthColormapMaterial && !DepthMID)
	{
		DepthMID = UMaterialInstanceDynamic::Create(DepthColormapMaterial, this);
		UpdateDepthMaterialParams();
	}

	// Create overlay blend MID if material is assigned and MID doesn't exist
	if (OverlayBlendMaterial && !OverlayMID)
	{
		OverlayMID = UMaterialInstanceDynamic::Create(OverlayBlendMaterial, this);
		UpdateDepthMaterialParams();
	}
}

void URammsCameraWidget::UpdateDepthMaterialParams()
{
	float ColormapIdx = static_cast<float>(static_cast<uint8>(DepthColormap));

	if (DepthMID)
	{
		DepthMID->SetScalarParameterValue(TEXT("DepthMin"), DepthRange.X);
		DepthMID->SetScalarParameterValue(TEXT("DepthMax"), DepthRange.Y);
		DepthMID->SetScalarParameterValue(TEXT("ColormapIndex"), ColormapIdx);
		if (CurrentDepthTexture)
		{
			DepthMID->SetTextureParameterValue(TEXT("DepthTexture"), CurrentDepthTexture);
		}
	}

	if (OverlayMID)
	{
		OverlayMID->SetScalarParameterValue(TEXT("DepthMin"), DepthRange.X);
		OverlayMID->SetScalarParameterValue(TEXT("DepthMax"), DepthRange.Y);
		OverlayMID->SetScalarParameterValue(TEXT("ColormapIndex"), ColormapIdx);
		OverlayMID->SetScalarParameterValue(TEXT("BlendAlpha"), DepthOverlayAlpha);
		if (CurrentTexture)
		{
			OverlayMID->SetTextureParameterValue(TEXT("RGBTexture"), CurrentTexture);
		}
		if (CurrentDepthTexture)
		{
			OverlayMID->SetTextureParameterValue(TEXT("DepthTexture"), CurrentDepthTexture);
		}
	}
}

void URammsCameraWidget::SetImageBrushFromTexture(UImage* Image, UTexture* Texture)
{
	if (!Image || !Texture)
		return;

	// Preserve OutlineSettings (corner radii) and restore RoundedBox draw type for textures
	FSlateBrush Brush = Image->GetBrush();
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.SetResourceObject(Texture);

	float TexW = static_cast<float>(Texture->GetSurfaceWidth());
	float TexH = static_cast<float>(Texture->GetSurfaceHeight());
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

	// Use RoundedBox draw type with the MID directly as the brush resource.
	// Slate's RoundedBox natively supports materials via TSlateMaterialShaderPS<RoundedBox>,
	// so we get both material rendering AND rounded corner clipping without an intermediate RT.
	FSlateBrush Brush = Image->GetBrush();
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.SetResourceObject(MID);

	if (SizeSource)
	{
		float TexW = static_cast<float>(SizeSource->GetSurfaceWidth());
		float TexH = static_cast<float>(SizeSource->GetSurfaceHeight());
		if (TexW > 0.0f && TexH > 0.0f)
		{
			Brush.ImageSize = FVector2D(TexW, TexH);
		}
	}

	Image->SetBrush(Brush);
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
	EnsureDepthMaterials();

	switch (ViewMode)
	{
		case ERammsCameraViewMode::RGB:
			if (CameraImage && CurrentTexture)
			{
				SetImageBrushFromTexture(CameraImage, CurrentTexture);
				CameraImage->SetColorAndOpacity(FLinearColor::White);
			}
			break;

		case ERammsCameraViewMode::Depth:
			if (CameraImage && CurrentDepthTexture)
			{
				if (DepthMID)
				{
					DepthMID->SetTextureParameterValue(TEXT("DepthTexture"), CurrentDepthTexture);
					SetImageBrushFromMaterial(CameraImage, DepthMID, CurrentDepthTexture, DepthRT);
				}
				else
				{
					SetImageBrushFromTexture(CameraImage, CurrentDepthTexture);
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
			if (DepthImage && CurrentDepthTexture)
			{
				if (DepthMID)
				{
					DepthMID->SetTextureParameterValue(TEXT("DepthTexture"), CurrentDepthTexture);
					SetImageBrushFromMaterial(DepthImage, DepthMID, CurrentDepthTexture, DepthRT);
				}
				else
				{
					SetImageBrushFromTexture(DepthImage, CurrentDepthTexture);
				}
				DepthImage->SetColorAndOpacity(FLinearColor::White);
			}
			break;

		case ERammsCameraViewMode::Overlay:
			if (CameraImage && CurrentTexture)
			{
				if (OverlayMID && CurrentDepthTexture)
				{
					OverlayMID->SetTextureParameterValue(TEXT("RGBTexture"), CurrentTexture);
					OverlayMID->SetTextureParameterValue(TEXT("DepthTexture"), CurrentDepthTexture);
					OverlayMID->SetScalarParameterValue(TEXT("BlendAlpha"), DepthOverlayAlpha);
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
