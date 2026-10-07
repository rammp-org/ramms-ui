// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "RammsCameraCapturePanel.generated.h"

class UBorder;
class UImage;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;
class URammsButton;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UCameraCaptureSubsystem;
class UIntrinsicSceneCaptureComponent2D;

/**
 * One camera from the capture subsystem, shown large, plus the controls that
 * decide what the subsystem does.
 *
 * Deliberately one feed and not all of them. A column of thumbnails is the
 * obvious first design and a bad one: every feed is too small to read, and N
 * cameras means N render targets composited every frame for a panel you are
 * only ever looking at one part of. A selector and a single large image shows
 * the thing you are actually trying to see.
 *
 * The image is the camera's own render target drawn straight into a brush --
 * already on the GPU, so no readback -- which also means it keeps updating
 * whether or not anything is being written to disk, the state you want while
 * framing a shot. The depth channel is the same thing for the depth target,
 * when the camera has one; in SingleCaptureColorDepth mode it does not, because
 * depth rides in the colour target's alpha, and the toggle says so.
 *
 * The panel does not own the cameras and never creates one. It reflects the
 * subsystem, and refreshes its camera list when the registered set changes.
 */
UCLASS(meta = (DisplayName = "Ramms Camera Capture Panel"))
class RAMMSUI_API URammsCameraCapturePanel : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	/** Show the capture/serialization buttons. Off gives a monitor-only panel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Capture")
	bool bShowControls = true;

	/** Show the capture-rate stepper. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Capture")
	bool bShowCaptureRate = true;

	/** Show frames-captured and the output directory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Capture")
	bool bShowStatistics = true;

	/** Width of the feed image, in pixels. Height follows the camera's aspect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Capture", meta = (ClampMin = "64.0"))
	float FeedWidth = 320.0f;

	/** How often the labels, statistics and feed texture refresh. The image
	 *  itself is a render target and updates with the renderer, not with this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Capture", meta = (ClampMin = "0.05"))
	float RefreshInterval = 0.25f;

	/** Steps the stepper moves through. Capturing every frame is first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Capture")
	TArray<int32> CaptureRateSteps = { 1, 2, 5, 10, 30, 60 };

	/** Near/far of the depth colormap, in metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Capture|Depth", meta = (ClampMin = "0.0"))
	float DepthMinMetres = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Capture|Depth", meta = (ClampMin = "0.1"))
	float DepthMaxMetres = 10.0f;

	/** Colormap index the material understands: 0 grayscale, 1 jet, 2 turbo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Capture|Depth")
	int32 DepthColormapIndex = 2;

	// ── Actions, also callable from Blueprint or a key binding ──────────

	UFUNCTION(BlueprintCallable, Category = "Camera Capture")
	void ToggleCapture();

	UFUNCTION(BlueprintCallable, Category = "Camera Capture")
	void ToggleSerialization();

	/** Move to the next/previous entry in CaptureRateSteps. */
	UFUNCTION(BlueprintCallable, Category = "Camera Capture")
	void StepCaptureRate(int32 Delta);

	/** Show the next/previous camera. Wraps. */
	UFUNCTION(BlueprintCallable, Category = "Camera Capture")
	void StepCamera(int32 Delta);

	/** Switch the feed between colour and depth. */
	UFUNCTION(BlueprintCallable, Category = "Camera Capture")
	void ToggleChannel();

	/** Collapse to just the title bar, or expand again. */
	UFUNCTION(BlueprintCallable, Category = "Camera Capture")
	void ToggleCollapsed();

	UFUNCTION(BlueprintPure, Category = "Camera Capture")
	bool IsCollapsed() const { return bCollapsed; }

	/** Re-read the registered camera list from the subsystem. */
	UFUNCTION(BlueprintCallable, Category = "Camera Capture")
	void RefreshCameraList();

protected:
	virtual void	 NativeConstruct() override;
	virtual void	 NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void	 ApplyStyle_Implementation() override;
	virtual void	 BuildWidgetTree() override;
	virtual void	 ResetCachedWidgets() override;
	virtual UWidget* GetRootWidgetForValidation() override;

	/** The subsystem on this world, or null outside play. */
	UCameraCaptureSubsystem* GetSubsystem() const;

	/** The camera currently selected, or null. */
	UIntrinsicSceneCaptureComponent2D* GetSelectedCamera() const;

	/** Point the feed image at the selected camera's current channel, and size
	 *  it to that target's aspect. */
	void UpdateFeedImage();

	/** Button text and statistics. Cheap; called on RefreshInterval. */
	void RefreshLabels();

	UFUNCTION()
	void HandleCaptureClicked();
	UFUNCTION()
	void HandleSerializationClicked();
	UFUNCTION()
	void HandleRateUpClicked();
	UFUNCTION()
	void HandleRateDownClicked();
	UFUNCTION()
	void HandlePrevCameraClicked();
	UFUNCTION()
	void HandleNextCameraClicked();
	UFUNCTION()
	void HandleChannelClicked();
	UFUNCTION()
	void HandleCollapseClicked();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> MainVBox;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	/** Title row: the title plus the collapse toggle. */
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> TitleHBox;
	UPROPERTY(Transient)
	TObjectPtr<URammsButton> CollapseButton;

	/** Everything below the title, hidden when collapsed. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ContentVBox;

	/** Capture / saving. The rate stepper is its own row -- all five controls on
	 *  one line did not fit the column and ran off the edge. */
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ControlsHBox;
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> RateHBox;
	UPROPERTY(Transient)
	TObjectPtr<URammsButton> CaptureButton;
	UPROPERTY(Transient)
	TObjectPtr<URammsButton> SerializationButton;
	UPROPERTY(Transient)
	TObjectPtr<URammsButton> RateDownButton;
	UPROPERTY(Transient)
	TObjectPtr<URammsButton> RateUpButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RateText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatsText;

	/** Camera selector: < name > and the channel toggle. */
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> SelectorHBox;
	UPROPERTY(Transient)
	TObjectPtr<URammsButton> PrevCameraButton;
	UPROPERTY(Transient)
	TObjectPtr<URammsButton> NextCameraButton;
	UPROPERTY(Transient)
	TObjectPtr<URammsButton> ChannelButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CameraNameText;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> FeedBox;
	UPROPERTY(Transient)
	TObjectPtr<UImage> FeedImage;

	/** The registered cameras, as of the last refresh. */
	TArray<TWeakObjectPtr<UIntrinsicSceneCaptureComponent2D>> Cameras;

	/** Index into Cameras. Kept in range as the list changes. */
	int32 SelectedCamera = 0;

	/** False shows colour, true shows depth. */
	bool bShowDepthChannel = false;

	/**
	 * Depth is colormapped through a material, and which channel it lives in
	 * depends on the capture mode: the DMV pass puts it in red, while
	 * SingleCaptureColorDepth packs it into the colour target's alpha. Same
	 * material, one connection apart, so the panel keeps an instance of each and
	 * picks by which target it was handed.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DepthFromRedMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DepthFromAlphaMID;

	/** Build the two depth materials if they are not built yet. */
	void EnsureDepthMaterials();

	/** Start collapsed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Capture")
	bool bStartCollapsed = false;

	bool  bCollapsed = false;
	float TimeSinceRefresh = 0.0f;
};
