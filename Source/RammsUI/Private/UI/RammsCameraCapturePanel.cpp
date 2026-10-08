// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsCameraCapturePanel.h"

#include "RammsControlHUDSettings.h"

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
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

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

	// One width for the whole panel, declared once. Without it each row sizes to
	// its own content and the longest one decides how far the panel reaches --
	// which is how the status line and the rate stepper ended up past the edge.
	USizeBox* WidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("WidthBox"));
	WidthBox->SetWidthOverride(FeedWidth);
	WidthBox->SetClipping(EWidgetClipping::ClipToBounds);
	PanelBorder->AddChild(WidthBox);

	MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainVBox"));
	WidthBox->AddChild(MainVBox);

	auto AddButtonTo = [this](UHorizontalBox* Box, const TCHAR* Name, const TCHAR* Label, float FillOrAuto) -> URammsButton* {
		URammsButton* Button = WidgetTree->ConstructWidget<URammsButton>(URammsButton::StaticClass(), Name);
		Button->SetText(FText::FromString(Label));
		if (UHorizontalBoxSlot* Slot = Box->AddChildToHorizontalBox(Button))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
			Slot->SetVerticalAlignment(VAlign_Center);
			if (FillOrAuto > 0.0f)
			{
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
		return Button;
	};

	// ── Title row: title, and the collapse toggle pinned to the right ──
	TitleHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleHBox"));
	MainVBox->AddChildToVerticalBox(TitleHBox);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("Camera Capture")));
	if (UHorizontalBoxSlot* Slot = TitleHBox->AddChildToHorizontalBox(TitleText))
	{
		Slot->SetPadding(FMargin(2.0f, 0.0f, 4.0f, 4.0f));
		Slot->SetVerticalAlignment(VAlign_Center);
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	CollapseButton = AddButtonTo(TitleHBox, TEXT("CollapseButton"), TEXT("-"), 0.0f);
	CollapseButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleCollapseClicked);

	// ── Everything else, hidden when collapsed ──
	ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentVBox"));
	MainVBox->AddChildToVerticalBox(ContentVBox);

	if (bShowControls)
	{
		ControlsHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ControlsHBox"));
		ControlsHBox->SetClipping(EWidgetClipping::ClipToBounds);
		if (UVerticalBoxSlot* Slot = ContentVBox->AddChildToVerticalBox(ControlsHBox))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}

		CaptureButton = AddButtonTo(ControlsHBox, TEXT("CaptureButton"), TEXT("Start Capture"), 1.0f);
		CaptureButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleCaptureClicked);

		SerializationButton = AddButtonTo(ControlsHBox, TEXT("SerializationButton"), TEXT("Saving: off"), 1.0f);
		SerializationButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleSerializationClicked);

		if (bShowCaptureRate)
		{
			// Its own row. Five controls on one line is more than this column
			// holds, and the overflow fell off the right-hand edge.
			RateHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RateHBox"));
			RateHBox->SetClipping(EWidgetClipping::ClipToBounds);
			if (UVerticalBoxSlot* Slot = ContentVBox->AddChildToVerticalBox(RateHBox))
			{
				Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
			}

			RateDownButton = AddButtonTo(RateHBox, TEXT("RateDownButton"), TEXT("-"), 0.0f);
			RateDownButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleRateDownClicked);

			RateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RateText"));
			RateText->SetText(FText::FromString(TEXT("every frame")));
			RateText->SetJustification(ETextJustify::Center);
			if (UHorizontalBoxSlot* Slot = RateHBox->AddChildToHorizontalBox(RateText))
			{
				Slot->SetPadding(FMargin(2.0f, 0.0f, 4.0f, 0.0f));
				Slot->SetVerticalAlignment(VAlign_Center);
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}

			RateUpButton = AddButtonTo(RateHBox, TEXT("RateUpButton"), TEXT("+"), 0.0f);
			RateUpButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleRateUpClicked);
		}
	}

	// ── Camera selector: < name > [channel] ──
	SelectorHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SelectorHBox"));
	SelectorHBox->SetClipping(EWidgetClipping::ClipToBounds);
	if (UVerticalBoxSlot* Slot = ContentVBox->AddChildToVerticalBox(SelectorHBox))
	{
		Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	PrevCameraButton = AddButtonTo(SelectorHBox, TEXT("PrevCameraButton"), TEXT("<"), 0.0f);
	PrevCameraButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandlePrevCameraClicked);

	CameraNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CameraNameText"));
	CameraNameText->SetText(FText::FromString(TEXT("no cameras")));
	// Clip rather than grow. A Fill slot does not shrink a text block below its
	// desired size, so without this a long name pushes the buttons beside it out
	// of the panel -- the name grows and the controls are what you lose.
	CameraNameText->SetClipping(EWidgetClipping::ClipToBounds);
	if (UHorizontalBoxSlot* Slot = SelectorHBox->AddChildToHorizontalBox(CameraNameText))
	{
		Slot->SetPadding(FMargin(2.0f, 0.0f, 4.0f, 0.0f));
		Slot->SetVerticalAlignment(VAlign_Center);
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	NextCameraButton = AddButtonTo(SelectorHBox, TEXT("NextCameraButton"), TEXT(">"), 0.0f);
	NextCameraButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleNextCameraClicked);

	ChannelButton = AddButtonTo(SelectorHBox, TEXT("ChannelButton"), TEXT("Colour"), 0.0f);
	ChannelButton->OnClicked.AddDynamic(this, &URammsCameraCapturePanel::HandleChannelClicked);

	// ── The feed: a plain image with the render target as its brush ──
	FeedBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FeedBox"));
	FeedBox->SetWidthOverride(FeedWidth);
	FeedBox->SetHeightOverride(FeedWidth * 0.75f); // replaced once a target is known
	if (UVerticalBoxSlot* Slot = ContentVBox->AddChildToVerticalBox(FeedBox))
	{
		Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	FeedImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FeedImage"));
	FeedBox->AddChild(FeedImage);

	// ── Status last, and wrapped: it is the longest line and the least urgent,
	// so it is the one that should reflow rather than push anything around. ──
	if (bShowStatistics)
	{
		StatsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatsText"));
		StatsText->SetText(FText::GetEmpty());
		StatsText->SetAutoWrapText(true);
		StatsText->SetWrapTextAt(FeedWidth - 4.0f);
		if (UVerticalBoxSlot* Slot = ContentVBox->AddChildToVerticalBox(StatsText))
		{
			Slot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 0.0f));
		}
	}

	bCollapsed = bStartCollapsed;
	ContentVBox->SetVisibility(bCollapsed ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

void URammsCameraCapturePanel::ResetCachedWidgets()
{
	PanelBorder = nullptr;
	MainVBox = nullptr;
	TitleText = nullptr;
	TitleHBox = nullptr;
	CollapseButton = nullptr;
	ContentVBox = nullptr;
	ControlsHBox = nullptr;
	RateHBox = nullptr;
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
			 RateUpButton.Get(), PrevCameraButton.Get(), NextCameraButton.Get(), ChannelButton.Get(),
			 CollapseButton.Get() })
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
	// What is on screen is a camera, not a position in a list. Clamping an index
	// across the rebuild quietly swapped the feed whenever an EARLIER camera
	// unregistered: with A/B/C and B showing, losing A leaves index 1 on C while
	// B is still registered and still what the user asked for.
	UIntrinsicSceneCaptureComponent2D* Previous = Cameras.IsValidIndex(SelectedCamera) ? Cameras[SelectedCamera].Get() : nullptr;

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

	const int32 FoundAt = Previous ? Cameras.IndexOfByPredicate(
										 [Previous](const TWeakObjectPtr<UIntrinsicSceneCaptureComponent2D>& C) { return C.Get() == Previous; })
								   : INDEX_NONE;

	// The clamp is the fallback for the one case it is right for: the camera
	// being shown is gone, so some neighbouring index is the best guess left.
	SelectedCamera = FoundAt != INDEX_NONE
		? FoundAt
		: (Cameras.Num() > 0 ? FMath::Clamp(SelectedCamera, 0, Cameras.Num() - 1) : 0);
}

