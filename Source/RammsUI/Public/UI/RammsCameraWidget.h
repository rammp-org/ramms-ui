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
#include "Components/HorizontalBox.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"

#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/Layout/Anchors.h"
#include "RammsDetectionTypes.h"
#include "RammsCameraWidget.generated.h"

class UCanvasPanelSlot;
class URammsBoundingBoxOverlay;

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
	Corner,

	/** Widget mode — size determined by parent layout, no viewport % sizing */
	Widget
};

/**
 * Which data channel to visualize
 */
UENUM(BlueprintType)
enum class ERammsCameraViewMode : uint8
{
	/** Show RGB color image */
	RGB UMETA(DisplayName = "RGB Color"),

	/** Show data stream through visualization material */
	Data UMETA(DisplayName = "Data (Visualized)"),

	/** Side-by-side RGB + Data */
	SideBySide UMETA(DisplayName = "Side-by-Side"),

	/** RGB with data overlay (alpha blended) */
	Overlay UMETA(DisplayName = "RGB + Data Overlay")
};

/**
 * A named option for a data stream visualization dropdown.
 * Maps a user-facing display name to a material scalar parameter value
 * (e.g. "Viridis" → 0.0, "Magma" → 1.0, "Turbo" → 2.0 for a ColormapIndex param).
 */
USTRUCT(BlueprintType)
struct FRammsDataStreamOption
{
	GENERATED_BODY()

	/** Display name shown in the cycling button (e.g. "Viridis", "Magma") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Stream")
	FText DisplayName;

	/** Scalar value sent to the material parameter identified by OptionParamName */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Stream")
	float Value = 0.0f;
};

/**
 * Configuration for a data stream visualization material.
 * Bundles the material(s) and their parameter names so the camera widget
 * can visualize any data stream (depth, mask, flow, etc.) without
 * hardcoding parameter names or material-specific logic.
 */
USTRUCT(BlueprintType)
struct FRammsDataStreamMaterialConfig
{
	GENERATED_BODY()

	/** Material for data-only view (e.g. depth colormap, mask colors, flow arrows) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Stream")
	TObjectPtr<UMaterialInterface> VisualizationMaterial;

	/** Material for RGB+data overlay blend */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Stream")
	TObjectPtr<UMaterialInterface> OverlayMaterial;

	/** Parameter name for the data texture input in both materials */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Stream")
	FName DataTextureParam = "DataTexture";

	/** Parameter name for the RGB texture in the overlay material */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Stream")
	FName RGBTextureParam = "RGBTexture";

	/** Parameter name for blend alpha in the overlay material */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Stream")
	FName BlendAlphaParam = "BlendAlpha";

	/** Additional scalar parameters passed to both materials (e.g. DepthMin, ColormapIndex) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Stream")
	TMap<FName, float> ScalarParams;

	/** Label shown on the view mode button (e.g. "Depth", "Mask", "Flow") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Stream")
	FText DisplayLabel = FText::FromString(TEXT("Data"));

	/**
	 * Material parameter name driven by the option selector (e.g. "ColormapIndex").
	 * Only used when Options is non-empty.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Stream|Options")
	FName OptionParamName;

	/** Named options for the cycling dropdown (e.g. colormap names). If empty, no option button is shown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Stream|Options")
	TArray<FRammsDataStreamOption> Options;

	/** Whether this config has any materials assigned */
	bool IsValid() const { return VisualizationMaterial != nullptr || OverlayMaterial != nullptr; }
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

	/** Data stream ID (e.g., "camera/wrist/depth", "camera/wrist/mask") — used in Data/SideBySide/Overlay modes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FString DataStreamID;

	/** Which channel(s) to display */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	ERammsCameraViewMode ViewMode = ERammsCameraViewMode::RGB;

	/** Data stream visualization config (materials, param names, scalar params) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FRammsDataStreamMaterialConfig DataStreamConfig;

	/** Overlay blend alpha (0 = RGB only, 1 = data only) — used in Overlay view mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OverlayBlendAlpha = 0.4f;

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

	/** Which display modes are available for cycling (default: all) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	TArray<ERammsCameraDisplayMode> AllowedDisplayModes = {
		ERammsCameraDisplayMode::Fullscreen,
		ERammsCameraDisplayMode::Windowed,
		ERammsCameraDisplayMode::Corner,
		ERammsCameraDisplayMode::Widget
	};

	/** Show the display mode cycle button in the header */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	bool bShowDisplayModeCycleButton = true;

