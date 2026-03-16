// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Interfaces/IRammsCameraProvider.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"

#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RammsCameraWidget.generated.h"

/**
 * Display modes for camera widget
 */
UENUM(BlueprintType)
enum class ERammsCameraDisplayMode : uint8
{
	/** Full screen (fills entire viewport) */
	Fullscreen,

	/** Large window (centered, takes most of screen) */
	Windowed,

	/** Corner widget (small, anchored to corner) */
	Corner
};

/**
 * Which data channel to visualize
 */
UENUM(BlueprintType)
enum class ERammsCameraViewMode : uint8
{
	/** Show RGB color image */
	RGB UMETA(DisplayName = "RGB Color"),

	/** Show depth image with colormap */
	Depth UMETA(DisplayName = "Depth (Colorized)"),

	/** Side-by-side RGB + Depth */
	SideBySide UMETA(DisplayName = "Side-by-Side"),

	/** RGB with depth overlay (alpha blended) */
	Overlay UMETA(DisplayName = "RGB + Depth Overlay")
};

/**
 * Colormap for depth visualization
 */
UENUM(BlueprintType)
enum class ERammsDepthColormap : uint8
{
	Grayscale UMETA(DisplayName = "Grayscale"),
	Jet		  UMETA(DisplayName = "Jet"),
	Turbo	  UMETA(DisplayName = "Turbo"),
	Inferno	  UMETA(DisplayName = "Inferno")
};

/**
 * Camera feed widget with multiple display modes
 * Integrates with IRammsCameraProvider for transport-agnostic camera streams
 */
UCLASS(meta = (DisplayName = "Ramms Camera Widget"))
class RAMMSUI_API URammsCameraWidget : public URammsBaseWidget
{
	GENERATED_BODY()

protected:
	/** Camera provider (optional - can be set at runtime via SetCameraProvider or auto-found) */
	UPROPERTY(BlueprintReadWrite, Category = "Camera")
	TScriptInterface<IRammsCameraProvider> CameraProvider;

	/** Automatically find and use the first ARammsCameraProviderBase in the level if no provider is set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bAutoFindProvider = true;

	/** Stream ID to display (e.g., "camera/wrist/color") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FString StreamID;

	/** Depth stream ID (e.g., "camera/wrist/depth") — only used in Depth/SideBySide/Overlay modes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FString DepthStreamID;

	/** Which channel(s) to display */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	ERammsCameraViewMode ViewMode = ERammsCameraViewMode::RGB;

	/** Colormap for depth visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	ERammsDepthColormap DepthColormap = ERammsDepthColormap::Turbo;

	/** Depth range in meters (min, max) — values outside this range are clipped */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FVector2D DepthRange = FVector2D(0.1f, 10.0f);

	/** Overlay blend alpha (0 = RGB only, 1 = depth only) — used in Overlay view mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DepthOverlayAlpha = 0.4f;

	/** Display mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	ERammsCameraDisplayMode DisplayMode = ERammsCameraDisplayMode::Corner;

	/** Which corner to anchor to in Corner mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	TEnumAsByte<EHorizontalAlignment> CornerHAlign = HAlign_Right;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	TEnumAsByte<EVerticalAlignment> CornerVAlign = VAlign_Top;

	/** Padding from the screen edge in DPI-scaled pixels (Corner mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display", meta = (ClampMin = "0.0"))
	FVector2D CornerPadding = FVector2D(16.0f, 16.0f);

	/** Corner size when in Corner mode (screen percentage) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display", meta = (ClampMin = "0.1", ClampMax = "0.5"))
	FVector2D CornerSize = FVector2D(0.25f, 0.25f);

	/** Windowed size (screen percentage) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display", meta = (ClampMin = "0.3", ClampMax = "1.0"))
	FVector2D WindowedSize = FVector2D(0.8f, 0.8f);

	/** Whether to show the label */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	bool bShowLabel = true;