void URammsCameraCapturePanel::ApplyHUDSettings()
{
	const URammsControlHUDSettings* Settings = GetDefault<URammsControlHUDSettings>();
	if (!Settings)
	{
		return;
	}
	DepthMinCM = Settings->DepthColormapMinCM;
	DepthMaxCM = Settings->DepthColormapMaxCM;
	bDepthColormapRepeat = Settings->bDepthColormapRepeat;
	DepthColormapIndex = Settings->DepthColormapIndex;
	MotionSensitivity = Settings->MotionSensitivity;
}

void URammsCameraCapturePanel::EnsureDepthMaterials()
{
	// Same material twice, differing only in which channel feeds the colormap.
	// The DMV pass writes depth to red; SingleCaptureColorDepth packs it into the
	// colour target's alpha, which is why one variant exists at all -- without it
	// the depth view in that mode had no texture to point at and the feed
	// vanished rather than merely being blank.
	// Loaded from soft references, not literal paths: a path the cooker cannot
	// see is an asset a packaged build can be missing, and the failure is quiet.
	auto Build = [this](const TSoftObjectPtr<UMaterialInterface>& Ref, TObjectPtr<UMaterialInstanceDynamic>& Out, const TCHAR* What) {
		if (Out || Ref.IsNull())
		{
			return;
		}
		if (UMaterialInterface* Base = Ref.LoadSynchronous())
		{
			Out = UMaterialInstanceDynamic::Create(Base, this);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[RammsCameraCapturePanel] Could not load the %s material (%s); that view will show the raw render ")
					TEXT("target instead of a colormap."),
				What, *Ref.ToSoftObjectPath().ToString());
		}
	};

	Build(DepthColormapMaterial, DepthFromRedMID, TEXT("depth"));
	Build(DepthColormapAlphaMaterial, DepthFromAlphaMID, TEXT("depth-in-alpha"));
	Build(MotionColormapMaterial, MotionMID, TEXT("motion-vector"));
}

