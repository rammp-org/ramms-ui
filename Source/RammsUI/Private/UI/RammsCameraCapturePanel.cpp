// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsCameraCapturePanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#include "UI/RammsButton.h"
#include "UI/RammsCameraWidget.h"
#include "UI/RammsUIStyle.h"

#include "CameraCaptureSubsystem.h"
#include "IntrinsicSceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

void URammsCameraCapturePanel::BuildWidgetTree()
{
	if (!WidgetTree || PanelBorder)
	{
		return;
	}

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(
		FLinearColor(0.06f, 0.06f, 0.08f, 0.9f), 4.0f, FLinearColor(0.3f, 0.3f, 0.3f, 1.0f), 1.0f);
	PanelBorder->SetPadding(FMargin(8.0f));
	WidgetTree->RootWidget = PanelBorder;

	MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainVBox"));
	PanelBorder->AddChild(MainVBox);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("Camera Capture")));
	if (UVerticalBoxSlot* Slot = MainVBox->AddChildToVerticalBox(TitleText))
	{
		Slot->SetPadding(FMargin(4.0f, 2.0f, 4.0f, 6.0f));
	}

	if (bShowControls)
	{
		ControlsHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ControlsHBox"));
		if (UVerticalBoxSlot* Slot = MainVBox->AddChildToVerticalBox(ControlsHBox))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}

		auto AddButton = [this](const TCHAR* Name, const TCHAR* Label) -> URammsButton* {
			URammsButton* Button = WidgetTree->ConstructWidget<URammsButton>(URammsButton::StaticClass(), Name);
			Button->SetText(FText::FromString(Label));
			if (UHorizontalBoxSlot* Slot = ControlsHBox->AddChildToHorizontalBox(Button))
			{
				Slot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
			}
			return Button;
		};

		CaptureButton = AddButton(TEXT("CaptureButton"), TEXT("Start Capture"));
		CaptureButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleCaptureClicked);

		SerializationButton = AddButton(TEXT("SerializationButton"), TEXT("Saving: off"));
		SerializationButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleSerializationClicked);

		if (bShowCaptureRate)
		{
			RateDownButton = AddButton(TEXT("RateDownButton"), TEXT("-"));
			RateDownButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleRateDownClicked);

			RateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RateText"));
			RateText->SetText(FText::FromString(TEXT("every frame")));
			if (UHorizontalBoxSlot* Slot = ControlsHBox->AddChildToHorizontalBox(RateText))
			{
				Slot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 0.0f));
				Slot->SetVerticalAlignment(VAlign_Center);
			}

			RateUpButton = AddButton(TEXT("RateUpButton"), TEXT("+"));
			RateUpButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleRateUpClicked);
		}
	}

	if (bShowStatistics)
	{
		StatsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatsText"));
		StatsText->SetText(FText::FromString(TEXT("")));
		if (UVerticalBoxSlot* Slot = MainVBox->AddChildToVerticalBox(StatsText))
		{
			Slot->SetPadding(FMargin(4.0f, 0.0f, 4.0f, 6.0f));
		}
	}

	FeedsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("FeedsScroll"));
	if (UVerticalBoxSlot* Slot = MainVBox->AddChildToVerticalBox(FeedsScroll))
	{
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	FeedsVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FeedsVBox"));
	FeedsScroll->AddChild(FeedsVBox);
}

void URammsCameraCapturePanel::ResetCachedWidgets()
{
	PanelBorder = nullptr;
	MainVBox = nullptr;
	TitleText = nullptr;
	ControlsHBox = nullptr;
	CaptureButton = nullptr;
	SerializationButton = nullptr;
	RateDownButton = nullptr;
	RateUpButton = nullptr;
	RateText = nullptr;
	StatsText = nullptr;
	FeedsScroll = nullptr;
	FeedsVBox = nullptr;
	Feeds.Reset();
	FeedCameras.Reset();
}

UWidget* URammsCameraCapturePanel::GetRootWidgetForValidation()
{
	return PanelBorder;
}

void URammsCameraCapturePanel::NativeConstruct()
{
	BuildWidgetTree();
	Super::NativeConstruct();
	RebuildFeeds();
	RefreshLabels();
}

void URammsCameraCapturePanel::ApplyStyle_Implementation()
{
	Super::ApplyStyle_Implementation();
	if (!Style)
	{
		return;
	}
	if (PanelBorder)
	{
		PanelBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(
			Style->Colors.Background, Style->Border.CornerRadiusMedium, Style->Colors.Border, Style->Border.BorderWidth);
	}
	if (TitleText)
	{
		TitleText->SetFont(Style->Typography.HeadingMedium);
		TitleText->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}
	for (UTextBlock* Text : { StatsText.Get(), RateText.Get() })
	{
		if (Text)
		{
			Text->SetFont(Style->Typography.Caption);
			Text->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
		}
	}
	// Buttons and feeds are created at runtime, outside this widget's tree, so
	// they are handed the style directly -- the same thing the control surface
	// panel has to do for its rows.
	for (URammsButton* Button : { CaptureButton.Get(), SerializationButton.Get(), RateDownButton.Get(), RateUpButton.Get() })
	{
		if (Button)
		{
			Button->SetStyle(Style);
		}
	}
	for (URammsCameraWidget* Feed : Feeds)
	{
		if (Feed)
		{
			Feed->SetStyle(Style);
		}
	}
}

