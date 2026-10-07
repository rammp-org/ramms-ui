// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "RammsCameraCapturePanel.generated.h"

class UBorder;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;
class URammsButton;
class URammsCameraWidget;
class UCameraCaptureSubsystem;
class UIntrinsicSceneCaptureComponent2D;

/**
 * Live view of everything the camera capture subsystem has registered, with the
 * controls that decide what it does.
 *
 * One feed per camera, taken from the camera's own render target rather than
 * from FCaptureData: the target is already on the GPU, so displaying it costs a
 * brush assignment instead of a readback. That also means the feeds show what
 * the cameras are rendering even while serialization is off, which is the state
 * you want when framing a shot.
 *
 * Depth goes in the camera widget's data-texture slot when a separate depth
 * target exists. In SingleCaptureColorDepth mode there is no second target --
 * depth rides in the colour target's alpha -- so the slot stays empty and the
 * panel says so rather than leaving a blank square unexplained.
 *
 * The panel does not own the cameras and never creates one. It reflects the
 * subsystem, and rebuilds its feed list when the registered set changes.
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

	/** Height of each camera feed, in pixels. Width follows the feed's aspect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Capture", meta = (ClampMin = "48.0"))
	float FeedHeight = 160.0f;

	/** How often the labels and statistics refresh. The feeds themselves are
	 *  render targets and update with the renderer, not with this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Capture", meta = (ClampMin = "0.05"))
	float RefreshInterval = 0.25f;

	/** Steps the stepper moves through. Capturing every frame is first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Capture")
	TArray<int32> CaptureRateSteps = { 1, 2, 5, 10, 30, 60 };

	// ── Actions, also callable from Blueprint or a key binding ──────────

	UFUNCTION(BlueprintCallable, Category = "Camera Capture")
	void ToggleCapture();

	UFUNCTION(BlueprintCallable, Category = "Camera Capture")
	void ToggleSerialization();

	/** Move to the next/previous entry in CaptureRateSteps. */
	UFUNCTION(BlueprintCallable, Category = "Camera Capture")
	void StepCaptureRate(int32 Delta);

	/** Discard the feed widgets and rebuild them from the subsystem. */
	UFUNCTION(BlueprintCallable, Category = "Camera Capture")
	void RebuildFeeds();

protected:
	virtual void	 NativeConstruct() override;
	virtual void	 NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void	 ApplyStyle_Implementation() override;
	virtual void	 BuildWidgetTree() override;
	virtual void	 ResetCachedWidgets() override;
	virtual UWidget* GetRootWidgetForValidation() override;

	/** The subsystem on this world, or null outside play. */
	UCameraCaptureSubsystem* GetSubsystem() const;

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

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> MainVBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ControlsHBox;

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

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> FeedsScroll;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> FeedsVBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URammsCameraWidget>> Feeds;

	/** Cameras the current feed widgets were built for, so a changed set can be
	 *  detected without rebuilding every tick. */
	TArray<TWeakObjectPtr<UIntrinsicSceneCaptureComponent2D>> FeedCameras;

	float TimeSinceRefresh = 0.0f;
};