	/** Whether to show the label */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	bool bShowLabel = true;

	/** Custom label (uses StreamID if empty) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FText CustomLabel;

	/** Border thickness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	float BorderThickness = 2.0f;

	/**
	 * Base passthrough material for displaying camera textures.
	 * Must be a User Interface / Translucent material with parameters:
	 *   - Texture2D "Texture"     (the camera feed)
	 *   - Scalar    "CornerRadius" (per-corner radius in pixels, uniform)
	 *   - Vector    "ImageSize"   (widget pixel dimensions, float2)
	 * The material should use RammsRoundedCorners.ush for corner masking.
	 * If not set, textures are displayed directly (no corner masking, may break with post-processing).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	TObjectPtr<UMaterialInterface> PassthroughMaterial;

	/** Base Z-order for this widget (higher values draw on top of lower ones) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	int32 BaseZOrder = 0;

	/** Z-order boost added when in Fullscreen mode (so fullscreen covers other widgets) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	int32 FullscreenZOrderBoost = 100;

	/** Bring widget to front when clicked in Windowed mode (focus-on-click) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	bool bFocusOnClick = true;

	/** Gap between stacked corner widgets (pixels) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display", meta = (ClampMin = "0.0"))
	float CornerStackGap = 8.0f;

	/** Enable drag-to-move */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bEnableDrag = true;

	/** Enable collapse toggle on title bar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bCollapsible = false;

	/** Automatically show bounding box overlay on construct */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	bool bShowDetectionOverlay = false;

	/**
	 * Source tag to subscribe to for subsystem detection broadcasts.
	 * If empty, defaults to the camera StreamID (e.g. "camera/wrist/color").
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection", meta = (EditCondition = "bShowDetectionOverlay"))
	FName DetectionSourceTag;

	/**
	 * Auto-clear detections after this many seconds of no new data. 0 = never auto-clear.
	 * Forwarded to the bounding box overlay's DetectionLifetime.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection",
		meta = (EditCondition = "bShowDetectionOverlay", ClampMin = "0.0", ClampMax = "60.0"))
	float DetectionLifetime = 0.0f;

	/** Constrain image area to maintain aspect ratio (prevents stretching) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bMaintainAspectRatio = true;

	/** Target aspect ratio (width / height). Ignored when bAutoDetectAspectRatio is true and a texture has been received. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (EditCondition = "bMaintainAspectRatio", ClampMin = "0.1", ClampMax = "10.0"))
	float AspectRatio = 16.0f / 9.0f;

	/** Automatically update AspectRatio from the first received texture dimensions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (EditCondition = "bMaintainAspectRatio"))
	bool bAutoDetectAspectRatio = true;

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

	// Widget references (Transient — rebuilt programmatically)
	UPROPERTY(Transient)
	TObjectPtr<UBorder> CameraBorder;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> CameraRootOverlay;

	/** Overlay wrapping the image area — bbox overlay is hosted here (not over the header) */
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> ImageContainerOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UImage> CameraImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CameraLabel;

	/** Wrapped version of camera label shown below buttons when header is narrow */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CameraLabelWrap;

	/** HBox containing header buttons (used to measure minimum button width) */
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ButtonRow;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> TitleBar;

	/** Collapse toggle button in the title bar */
	UPROPERTY(Transient)
	TObjectPtr<UButton> CollapseButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CollapseIcon;

	/** SizeBox wrapping camera content for collapse animation */
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> CameraSizeBox;

	/** SizeBox wrapping the entire widget tree — used to constrain size when placed
	 *  in a non-Canvas container (NamedSlot, Overlay, SizeBox). */
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> InternalSizeBox;

	/** SizeBox constraining the image area to maintain aspect ratio (created when bMaintainAspectRatio is true) */
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> ImageAspectRatioBox;

	/** Current texture being displayed (UTexture2D or UTextureRenderTarget2D) */
	UPROPERTY(Transient)
	TObjectPtr<UTexture> CurrentTexture;

