// RammsSceneCameraView.h
//
// The robot's scene camera as the main screen's background: the camera image fills the view, the
// robot's scene objects are tappable markers on it, and a button lets the user hide or show the
// camera. That button is local to the UI; nothing about it goes out over the bridge.
//
// Its streams are Project Settings > Ramms Camera Streams' scene camera set (camera/scene/color, with
// camera/scene/depth as its data stream, and which of the bridge's objects are the scene's).
//
// Place it behind the rest of the screen (W_RAMMS_UI puts it at the bottom of its overlay). The
// view stays empty until the first scene frame arrives, so a robot without a scene camera
// leaves the screen as it was. The wrist camera's portal (OpenCameraFeed) opens over it.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "UI/RammsDetectionMarker.h"
#include "RammsSceneCameraView.generated.h"

class UOverlay;
class URammsButton;
class URammsCameraWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRammsSceneCameraShownChanged, bool, bShown);

UCLASS(meta = (DisplayName = "Ramms Scene Camera View"))
class RAMMSUI_API URammsSceneCameraView : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	/** The marker put on each scene object (a Blueprint subclass of Ramms Detection Marker). None = no markers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Camera")
	TSubclassOf<URammsDetectionMarker> MarkerClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Camera")
	ERammsDetectionMarkerPlacement MarkerPlacement = ERammsDetectionMarkerPlacement::Box;

	/** Keep the frame's aspect ratio (letterboxed), so markers line up with the image. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Camera")
	bool bMaintainAspectRatio = true;

	/** Show nothing, not even the button, until the first scene frame arrives. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Camera")
	bool bWaitForFirstFrame = true;

	/** The user's choice: show the scene camera (true) or hide it. Changed by the button or SetSceneCameraShown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Camera")
	bool bSceneCameraShown = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Camera|Button")
	bool bShowToggleButton = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Camera|Button")
	TEnumAsByte<EHorizontalAlignment> ToggleHAlign = HAlign_Right;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Camera|Button")
	TEnumAsByte<EVerticalAlignment> ToggleVAlign = VAlign_Top;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Camera|Button")
	FMargin TogglePadding = FMargin(24.0f, 24.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Camera|Button")
	FText HideCameraText = NSLOCTEXT("RammsSceneCameraView", "HideCamera", "Hide camera");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Camera|Button")
	FText ShowCameraText = NSLOCTEXT("RammsSceneCameraView", "ShowCamera", "Show camera");

	/** Fired when the scene camera is hidden or shown (by the button or SetSceneCameraShown). */
	UPROPERTY(BlueprintAssignable, Category = "Scene Camera")
	FOnRammsSceneCameraShownChanged OnSceneCameraShownChanged;

	/** Show or hide the scene camera and its markers. Local to the UI. */
	UFUNCTION(BlueprintCallable, Category = "Scene Camera")
	void SetSceneCameraShown(bool bShown);

	UFUNCTION(BlueprintCallable, Category = "Scene Camera")
	void ToggleSceneCamera() { SetSceneCameraShown(!bSceneCameraShown); }

	UFUNCTION(BlueprintPure, Category = "Scene Camera")
	bool IsSceneCameraShown() const { return bSceneCameraShown; }

	/** A scene frame has arrived. */
	UFUNCTION(BlueprintPure, Category = "Scene Camera")
	bool HasSceneFrame() const { return bHadFrame; }

	UFUNCTION(BlueprintPure, Category = "Scene Camera")
	URammsCameraWidget* GetCameraWidget() const { return Camera; }

protected:
	virtual void BuildWidgetTree() override;
	virtual void ResetCachedWidgets() override;
	virtual UWidget* GetRootWidgetForValidation() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void SynchronizeProperties() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> RootOverlay;

	UPROPERTY(Transient)
	TObjectPtr<URammsCameraWidget> Camera;

	UPROPERTY(Transient)
	TObjectPtr<URammsButton> ToggleButton;

	bool bHadFrame = false;

	/** Copy this view's settings to the camera widget (before it's constructed, and on designer changes). */
	void ConfigureCamera();

	/** The camera's and the button's visibility, and the button's text. */
	void ApplyShown();

	UFUNCTION()
	void HandleToggleClicked();
};