// ---------------------------------------------------------------------------
// Subsystem
// ---------------------------------------------------------------------------

UCameraCaptureSubsystem* URammsCameraCapturePanel::GetSubsystem() const
{
	// A world subsystem, so it exists only while a world does -- null in the
	// widget designer, and null before BeginPlay.
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UCameraCaptureSubsystem>() : nullptr;
}

// ---------------------------------------------------------------------------
// Feeds
// ---------------------------------------------------------------------------

void URammsCameraCapturePanel::RebuildFeeds()
{
	if (!FeedsVBox)
	{
		return;
	}

	FeedsVBox->ClearChildren();
	Feeds.Reset();
	FeedCameras.Reset();

	UCameraCaptureSubsystem* Sub = GetSubsystem();
	if (!Sub)
	{
		return;
	}

	const TArray<UIntrinsicSceneCaptureComponent2D*> Cameras = Sub->GetRegisteredCameras();
	for (UIntrinsicSceneCaptureComponent2D* Camera : Cameras)
	{
		if (!Camera)
		{
			continue;
		}

		URammsCameraWidget* Feed = CreateWidget<URammsCameraWidget>(this);
		if (!Feed)
		{
			continue;
		}
		Feed->SetStyle(Style);
		// The label is ours rather than the camera widget's: its CustomLabel is
		// protected, and the public way in is SetStreamID, which also means "go
		// find this stream" -- a side effect this panel has no use for, since the
		// texture is being handed over directly.
		const FString CameraLabel = Camera->GetOwner()
			? FString::Printf(TEXT("%s :: %s"), *Camera->GetOwner()->GetName(), *Camera->GetName())
			: Camera->GetName();

		UVerticalBox* FeedBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetText(FText::FromString(CameraLabel));
		if (Style)
		{
			Label->SetFont(Style->Typography.Caption);
			Label->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
		}
		FeedBox->AddChildToVerticalBox(Label);

		USizeBox* HeightBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		HeightBox->SetHeightOverride(FeedHeight);
		HeightBox->AddChild(Feed);
		FeedBox->AddChildToVerticalBox(HeightBox);

		if (UVerticalBoxSlot* Slot = FeedsVBox->AddChildToVerticalBox(FeedBox))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}

		// Textures go on AFTER the widget is in the tree. CreateWidget does not
		// run NativeConstruct -- that happens when the widget gets a parent -- so
		// setting them earlier reached a widget whose internal UImage did not
		// exist yet, and the feed drew its frame and label around nothing.
		//
		// The camera's own render target, not FCaptureData: it is already on the
		// GPU, so showing it is a brush assignment rather than a readback, and it
		// keeps updating whether or not anything is being written to disk.
		Feed->SetTexture(Camera->TextureTarget);

		// Depth into the data slot when there is a separate target for it. In
		// single-capture mode there is none, because depth is in the colour
		// target's alpha.
		if (UTextureRenderTarget2D* DepthRT = Sub->GetDepthRenderTarget(Camera))
		{
			Feed->SetDataTexture(DepthRT);
		}

		Feeds.Add(Feed);
		FeedCameras.Add(Camera);
	}
}

// ---------------------------------------------------------------------------
// Tick / labels
// ---------------------------------------------------------------------------

void URammsCameraCapturePanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TimeSinceRefresh += InDeltaTime;
	if (TimeSinceRefresh < RefreshInterval)
	{
		return;
	}
	TimeSinceRefresh = 0.0f;

	// Rebuild only when the registered set actually changed. Cameras register on
	// BeginPlay and can unregister when their actor goes away, so the list is not
	// fixed -- but rebuilding every refresh would discard and recreate every feed
	// widget several times a second.
	if (UCameraCaptureSubsystem* Sub = GetSubsystem())
	{
		const TArray<UIntrinsicSceneCaptureComponent2D*> Cameras = Sub->GetRegisteredCameras();
		bool											 bChanged = Cameras.Num() != FeedCameras.Num();
		if (!bChanged)
		{
			for (int32 i = 0; i < Cameras.Num(); ++i)
			{
				if (FeedCameras[i].Get() != Cameras[i])
				{
					bChanged = true;
					break;
				}
			}
		}
		if (bChanged)
		{
			RebuildFeeds();
		}
	}

	// Re-assert the feed textures. The camera widget drives its own image from a
	// provider/stream when it has one and refreshes on tick, which overwrites a
	// texture handed to it directly -- so setting it once at build time does not
	// stick. Re-assigning costs a brush update per feed at RefreshInterval.
	if (UCameraCaptureSubsystem* Sub = GetSubsystem())
	{
		for (int32 i = 0; i < Feeds.Num() && i < FeedCameras.Num(); ++i)
		{
			UIntrinsicSceneCaptureComponent2D* Camera = FeedCameras[i].Get();
			if (Feeds[i] && Camera)
			{
				Feeds[i]->SetTexture(Camera->TextureTarget);
				if (UTextureRenderTarget2D* DepthRT = Sub->GetDepthRenderTarget(Camera))
				{
					Feeds[i]->SetDataTexture(DepthRT);
				}
			}
		}
	}

	RefreshLabels();
}