	/** Current data texture (e.g. depth, mask, flow — UTexture2D or UTextureRenderTarget2D) */
	UPROPERTY(Transient)
	TObjectPtr<UTexture> CurrentDataTexture;

	/** Second image widget for side-by-side data display */
	UPROPERTY(Transient)
	TObjectPtr<UImage> DataImage;

	/** Bounding box overlay (created on demand) */
	UPROPERTY(Transient)
	TObjectPtr<URammsBoundingBoxOverlay> BBoxOverlay;

	/** Whether the bbox overlay is intentionally enabled (tracks Show/Hide state) */
	bool bBBoxOverlayEnabled = false;

	/** View-mode toggle button in title bar */
	UPROPERTY(Transient)
	TObjectPtr<UButton> ViewModeButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ViewModeLabel;

	/** Data stream option cycling button (shown only when DataStreamConfig.Options is non-empty) */
	UPROPERTY(Transient)
	TObjectPtr<UButton> OptionButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OptionLabel;

	/** Display mode cycle button */
	UPROPERTY(Transient)
	TObjectPtr<UButton> DisplayModeCycleButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DisplayModeCycleLabel;

	/** Current index into DataStreamConfig.Options */
	int32 CurrentOptionIndex = 0;

	/** Dynamic material instance for data visualization */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DataMID;

	/** Dynamic material instance for overlay blend */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OverlayMID;

	/** Dynamic material instance for RGB passthrough on CameraImage (with rounded corner masking) */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PassthroughMID_RGB;

	/** Dynamic material instance for RGB passthrough on DataImage in SideBySide mode */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PassthroughMID_Data;

	/** Render target for data material output (enables RoundedBox corners) */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> DataRT;

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

	/** Cached depth format from the data stream provider (auto-detected on first frame) */
	ERammsDepthFormat CachedDataDepthFormat = ERammsDepthFormat::Unknown;

	/** One-shot flag: true once we've logged overlay material diagnostics */
	bool bOverlayDiagLogged = false;

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
	 * Cycle to the next allowed display mode
	 */
	UFUNCTION(BlueprintCallable, Category = "Display")
	void CycleDisplayMode();

	/**
	 * Get current display mode
	 */
	UFUNCTION(BlueprintPure, Category = "Display")
	ERammsCameraDisplayMode GetDisplayMode() const { return DisplayMode; }

	/**
	 * Set view mode (RGB, Data, Side-by-Side, Overlay)
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetViewMode(ERammsCameraViewMode NewViewMode);

	/**
	 * Get current view mode
	 */
	UFUNCTION(BlueprintPure, Category = "Camera")
	ERammsCameraViewMode GetViewMode() const { return ViewMode; }

	/**
	 * Set the current data stream option by index (cycles the dropdown).
	 * Only effective when DataStreamConfig.Options is non-empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetOptionIndex(int32 Index);

	/**
	 * Get current option index
	 */
	UFUNCTION(BlueprintPure, Category = "Camera")
	int32 GetOptionIndex() const { return CurrentOptionIndex; }

	/**
	 * Set the data stream visualization config (materials, param names, scalars).
	 * Recreates dynamic material instances. Existing data texture is preserved.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetDataStreamConfig(const FRammsDataStreamMaterialConfig& Config);

	/**
	 * Update a single scalar parameter on both data and overlay materials.
	 * Useful for tweaking values without swapping the full config.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetMaterialScalarParam(FName ParamName, float Value);

	/**
	 * Set overlay blend alpha (0 = RGB only, 1 = data only)
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetOverlayBlendAlpha(float Alpha);

	/**
	 * Set data stream ID
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetDataStreamID(const FString& NewDataStreamID);

	/**
	 * Set both RGB and data stream IDs at once (convenience)
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetStreams(const FString& RGBStreamID, const FString& NewDataStreamID);

	/**
	 * Set both stream IDs and the data visualization config in one call.
	 * This is the recommended way to fully configure a camera widget.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ConfigureStreams(const FString& RGBStreamID, const FString& NewDataStreamID,
		const FRammsDataStreamMaterialConfig& Config);

	/**
	 * Manually set camera texture (for testing or manual control)
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetTexture(UTexture* Texture);

	/**
	 * Manually set data texture (for testing or manual control)
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetDataTexture(UTexture* Texture);

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

	/**
	 * Set whether the image area maintains aspect ratio
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void SetMaintainAspectRatio(bool bMaintain);

	/**
	 * Set the target aspect ratio (width / height). Disables auto-detect.
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void SetAspectRatio(float NewAspectRatio);

	/**
	 * Set the base z-order and immediately re-apply.
	 */
	UFUNCTION(BlueprintCallable, Category = "Display")
	void SetZOrder(int32 NewBaseZOrder);