	/** Custom label (uses StreamID if empty) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FText CustomLabel;

	/** Border thickness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	float BorderThickness = 2.0f;

	/** Enable drag-to-move */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bEnableDrag = true;

	/** Enable collapse toggle on title bar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bCollapsible = false;

	/** Is currently collapsed (only relevant when bCollapsible is true) */
	UPROPERTY(Transient)
	bool bCameraCollapsed = false;

	/** Is currently being dragged */
	UPROPERTY(Transient)
	bool bIsDragging = false;

	/** Drag start position (screen space) */
	FVector2D DragStartMousePos;

	/** Widget position at drag start */
	FVector2D DragStartWidgetPos;

	/** Current widget position in screen pixels (for drag tracking and viewport positioning) */
	FVector2D WidgetPosition = FVector2D::ZeroVector;

	// Widget references (built programmatically)
	UPROPERTY()
	TObjectPtr<UBorder> CameraBorder;

	UPROPERTY()
	TObjectPtr<UImage> CameraImage;

	UPROPERTY()
	TObjectPtr<UTextBlock> CameraLabel;

	UPROPERTY()
	TObjectPtr<UBorder> TitleBar;

	/** Collapse toggle button in the title bar */
	UPROPERTY()
	TObjectPtr<UButton> CollapseButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> CollapseIcon;

	/** SizeBox wrapping camera content for collapse animation */
	UPROPERTY()
	TObjectPtr<USizeBox> CameraSizeBox;

	/** Current texture being displayed (UTexture2D or UTextureRenderTarget2D) */
	UPROPERTY(Transient)
	TObjectPtr<UTexture> CurrentTexture;

	/** Current depth texture (UTexture2D or UTextureRenderTarget2D) */
	UPROPERTY(Transient)
	TObjectPtr<UTexture> CurrentDepthTexture;

	/** Second image widget for side-by-side depth display */
	UPROPERTY()
	TObjectPtr<UImage> DepthImage;

	/** View-mode toggle button in title bar */
	UPROPERTY()
	TObjectPtr<UButton> ViewModeButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> ViewModeLabel;

	/** Optional material for depth colorization (set via DepthColormapMaterial property) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera",
		meta = (ToolTip = "Material with 'DepthTexture', 'DepthMin', 'DepthMax', 'ColormapIndex' parameters"))
	TObjectPtr<UMaterialInterface> DepthColormapMaterial;

	/** Optional material for overlay blending (set via OverlayBlendMaterial property) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera",
		meta = (ToolTip = "Material with 'RGBTexture', 'DepthTexture', 'BlendAlpha', 'DepthMin', 'DepthMax', 'ColormapIndex' parameters"))
	TObjectPtr<UMaterialInterface> OverlayBlendMaterial;

	/** Dynamic material instance for depth image */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DepthMID;

	/** Dynamic material instance for overlay blend */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OverlayMID;

	/** Render target for depth material output (enables RoundedBox corners) */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> DepthRT;

	/** Render target for overlay material output (enables RoundedBox corners) */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> OverlayRT;

	/** Tracked subscriptions to all discovered providers' frame delegates */
	struct FProviderSubscription
	{
		TWeakObjectPtr<UObject> Object;
		IRammsCameraProvider*	Interface = nullptr;
		FDelegateHandle			Handle;
	};
	TArray<FProviderSubscription> ProviderSubscriptions;

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void SynchronizeProperties() override;

	/** Apply style to camera widget */
	virtual void ApplyStyle_Implementation() override;

	/**
	 * Set the camera provider
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetCameraProvider(TScriptInterface<IRammsCameraProvider> Provider);

	/**
	 * Set the stream to display
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetStreamID(const FString& NewStreamID);

	/**
	 * Set display mode with optional animation
	 */
	UFUNCTION(BlueprintCallable, Category = "Display")
	void SetDisplayMode(ERammsCameraDisplayMode NewMode, bool bAnimateTransition = true);