void URammsCameraCapturePanel::RefreshLabels()
{
	UCameraCaptureSubsystem* Sub = GetSubsystem();

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(
			Sub ? FString::Printf(TEXT("Camera Capture  (%d cameras)"), Sub->GetRegisteredCameraCount())
				: FString(TEXT("Camera Capture  (no subsystem)"))));
	}

	// Without a subsystem there is nothing to drive, so the controls say so
	// rather than pretending to work.
	const bool bEnabled = Sub != nullptr;
	for (URammsButton* Button : { CaptureButton.Get(), SerializationButton.Get(), RateDownButton.Get(), RateUpButton.Get() })
	{
		if (Button)
		{
			Button->SetEnabled(bEnabled);
		}
	}
	if (!Sub)
	{
		return;
	}

	if (CaptureButton)
	{
		CaptureButton->SetText(FText::FromString(Sub->IsCapturing() ? TEXT("Stop Capture") : TEXT("Start Capture")));
	}
	if (SerializationButton)
	{
		SerializationButton->SetText(FText::FromString(
			Sub->IsSerializationEnabled() ? TEXT("Saving: on") : TEXT("Saving: off")));
	}
	if (RateText)
	{
		const int32 N = Sub->GetCaptureRate();
		RateText->SetText(FText::FromString(
			N <= 1 ? FString(TEXT("every frame")) : FString::Printf(TEXT("every %d frames"), N)));
	}
	if (StatsText)
	{
		const FCaptureStatistics Stats = Sub->GetStatistics();

		// The output directory is stored relative to the project, which prints as
		// a stack of "../.." that overflows the panel and says nothing. Show the
		// last couple of components, which is the part that identifies the run.
		FString Dir = Sub->GetOutputDirectory();
		FPaths::NormalizeDirectoryName(Dir);
		TArray<FString> Parts;
		Dir.ParseIntoArray(Parts, TEXT("/"), true);
		Parts.RemoveAll([](const FString& P) { return P == TEXT("..") || P == TEXT("."); });
		if (Parts.Num() > 2)
		{
			Parts.RemoveAt(0, Parts.Num() - 2);
		}
		const FString ShortDir = Parts.Num() > 0 ? FString::Join(Parts, TEXT("/")) : Dir;

		StatsText->SetText(FText::FromString(FString::Printf(
			TEXT("%lld frames  |  kick %.2f ms  |  %s"),
			Stats.TotalFramesCaptured,
			Stats.AverageCaptureTimeMs,
			*ShortDir)));
	}
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void URammsCameraCapturePanel::ToggleCapture()
{
	if (UCameraCaptureSubsystem* Sub = GetSubsystem())
	{
		if (Sub->IsCapturing())
		{
			Sub->StopCapture();
		}
		else
		{
			Sub->StartCapture();
		}
		RefreshLabels();
	}
}

void URammsCameraCapturePanel::ToggleSerialization()
{
	if (UCameraCaptureSubsystem* Sub = GetSubsystem())
	{
		Sub->SetSerializationEnabled(!Sub->IsSerializationEnabled());
		RefreshLabels();
	}
}

void URammsCameraCapturePanel::StepCaptureRate(int32 Delta)
{
	UCameraCaptureSubsystem* Sub = GetSubsystem();
	if (!Sub || CaptureRateSteps.Num() == 0 || Delta == 0)
	{
		return;
	}

	// Move to the neighbouring step, choosing the nearest entry to the current
	// value first so an out-of-list rate set elsewhere still steps sensibly.
	const int32 Current = Sub->GetCaptureRate();
	int32		Nearest = 0;
	for (int32 i = 1; i < CaptureRateSteps.Num(); ++i)
	{
		if (FMath::Abs(CaptureRateSteps[i] - Current) < FMath::Abs(CaptureRateSteps[Nearest] - Current))
		{
			Nearest = i;
		}
	}
	const int32 Next = FMath::Clamp(Nearest + Delta, 0, CaptureRateSteps.Num() - 1);
	Sub->SetCaptureRate(FMath::Max(1, CaptureRateSteps[Next]));
	RefreshLabels();
}

void URammsCameraCapturePanel::HandleCaptureClicked()
{
	ToggleCapture();
}

void URammsCameraCapturePanel::HandleSerializationClicked()
{
	ToggleSerialization();
}

void URammsCameraCapturePanel::HandleRateUpClicked()
{
	StepCaptureRate(1);
}

void URammsCameraCapturePanel::HandleRateDownClicked()
{
	StepCaptureRate(-1);
}