	/**
	 * Get the effective z-order (BaseZOrder + display-mode boost).
	 */
	UFUNCTION(BlueprintPure, Category = "Display")
	int32 GetEffectiveZOrder() const;

	// ── Bounding Box Overlay ────────────────────────────────────

	/**
	 * Show bounding box overlay. Creates the overlay widget if needed.
	 * @param SourceTag - If set, auto-subscribes to subsystem detections from this source
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera|Detection")
	void ShowBoundingBoxOverlay(FName SourceTag = NAME_None);

	/** Hide the bounding box overlay and clear its detections (widget remains cached for re-show) */
	UFUNCTION(BlueprintCallable, Category = "Camera|Detection")
	void HideBoundingBoxOverlay();

	/** Set detections directly on the overlay (creates overlay if needed) */
	UFUNCTION(BlueprintCallable, Category = "Camera|Detection")
	void SetDetections(const TArray<FRammsBoundingBox>& InBoxes);

	/** Clear all detections from the overlay */
	UFUNCTION(BlueprintCallable, Category = "Camera|Detection")
	void ClearDetections();

	/** Whether the bounding box overlay is currently visible */
	UFUNCTION(BlueprintPure, Category = "Camera|Detection")
	bool IsBoundingBoxOverlayVisible() const;

	/** Get the bounding box overlay widget (may be null if not shown) */
	UFUNCTION(BlueprintPure, Category = "Camera|Detection")
	URammsBoundingBoxOverlay* GetBoundingBoxOverlay() const { return BBoxOverlay; }

protected:
	// Mouse/Touch interaction overrides
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

protected:
	/** Build the widget tree programmatically */
	virtual void	 ResetCachedWidgets() override;
	virtual void	 BuildWidgetTree() override;
	virtual UWidget* GetRootWidgetForValidation() override { return InternalSizeBox ? (UWidget*)InternalSizeBox : (UWidget*)CameraBorder; }

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

	/** Create or update dynamic material instances for data stream visualization */
	void EnsureDataMaterials();

	/** Update data material parameters (texture, scalar params from config) */
	void UpdateDataMaterialParams();

	/** Update displayed images based on current textures and view mode */
	void UpdateDisplayedImages();

	/** Cycle view mode (called from button) */
	UFUNCTION()
	void OnViewModeClicked();

	/** Cycle data stream option (called from option button) */
	UFUNCTION()
	void OnOptionClicked();

	/** Update option button visibility and label based on current config */
	void UpdateOptionButton();

	/** Collapse toggle callback */
	UFUNCTION()
	void OnCollapseClicked();

	/** Display mode cycle callback */
	UFUNCTION()
	void OnDisplayModeCycleClicked();

	/** Get abbreviated label for a display mode */
	static FString GetDisplayModeShortLabel(ERammsCameraDisplayMode Mode);

	/** Update display mode cycle button visibility and label */
	void UpdateDisplayModeCycleButton();

	/** Apply z-order based on current display mode */
	void ApplyZOrder();

	/** Compute absolute top-left position of widget within its canvas panel */
	FVector2D ComputeSlotAbsoluteTopLeft(UCanvasPanelSlot* CanvasSlot, const FVector2D& CanvasSize) const;

	/** Compute absolute size of widget within its canvas panel */
	FVector2D ComputeSlotAbsoluteSize(UCanvasPanelSlot* CanvasSlot, const FVector2D& CanvasSize) const;

	/** Get canvas-space size (viewport / DPI scale) */
	FVector2D GetCanvasSize() const;

	/** Register this widget in the corner stacking registry */
	void RegisterCorner();

	/** Unregister this widget from the corner stacking registry */
	void UnregisterCorner();

