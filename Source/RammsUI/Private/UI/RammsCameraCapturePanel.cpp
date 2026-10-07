// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsCameraCapturePanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#include "UI/RammsButton.h"
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
	PanelBorder->SetHorizontalAlignment(HAlign_Left);
	PanelBorder->SetVerticalAlignment(VAlign_Top);
	WidgetTree->RootWidget = PanelBorder;

	MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainVBox"));
	PanelBorder->AddChild(MainVBox);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("Camera Capture")));
	if (UVerticalBoxSlot* Slot = MainVBox->AddChildToVerticalBox(TitleText))
	{
		Slot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 4.0f));
	}

	auto AddButtonTo = [this](UHorizontalBox* Box, const TCHAR* Name, const TCHAR* Label) -> URammsButton* {
		URammsButton* Button = WidgetTree->ConstructWidget<URammsButton>(URammsButton::StaticClass(), Name);
		Button->SetText(FText::FromString(Label));
		if (UHorizontalBoxSlot* Slot = Box->AddChildToHorizontalBox(Button))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		return Button;
	};

	if (bShowControls)
	{
		ControlsHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ControlsHBox"));
		if (UVerticalBoxSlot* Slot = MainVBox->AddChildToVerticalBox(ControlsHBox))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}

		CaptureButton = AddButtonTo(ControlsHBox, TEXT("CaptureButton"), TEXT("Start Capture"));
		CaptureButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleCaptureClicked);

		SerializationButton = AddButtonTo(ControlsHBox, TEXT("SerializationButton"), TEXT("Saving: off"));
		SerializationButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleSerializationClicked);

		if (bShowCaptureRate)
		{
			RateDownButton = AddButtonTo(ControlsHBox, TEXT("RateDownButton"), TEXT("-"));
			RateDownButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleRateDownClicked);

			RateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RateText"));
			RateText->SetText(FText::FromString(TEXT("every frame")));
			if (UHorizontalBoxSlot* Slot = ControlsHBox->AddChildToHorizontalBox(RateText))
			{
				Slot->SetPadding(FMargin(2.0f, 0.0f, 4.0f, 0.0f));
				Slot->SetVerticalAlignment(VAlign_Center);
			}

			RateUpButton = AddButtonTo(ControlsHBox, TEXT("RateUpButton"), TEXT("+"));
			RateUpButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleRateUpClicked);
		}
	}

	if (bShowStatistics)
	{
		StatsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatsText"));
		StatsText->SetText(FText::GetEmpty());
		if (UVerticalBoxSlot* Slot = MainVBox->AddChildToVerticalBox(StatsText))
		{
			Slot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 6.0f));
		}
	}

	// Camera selector: < name > [channel]
	SelectorHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SelectorHBox"));
	if (UVerticalBoxSlot* Slot = MainVBox->AddChildToVerticalBox(SelectorHBox))
	{
		Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	SelectorHBox->SetClipping(EWidgetClipping::ClipToBounds);

	PrevCameraButton = AddButtonTo(SelectorHBox, TEXT("PrevCameraButton"), TEXT("<"));
	PrevCameraButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandlePrevCameraClicked);

	CameraNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CameraNameText"));
	CameraNameText->SetText(FText::FromString(TEXT("no cameras")));
	if (UHorizontalBoxSlot* Slot = SelectorHBox->AddChildToHorizontalBox(CameraNameText))
	{
		Slot->SetPadding(FMargin(2.0f, 0.0f, 4.0f, 0.0f));
		Slot->SetVerticalAlignment(VAlign_Center);
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	NextCameraButton = AddButtonTo(SelectorHBox, TEXT("NextCameraButton"), TEXT(">"));
	NextCameraButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleNextCameraClicked);

	ChannelButton = AddButtonTo(SelectorHBox, TEXT("ChannelButton"), TEXT("Colour"));
	ChannelButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleChannelClicked);

	// The feed: a plain image with the render target as its brush. No chrome, no
	// stream resolution, no display modes -- just the pixels, sized here.
	FeedBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FeedBox"));
	FeedBox->SetWidthOverride(FeedWidth);
	FeedBox->SetHeightOverride(FeedWidth * 0.75f); // replaced once a target is known
	if (UVerticalBoxSlot* Slot = MainVBox->AddChildToVerticalBox(FeedBox))
	{
		Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
	}

	FeedImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FeedImage"));
	FeedBox->AddChild(FeedImage);
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
	SelectorHBox = nullptr;
	PrevCameraButton = nullptr;
	NextCameraButton = nullptr;
	ChannelButton = nullptr;
	CameraNameText = nullptr;
	FeedBox = nullptr;
	FeedImage = nullptr;
	Cameras.Reset();
}

