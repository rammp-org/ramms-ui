// RammsSceneCameraView.cpp

#include "UI/RammsSceneCameraView.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "UI/RammsButton.h"
#include "UI/RammsCameraWidget.h"

void URammsSceneCameraView::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	// Taps go through the view to the camera's markers and the button, or on to what's behind it
	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SceneRoot"));
	RootOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootOverlay;

	Camera = WidgetTree->ConstructWidget<URammsCameraWidget>(URammsCameraWidget::StaticClass(), TEXT("SceneCamera"));
	ConfigureCamera();
	if (UOverlaySlot* CameraSlot = RootOverlay->AddChildToOverlay(Camera))
	{
		CameraSlot->SetHorizontalAlignment(HAlign_Fill);
		CameraSlot->SetVerticalAlignment(VAlign_Fill);
	}

	ToggleButton = WidgetTree->ConstructWidget<URammsButton>(URammsButton::StaticClass(), TEXT("SceneCameraToggle"));
	ToggleButton->SetVariant(ERammsButtonVariant::Secondary);
	ToggleButton->SetCompact(true);
	if (UOverlaySlot* ButtonSlot = RootOverlay->AddChildToOverlay(ToggleButton))
	{
		ButtonSlot->SetHorizontalAlignment(ToggleHAlign);
		ButtonSlot->SetVerticalAlignment(ToggleVAlign);
		ButtonSlot->SetPadding(TogglePadding);
	}

	ApplyShown();
}

void URammsSceneCameraView::ResetCachedWidgets()
{
	RootOverlay = nullptr;
	Camera = nullptr;
	ToggleButton = nullptr;
}

UWidget* URammsSceneCameraView::GetRootWidgetForValidation()
{
	return RootOverlay;
}

void URammsSceneCameraView::ConfigureCamera()
{
	if (!Camera)
	{
		return;
	}
	Camera->StreamSet = ERammsCameraStreamSet::Scene;
	Camera->DisplayMode = ERammsCameraDisplayMode::Widget;
	Camera->bShowLabel = false;
	Camera->bShowDisplayModeCycleButton = false;
	Camera->bEnableDrag = false;
	Camera->bFocusOnClick = false;
	Camera->bCollapsible = false;
	Camera->bMaintainAspectRatio = bMaintainAspectRatio;
	Camera->DetectionMarkerClass = MarkerClass;
	Camera->DetectionMarkerPlacement = MarkerPlacement;
	Camera->bDrawDetectionBoxes = !MarkerClass;
	Camera->bInteractiveBoundingBoxOverlay = true;
	Camera->bReceiveBridgeDetections = true;

	// The camera built its title bar (stream name, view-mode buttons) when it was created, before
	// these settings reached it; the background view has none
	if (Camera->TitleBar)
	{
		Camera->TitleBar->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URammsSceneCameraView::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (IsDesignTime())
	{
		ConfigureCamera();
		if (RootOverlay && ToggleButton)
		{
			if (UOverlaySlot* ButtonSlot = Cast<UOverlaySlot>(ToggleButton->Slot))
			{
				ButtonSlot->SetHorizontalAlignment(ToggleHAlign);
				ButtonSlot->SetVerticalAlignment(ToggleVAlign);
				ButtonSlot->SetPadding(TogglePadding);
			}
		}
	}
	ApplyShown();
}

void URammsSceneCameraView::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (ToggleButton)
	{
		ToggleButton->OnClicked.AddUniqueDynamic(this, &URammsSceneCameraView::HandleToggleClicked);
	}
	ApplyShown();
}

void URammsSceneCameraView::NativeDestruct()
{
	if (ToggleButton)
	{
		ToggleButton->OnClicked.RemoveDynamic(this, &URammsSceneCameraView::HandleToggleClicked);
	}
	Super::NativeDestruct();
}

void URammsSceneCameraView::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The camera receives frames even while collapsed; show the view once there is one
	if (!bHadFrame && Camera)
	{
		const FVector2D ImageSize = Camera->GetImageSize();
		if (ImageSize.X > 0.0 && ImageSize.Y > 0.0)
		{
			bHadFrame = true;
			ApplyShown();
		}
	}
}

void URammsSceneCameraView::SetSceneCameraShown(bool bShown)
{
	if (bSceneCameraShown == bShown)
	{
		return;
	}
	bSceneCameraShown = bShown;
	ApplyShown();
	OnSceneCameraShownChanged.Broadcast(bSceneCameraShown);
}

void URammsSceneCameraView::HandleToggleClicked()
{
	ToggleSceneCamera();
}

void URammsSceneCameraView::ApplyShown()
{
	const bool bAvailable = bHadFrame || !bWaitForFirstFrame || IsDesignTime();
	if (Camera)
	{
		Camera->SetVisibility(bAvailable && bSceneCameraShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ToggleButton)
	{
		ToggleButton->SetVisibility(bAvailable && bShowToggleButton ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		ToggleButton->SetText(bSceneCameraShown ? HideCameraText : ShowCameraText);
	}
}