	/** Get this widget's index in its corner stack (0 = closest to edge) */
	int32 GetCornerStackIndex() const;

	/** Make a key for the corner registry from alignment enums */
	static uint8 MakeCornerKey(EHorizontalAlignment H, EVerticalAlignment V);

	/** Check header width and toggle single-line vs two-row layout */
	void UpdateHeaderLayout();

	/** Update collapse icon text */
	void UpdateCollapseIcon();

	/** Set image brush from texture (via passthrough material if available, direct otherwise) */
	void SetImageBrushFromTexture(UImage* Image, UTexture* Texture);

	/** Set image brush from a material, passing corner/size params for rounded masking */
	void SetImageBrushFromMaterial(UImage* Image, UMaterialInstanceDynamic* MID,
		UTexture* SizeSource, TObjectPtr<UTextureRenderTarget2D>& RenderTarget);

	/** Update CornerRadius and ImageSize parameters on a MID for rounded masking */
	void UpdateMaterialCornerParams(UMaterialInstanceDynamic* MID, UImage* Image);

	/** Set CameraSizeBox slot to Fill or Auto */
	void SetCameraSizeBoxSlotFill(bool bFill);

	/** Cached expanded Canvas Panel slot size */
	FVector2D CachedExpandedSlotSize = FVector2D::ZeroVector;

	/** Cached content area height for collapse animation */
	float CachedContentHeight = 0.0f;

	/** Cached per-corner radii for material-based corner masking */
	FVector4 CachedRGBCornerRadii = FVector4(4.0f, 4.0f, 4.0f, 4.0f);
	FVector4 CachedDataCornerRadii = FVector4(4.0f, 4.0f, 4.0f, 4.0f);

	/** Collapse animation state */
	float CollapseProgress = 1.0f; // 1 = expanded, 0 = collapsed
	float CollapseTarget = 1.0f;
	bool  bCollapseAnimating = false;

	/** Display mode transition animation state */
	bool	  bDisplayModeTransitioning = false;
	float	  DisplayModeTransitionProgress = 0.0f;
	FVector2D TransitionStartPos = FVector2D::ZeroVector;
	FVector2D TransitionStartSize = FVector2D::ZeroVector;
	FVector2D TransitionTargetPos = FVector2D::ZeroVector;
	FVector2D TransitionTargetSize = FVector2D::ZeroVector;

	/** Target canvas slot properties to restore after transition animation */
	FAnchors  TransitionTargetAnchors;
	FVector2D TransitionTargetAlignment = FVector2D::ZeroVector;
	FVector2D TransitionTargetSlotPos = FVector2D::ZeroVector;
	FVector2D TransitionTargetSlotSize = FVector2D::ZeroVector;

	/** Whether the header is currently in narrow (two-row) mode */
	bool bHeaderNarrowMode = false;

	/** Cached title bar width for UpdateHeaderLayout — skip re-measurement when unchanged */
	float CachedHeaderCheckWidth = -1.0f;

	/** Cached MID pointers for material corner param change detection */
	TWeakObjectPtr<UMaterialInstanceDynamic> LastCornerMID_RGB;
	TWeakObjectPtr<UMaterialInstanceDynamic> LastCornerMID_Data;

	/** Cached image size for material corner param change detection */
	FVector2D LastMaterialImageSize_RGB = FVector2D::ZeroVector;
	FVector2D LastMaterialImageSize_Data = FVector2D::ZeroVector;

	/** Last applied z-order (to avoid redundant viewport re-adds) */
	int32 CachedAppliedZOrder = INT32_MIN;

	/** Per-instance focus z-order boost (incremented on click for focus ordering) */
	int32 FocusZOrderBoost = 0;

	/** Composite key this widget is registered under in the corner registry */
	TPair<UWidget*, uint8> RegisteredCornerKey = { nullptr, 0xFF };

	/** Global counter for focus-on-click ordering across all camera widget instances */
	static int32 FocusZOrderCounter;

	/** Corner stacking registry: (parent, corner alignment) → ordered list of widgets.
	 *  Scoped per parent so widgets in different layouts don't interfere. */
	static TMap<TPair<UWidget*, uint8>, TArray<TWeakObjectPtr<URammsCameraWidget>>> CornerRegistry;
};