UWidget* URammsCameraCapturePanel::GetRootWidgetForValidation()
{
	return PanelBorder;
}

void URammsCameraCapturePanel::NativeConstruct()
{
	BuildWidgetTree();
	Super::NativeConstruct();
	RefreshCameraList();
	UpdateFeedImage();
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
		TitleText->SetFont(Style->Typography.HeadingSmall);
		TitleText->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}
	for (UTextBlock* Text : { StatsText.Get(), RateText.Get(), CameraNameText.Get() })
	{
		if (Text)
		{
			Text->SetFont(Style->Typography.Caption);
			Text->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
		}
	}
	// Buttons are created at runtime, outside this widget's tree, so they are
	// handed the style directly -- the same thing the control surface panel does.
	for (URammsButton* Button : { CaptureButton.Get(), SerializationButton.Get(), RateDownButton.Get(),
			 RateUpButton.Get(), PrevCameraButton.Get(), NextCameraButton.Get(), ChannelButton.Get() })
	{
		if (Button)
		{
			Button->SetStyle(Style);
		}
	}
}

// ---------------------------------------------------------------------------
// Subsystem / cameras
// ---------------------------------------------------------------------------

UCameraCaptureSubsystem* URammsCameraCapturePanel::GetSubsystem() const
{
	// A world subsystem, so it exists only while a world does -- null in the
	// widget designer, and null before BeginPlay.
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UCameraCaptureSubsystem>() : nullptr;
}

UIntrinsicSceneCaptureComponent2D* URammsCameraCapturePanel::GetSelectedCamera() const
{
	return Cameras.IsValidIndex(SelectedCamera) ? Cameras[SelectedCamera].Get() : nullptr;
}

void URammsCameraCapturePanel::RefreshCameraList()
{
	Cameras.Reset();
	if (UCameraCaptureSubsystem* Sub = GetSubsystem())
	{
		for (UIntrinsicSceneCaptureComponent2D* Camera : Sub->GetRegisteredCameras())
		{
			if (Camera)
			{
				Cameras.Add(Camera);
			}
		}
	}
	// Keep the selection in range rather than letting it point past the end when
	// a camera unregisters.
	SelectedCamera = Cameras.Num() > 0 ? FMath::Clamp(SelectedCamera, 0, Cameras.Num() - 1) : 0;
}

