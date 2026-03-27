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

	/** Enable drag-to-move */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bEnableDrag = true;

	/** Enable collapse toggle on title bar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bCollapsible = false;

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

	/** SizeBox wrapping the entire widget tree — used to constrain size when placed
	 *  in a non-Canvas container (NamedSlot, Overlay, SizeBox). */
	UPROPERTY()
	TObjectPtr<USizeBox> InternalSizeBox;

	/** SizeBox constraining the image area to maintain aspect ratio (created when bMaintainAspectRatio is true) */
	UPROPERTY()
	TObjectPtr<USizeBox> ImageAspectRatioBox;

	/** Current texture being displayed (UTexture2D or UTextureRenderTarget2D) */
	UPROPERTY(Transient)
	TObjectPtr<UTexture> CurrentTexture;

	/** Current data texture (e.g. depth, mask, flow — UTexture2D or UTextureRenderTarget2D) */
	UPROPERTY(Transient)
	TObjectPtr<UTexture> CurrentDataTexture;

	/** Second image widget for side-by-side data display */
	UPROPERTY()
	TObjectPtr<UImage> DataImage;

	/** View-mode toggle button in title bar */
	UPROPERTY()
	TObjectPtr<UButton> ViewModeButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> ViewModeLabel;

	/** Data stream option cycling button (shown only when DataStreamConfig.Options is non-empty) */
	UPROPERTY()
	TObjectPtr<UButton> OptionButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> OptionLabel;

	/** Current index into DataStreamConfig.Options */
	int32 CurrentOptionIndex = 0;

	/** Dynamic material instance for data visualization */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DataMID;

	/** Dynamic material instance for overlay blend */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OverlayMID;

	/** Dynamic material instance for RGB passthrough (with rounded corner masking) */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PassthroughMID;

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
};