bool URammsCameraCapturePanel::IsChannelAvailable(ERammsFeedChannel Channel) const
{
	UIntrinsicSceneCaptureComponent2D* Camera = GetSelectedCamera();
	if (!Camera)
	{
		return false;
	}
	UCameraCaptureSubsystem* Sub = GetSubsystem();

	switch (Channel)
	{
		case ERammsFeedChannel::Colour:
			return Camera->TextureTarget != nullptr;

		case ERammsFeedChannel::Depth:
		{
			if (!Sub || !Sub->IsCapturingDepth())
			{
				return false;
			}
			// Single capture has no depth target: depth is in the colour
			// target's alpha, so that target is what it needs.
			return Sub->GetCaptureMode() == ERammsCaptureMode::SingleCaptureColorDepth
				? Camera->TextureTarget != nullptr
				: Sub->GetDepthRenderTarget(Camera) != nullptr;
		}

		case ERammsFeedChannel::Motion:
			// Its own pass, so its own target, and available in either capture
			// mode -- which it was not when motion rode on the depth pass.
			return Sub && Sub->IsCapturingMotionVectors() && Sub->GetMotionRenderTarget(Camera) != nullptr;

		default:
			return false;
	}
}

void URammsCameraCapturePanel::UpdateFeedImage()
{
	if (!FeedImage || !FeedBox)
	{
		return;
	}

	UIntrinsicSceneCaptureComponent2D* Camera = GetSelectedCamera();
	if (!Camera)
	{
		FeedImage->SetBrushResourceObject(nullptr);
		FeedImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UCameraCaptureSubsystem* Sub = GetSubsystem();
	UTextureRenderTarget2D*	 ColourTarget = Camera->TextureTarget;
	UTextureRenderTarget2D*	 DepthTarget = Sub ? Sub->GetDepthRenderTarget(Camera) : nullptr;

	// The selected channel may have stopped being available since it was picked
	// -- the capture mode can change under the panel, and a camera can lose its
	// DMV target. Fall back rather than drawing the wrong plane.
	if (FeedChannel != ERammsFeedChannel::Colour && !IsChannelAvailable(FeedChannel))
	{
		FeedChannel = ERammsFeedChannel::Colour;
	}

	// Which texture the feed draws, and whether it goes through a colormap.
	UTextureRenderTarget2D*	  Source = ColourTarget;
	UMaterialInstanceDynamic* Material = nullptr;

	if (FeedChannel == ERammsFeedChannel::Depth)
	{
		EnsureDepthMaterials();

		// Ask the mode, not the target. A null depth target is also what a
		// two-render camera returns before it is set up, and what one returns
		// when the DMV material is missing -- treating null as "depth is in
		// alpha" colormapped the colour target's alpha in those states and
		// presented inverted opacity as a distance measurement.
		const bool bDepthInAlpha = Sub && Sub->GetCaptureMode() == ERammsCaptureMode::SingleCaptureColorDepth;

		if (bDepthInAlpha)
		{
			// Single capture: there is no depth target, because depth rides in
			// the colour target's alpha. Same colormap, alpha variant.
			Source = ColourTarget;
			Material = DepthFromAlphaMID;
		}
		else if (DepthTarget)
		{
			// Also alpha. This read the target's RED channel back when that target
			// came from the DMV post-process pass; after depth moved to the depth
			// buffer, red there is linear scene colour, so the depth view in this
			// mode was colormapping the picture.
			Source = DepthTarget;
			Material = DepthFromAlphaMID;
		}
		// Otherwise Source stays null and the feed collapses, which is honest:
		// this camera has no depth to show yet.
	}
	else if (FeedChannel == ERammsFeedChannel::Motion)
	{
		EnsureDepthMaterials();
		// Motion has its own target now, in either capture mode -- it is no
		// longer the depth pass wearing a second hat.
		if (UTextureRenderTarget2D* MotionTarget = Sub ? Sub->GetMotionRenderTarget(Camera) : nullptr)
		{
			Source = MotionTarget;
			Material = MotionMID;
		}
		else
		{
			Source = nullptr;
		}
	}

	if (!Source)
	{
		FeedImage->SetBrushResourceObject(nullptr);
		FeedImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// Size from the source's own aspect, so a 4:3 depth target and a 16:9 colour
	// target both fill the width and neither is stretched.
	const float Aspect = Source->SizeY > 0 ? static_cast<float>(Source->SizeX) / static_cast<float>(Source->SizeY) : 1.0f;
	const float Height = Aspect > 0.0f ? FeedWidth / Aspect : FeedWidth;

	FeedImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (Material)
	{
		// Setting a parameter a material does not have is a no-op, so the depth
		// range goes on unconditionally rather than branching per material.
		Material->SetTextureParameterValue(TEXT("DataTexture"), Source);
		// CENTIMETRES, which is what the data is: both modes capture depth with
		// SCS_SceneColorSceneDepth, measured at 615..1026 cm for real geometry
		// with an enormous sentinel where the sky is. The material normalises
		// (depth - Near) / (Far - Near) in whatever units it is handed, so the
		// planes have to be in the same ones.
		//
		// There was a branch here giving one source a 0..1 range, from when the
		// second one was the tonemapped DMV pass and had no unit at all. Both
		// read the depth buffer now, so there is one answer.
		//
		// Note the range these default to is a VIEWING choice, not a measurement:
		// the plane values only decide which distances the colour ramp spans.
		Material->SetScalarParameterValue(TEXT("DepthMin"), DepthMinCM);
		Material->SetScalarParameterValue(TEXT("DepthMax"), DepthMaxCM);
		// Repeating turns the ramp into contour bands, one per (Max - Min), so a
		// tight range still says something about distant surfaces instead of
		// flattening them all to the far colour.
		Material->SetScalarParameterValue(TEXT("DepthWrap"), bDepthColormapRepeat ? 1.0f : 0.0f);
		Material->SetScalarParameterValue(TEXT("ColormapIndex"), static_cast<float>(DepthColormapIndex));
		Material->SetScalarParameterValue(TEXT("Sensitivity"), MotionSensitivity);
		Material->SetVectorParameterValue(TEXT("MotionXMask"), MotionXMask);
		Material->SetVectorParameterValue(TEXT("MotionYMask"), MotionYMask);
		Material->SetVectorParameterValue(TEXT("ImageSize"), FLinearColor(FeedWidth, Height, 0.0f, 0.0f));
		Material->SetVectorParameterValue(TEXT("CornerRadii"), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		FeedImage->SetBrushResourceObject(Material);
	}
	else
	{
		FeedImage->SetBrushResourceObject(Source);
	}

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

	// Collapsed, nothing below the title is visible, so none of it is worth
	// recomputing -- only the title and the collapse button still show.
	if (!bCollapsed)
	{
		// Cheap, and it keeps the brush pointing at the right target if a camera's
		// render target is recreated -- a resolution change does exactly that.
		UpdateFeedImage();
	}
	RefreshLabels();
}

void URammsCameraCapturePanel::RefreshLabels()
{
	// Cheap, and it means changing a value in Project Settings shows up without
	// restarting play.
	ApplyHUDSettings();

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
	if (CollapseButton)
	{
		CollapseButton->SetText(FText::FromString(bCollapsed ? TEXT("+") : TEXT("-")));
	}
	if (ChannelButton)
	{
		// Disabled when there is nothing to cycle TO, which is the honest state
		// in single capture with depth off, or with no camera at all.
		const bool bHasAlternative = IsChannelAvailable(ERammsFeedChannel::Depth)
			|| IsChannelAvailable(ERammsFeedChannel::Motion);
		ChannelButton->SetEnabled(Cameras.Num() > 0 && bHasAlternative);

		const TCHAR* ChannelName = TEXT("Colour");
		const TCHAR* ChannelHint = TEXT("Showing colour");
		switch (FeedChannel)
		{
			case ERammsFeedChannel::Depth:
				ChannelName = TEXT("Depth");
				ChannelHint = TEXT("Showing depth, colormapped");
				break;
			case ERammsFeedChannel::Motion:
				ChannelName = TEXT("Motion");
				ChannelHint = TEXT("Showing motion vectors, colormapped by magnitude and angle");
				break;
			default:
				break;
		}
		ChannelButton->SetText(FText::FromString(ChannelName));
		ChannelButton->SetToolTipText(FText::FromString(ChannelHint));
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
	// Walk forward to the next channel this camera can show. At most three steps,
	// and colour is the backstop -- a camera with a render target always has it,
	// so the loop cannot spin.
	static constexpr int32 NumChannels = 3;
	for (int32 Step = 1; Step <= NumChannels; ++Step)
	{
		const ERammsFeedChannel Candidate =
			static_cast<ERammsFeedChannel>((static_cast<int32>(FeedChannel) + Step) % NumChannels);
		if (IsChannelAvailable(Candidate))
		{
			FeedChannel = Candidate;
			break;
		}
	}
	UpdateFeedImage();
	RefreshLabels();
}

void URammsCameraCapturePanel::SetFeedChannel(ERammsFeedChannel Channel)
{
	FeedChannel = IsChannelAvailable(Channel) ? Channel : ERammsFeedChannel::Colour;
	UpdateFeedImage();
	RefreshLabels();
}

void URammsCameraCapturePanel::ToggleCollapsed()
{
	bCollapsed = !bCollapsed;
	if (ContentVBox)
	{
		ContentVBox->SetVisibility(bCollapsed ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	RefreshLabels();
}

void URammsCameraCapturePanel::HandleCollapseClicked()
{
	ToggleCollapsed();
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