void URammsCameraCapturePanel::UpdateFeedImage()
{
	if (!FeedImage || !FeedBox)
	{
		return;
	}

	UIntrinsicSceneCaptureComponent2D* Camera = GetSelectedCamera();
	UTextureRenderTarget2D*			   Target = nullptr;
	if (Camera)
	{
		UCameraCaptureSubsystem* Sub = GetSubsystem();
		// Spelled out rather than a ternary: TextureTarget is a TObjectPtr and the
		// accessor returns a raw pointer, which makes the conditional ambiguous.
		if (bShowDepthChannel && Sub)
		{
			Target = Sub->GetDepthRenderTarget(Camera);
		}
		else
		{
			Target = Camera->TextureTarget;
		}
	}

	if (!Target)
	{
		FeedImage->SetBrushResourceObject(nullptr);
		FeedImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	FeedImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	FeedImage->SetBrushResourceObject(Target);

	// Size from the target's own aspect, so a 4:3 depth target and a 16:9 colour
	// target both fill the width and neither is stretched.
	const float Aspect = Target->SizeY > 0 ? static_cast<float>(Target->SizeX) / static_cast<float>(Target->SizeY) : 1.0f;
	const float Height = Aspect > 0.0f ? FeedWidth / Aspect : FeedWidth;
	FeedImage->SetBrushSize(FVector2D(FeedWidth, Height));
	FeedBox->SetWidthOverride(FeedWidth);
	FeedBox->SetHeightOverride(Height);
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

	// Re-read the list only when it actually changed. Cameras register on
	// BeginPlay and unregister with their actor, so it is not fixed.
	if (UCameraCaptureSubsystem* Sub = GetSubsystem())
	{
		const TArray<UIntrinsicSceneCaptureComponent2D*> Current = Sub->GetRegisteredCameras();
		bool											 bChanged = Current.Num() != Cameras.Num();
		if (!bChanged)
		{
			for (int32 i = 0; i < Current.Num(); ++i)
			{
				if (Cameras[i].Get() != Current[i])
				{
					bChanged = true;
					break;
				}
			}
		}
		if (bChanged)
		{
			RefreshCameraList();
		}
	}

	// Cheap, and it keeps the brush pointing at the right target if a camera's
	// render target is recreated -- a resolution change does exactly that.
	UpdateFeedImage();
	RefreshLabels();
}

void URammsCameraCapturePanel::RefreshLabels()
{
	UCameraCaptureSubsystem* Sub = GetSubsystem();

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(
			Sub ? FString::Printf(TEXT("Camera Capture  (%d)"), Cameras.Num())
				: FString(TEXT("Camera Capture  (no subsystem)"))));
	}

	const bool bEnabled = Sub != nullptr;
	for (URammsButton* Button : { CaptureButton.Get(), SerializationButton.Get(), RateDownButton.Get(), RateUpButton.Get() })
	{
		if (Button)
		{
			Button->SetEnabled(bEnabled);
		}
	}
	const bool bMultiple = Cameras.Num() > 1;
	for (URammsButton* Button : { PrevCameraButton.Get(), NextCameraButton.Get() })
	{
		if (Button)
		{
			Button->SetEnabled(bMultiple);
		}
	}
	if (ChannelButton)
	{
		ChannelButton->SetEnabled(Cameras.Num() > 0);
		ChannelButton->SetText(FText::FromString(bShowDepthChannel ? TEXT("Depth") : TEXT("Colour")));
	}

	if (CameraNameText)
	{
		if (UIntrinsicSceneCaptureComponent2D* Camera = GetSelectedCamera())
		{
			// Component name only. The owning actor is usually the same for every
			// camera, so the prefix is the part that is never the answer, and it is
			// what pushes the useful half out of the panel.
			// Truncated, not wrapped and not left to overflow. A Fill slot does not
			// shrink a text block below its desired size, so a long component name
			// pushes the next/channel buttons out from under the pointer -- the
			// name grows and the controls are what you lose.
			FString		Name = Camera->GetName();
			const int32 MaxNameChars = 22;
			if (Name.Len() > MaxNameChars)
			{
				Name = Name.Left(MaxNameChars - 1) + TEXT("\u2026");
			}
			CameraNameText->SetText(FText::FromString(FString::Printf(
				TEXT("%d/%d  %s"), SelectedCamera + 1, Cameras.Num(), *Name)));
		}
		else
		{
			CameraNameText->SetText(FText::FromString(TEXT("no cameras registered")));
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
			N <= 1 ? FString(TEXT("every frame")) : FString::Printf(TEXT("every %d"), N)));
	}
	if (StatsText)
	{
		const FCaptureStatistics Stats = Sub->GetStatistics();

		// The output directory is stored relative to the project, which prints as
		// a stack of "../.." that overflows the panel and says nothing. Show the
		// last couple of components, which is the part that names the run.
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
			Stats.TotalFramesCaptured, Stats.AverageCaptureTimeMs, *ShortDir)));
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

	// Start from the nearest listed step, so a rate set elsewhere and not in the
	// list still moves sensibly rather than jumping to one end.
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

void URammsCameraCapturePanel::StepCamera(int32 Delta)
{
	if (Cameras.Num() == 0 || Delta == 0)
	{
		return;
	}
	SelectedCamera = ((SelectedCamera + Delta) % Cameras.Num() + Cameras.Num()) % Cameras.Num();
	UpdateFeedImage();
	RefreshLabels();
}

void URammsCameraCapturePanel::ToggleChannel()
{
	bShowDepthChannel = !bShowDepthChannel;
	UpdateFeedImage();
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
void URammsCameraCapturePanel::HandlePrevCameraClicked()
{
	StepCamera(-1);
}
void URammsCameraCapturePanel::HandleNextCameraClicked()
{
	StepCamera(1);
}
void URammsCameraCapturePanel::HandleChannelClicked()
{
	ToggleChannel();
}