	/**
	 * Get current display mode
	 */
	UFUNCTION(BlueprintPure, Category = "Display")
	ERammsCameraDisplayMode GetDisplayMode() const { return DisplayMode; }

	/**
	 * Set view mode (RGB, Depth, Side-by-Side, Overlay)
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetViewMode(ERammsCameraViewMode NewViewMode);

	/**
	 * Get current view mode
	 */
	UFUNCTION(BlueprintPure, Category = "Camera")
	ERammsCameraViewMode GetViewMode() const { return ViewMode; }

	/**
	 * Set depth colormap
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetDepthColormap(ERammsDepthColormap NewColormap);

	/**
	 * Set depth range (min, max) in meters
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetDepthRange(float MinDepth, float MaxDepth);

	/**
	 * Set depth stream ID
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetDepthStreamID(const FString& NewDepthStreamID);

	/**
	 * Manually set camera texture (for testing or manual control)
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetTexture(UTexture* Texture);

	/**
	 * Manually set depth texture (for testing or manual control)
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetDepthTexture(UTexture* Texture);

	/**
	 * Enable or disable drag-to-move
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetDragEnabled(bool bEnabled) { bEnableDrag = bEnabled; }

	/**
	 * Is drag enabled?
	 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsDragEnabled() const { return bEnableDrag; }

	/**
	 * Toggle collapse state (only if bCollapsible)
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ToggleCollapse();

	/**
	 * Set collapsed state
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetCameraCollapsed(bool bCollapsed);

	/**
	 * Is currently collapsed?
	 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsCameraCollapsed() const { return bCameraCollapsed; }

protected:
	// Mouse/Touch interaction overrides
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

protected:
	/** Build the widget tree programmatically */
	virtual void ResetCachedWidgets() override;
	virtual void BuildWidgetTree() override;

	/** Update layout based on current display mode */
	void UpdateLayout(bool bAnimate);

	/** Callback when new camera frame is ready */
	void OnCameraFrameReady(const FString& InStreamID, UTexture* Texture, int64 Timestamp);

	/** Start receiving camera stream */
	void StartStream();

	/** Stop receiving camera stream */
	void StopStream();

	/** Apply the current view mode layout (updates which images are visible) */
	void ApplyViewModeLayout();

	/** Update image corner radii based on layout mode (collapsible vs overlay) and view mode (single vs SbS) */
	void UpdateImageCornerRadii();

	/** Update header corner radii based on collapse state (all corners when collapsed, top only when expanded) */
	void UpdateHeaderCornerRadii();

	/** Create or update dynamic material instances for depth/overlay */
	void EnsureDepthMaterials();

	/** Update depth material parameters (texture, range, colormap) */
	void UpdateDepthMaterialParams();

	/** Update displayed images based on current textures and view mode */
	void UpdateDisplayedImages();

	/** Cycle view mode (called from button) */
	UFUNCTION()
	void OnViewModeClicked();

	/** Collapse toggle callback */
	UFUNCTION()
	void OnCollapseClicked();

	/** Update collapse icon text */
	void UpdateCollapseIcon();

	/** Set image brush from texture, preserving RoundedBox corner settings */
	void SetImageBrushFromTexture(UImage* Image, UTexture* Texture);

	/** Render a material to a render target and display it with RoundedBox support */
	void SetImageBrushFromMaterial(UImage* Image, UMaterialInstanceDynamic* MID,
		UTexture* SizeSource, TObjectPtr<UTextureRenderTarget2D>& RenderTarget);

	/** Set CameraSizeBox slot to Fill or Auto */
	void SetCameraSizeBoxSlotFill(bool bFill);

	/** Cached expanded Canvas Panel slot size */
	FVector2D CachedExpandedSlotSize = FVector2D::ZeroVector;

	/** Cached content area height for collapse animation */
	float CachedContentHeight = 0.0f;

	/** Collapse animation state */
	float CollapseProgress = 1.0f; // 1 = expanded, 0 = collapsed
	float CollapseTarget = 1.0f;
	bool  bCollapseAnimating = false;
};
