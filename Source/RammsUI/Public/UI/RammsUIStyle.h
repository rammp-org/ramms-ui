// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "RammsUIStyle.generated.h"

/**
 * Animation easing types for UI transitions
 */
UENUM(BlueprintType)
enum class ERammsUIEasing : uint8
{
	Linear,
	EaseIn,
	EaseOut,
	EaseInOut,
	Bounce,
	Elastic
};

/**
 * Animation curve settings
 */
USTRUCT(BlueprintType)
struct FRammsAnimationCurve
{
	GENERATED_BODY()

	/** Animation duration in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.0"))
	float Duration = 0.3f;

	/** Easing function */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	ERammsUIEasing Easing = ERammsUIEasing::EaseInOut;

	/** Optional delay before animation starts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.0"))
	float Delay = 0.0f;
};

/**
 * Color palette for UI theming
 */
USTRUCT(BlueprintType)
struct FRammsColorPalette
{
	GENERATED_BODY()

	/** Primary color (main UI elements, accent) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Primary = FLinearColor(0.0f, 0.478f, 0.8f, 1.0f); // Blue

	/** Secondary color (supporting elements) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Secondary = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f); // Gray

	/** Background color (panels, containers) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Background = FLinearColor(0.02f, 0.02f, 0.02f, 0.95f); // Dark with alpha

	/** Surface color (cards, elevated elements) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Surface = FLinearColor(0.1f, 0.1f, 0.1f, 0.95f);

	/** Text primary (main text, headings) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor TextPrimary = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f); // White

	/** Text secondary (labels, descriptions) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor TextSecondary = FLinearColor(0.7f, 0.7f, 0.7f, 1.0f); // Light gray

	/** Text disabled (inactive elements) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor TextDisabled = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f); // Dark gray

	/** Border/divider color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Border = FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);

	/** Success/positive state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Success = FLinearColor(0.0f, 0.8f, 0.3f, 1.0f); // Green

	/** Warning state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Warning = FLinearColor(1.0f, 0.7f, 0.0f, 1.0f); // Orange

	/** Error/danger state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Error = FLinearColor(0.9f, 0.2f, 0.2f, 1.0f); // Red

	/** Information state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Info = FLinearColor(0.0f, 0.7f, 0.9f, 1.0f); // Cyan
};

/**
 * Typography settings
 */
USTRUCT(BlueprintType)
struct FRammsTypography
{
	GENERATED_BODY()

	/** Large heading font (titles, main headers) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
	FSlateFontInfo HeadingLarge;

	/** Medium heading font (section headers) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
	FSlateFontInfo HeadingMedium;

	/** Small heading font (subsection headers) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
	FSlateFontInfo HeadingSmall;

	/** Body text font (main content) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
	FSlateFontInfo Body;

	/** Caption font (labels, hints) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
	FSlateFontInfo Caption;

	/** Monospace font (values, code) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
	FSlateFontInfo Monospace;

	FRammsTypography()
	{
		// Initialize with default font and sizes
		// Users should set their preferred font in the DataAsset editor
		HeadingLarge.Size = 32;
		HeadingMedium.Size = 24;
		HeadingSmall.Size = 18;
		Body.Size = 14;
		Caption.Size = 12;
		Monospace.Size = 14;
	}
};

/**
 * Spacing values for consistent layout
 */
USTRUCT(BlueprintType)
struct FRammsSpacing
{
	GENERATED_BODY()

	/** Extra small spacing (2px) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spacing", meta = (ClampMin = "0.0"))
	float XSmall = 2.0f;

	/** Small spacing (4px) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spacing", meta = (ClampMin = "0.0"))
	float Small = 4.0f;

	/** Medium spacing (8px) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spacing", meta = (ClampMin = "0.0"))
	float Medium = 8.0f;

	/** Large spacing (16px) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spacing", meta = (ClampMin = "0.0"))
	float Large = 16.0f;

	/** Extra large spacing (24px) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spacing", meta = (ClampMin = "0.0"))
	float XLarge = 24.0f;

	/** Extra extra large spacing (32px) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spacing", meta = (ClampMin = "0.0"))
	float XXLarge = 32.0f;
};

/**
 * Border and corner settings
 */
USTRUCT(BlueprintType)
struct FRammsBorderStyle
{
	GENERATED_BODY()

	/** Default border width */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Border", meta = (ClampMin = "0.0"))
	float BorderWidth = 1.0f;

	/** Thick border width (emphasized elements) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Border", meta = (ClampMin = "0.0"))
	float BorderWidthThick = 2.0f;

	/** Corner radius for small elements */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Border", meta = (ClampMin = "0.0"))
	float CornerRadiusSmall = 2.0f;

	/** Corner radius for medium elements */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Border", meta = (ClampMin = "0.0"))
	float CornerRadiusMedium = 4.0f;

	/** Corner radius for large elements */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Border", meta = (ClampMin = "0.0"))
	float CornerRadiusLarge = 8.0f;
};

/**
 * Scrollbar appearance settings.
 * Defines consistent styling for all scrollbars across the UI.
 * Per-widget overrides are possible by providing non-default values.
 */
USTRUCT(BlueprintType)
struct FRammsScrollBarStyle
{
	GENERATED_BODY()

	/** Scrollbar track + thumb thickness in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScrollBar", meta = (ClampMin = "1.0", ClampMax = "32.0"))
	float Thickness = 6.0f;

	/** Corner radius for the thumb and track rounded box brushes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScrollBar", meta = (ClampMin = "0.0"))
	float CornerRadius = 3.0f;

	/** Thumb color in the normal (idle) state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScrollBar")
	FLinearColor ThumbNormalColor = FLinearColor(0.7f, 0.7f, 0.7f, 0.4f);

	/** Thumb color when hovered */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScrollBar")
	FLinearColor ThumbHoveredColor = FLinearColor(0.7f, 0.7f, 0.7f, 0.7f);

	/** Thumb color when being dragged */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScrollBar")
	FLinearColor ThumbDraggedColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.8f);

	/** Track background color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScrollBar")
	FLinearColor TrackColor = FLinearColor(0.1f, 0.1f, 0.1f, 0.15f);

	/** Padding between the scrollbar and the scroll content edge (Left, Top, Right, Bottom) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScrollBar")
	FMargin Padding = FMargin(0.0f, 2.0f, 2.0f, 2.0f);

	/** Disable the shadow overlay that UE draws at the scroll edges (breaks rounded corners) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScrollBar")
	bool bDisableEdgeShadows = true;

	/** Whether these values have been explicitly customised (false = use style defaults) */
	bool IsDefault() const
	{
		// A quick sentinel: if Thickness is still exactly the struct default, treat as "not customised"
		return FMath::IsNearlyEqual(Thickness, 6.0f)
			&& FMath::IsNearlyEqual(CornerRadius, 3.0f)
			&& ThumbNormalColor.Equals(FLinearColor(0.7f, 0.7f, 0.7f, 0.4f));
	}
};

/**
 * Slider appearance settings.
 * Defines consistent styling for all sliders across the UI.
 * Per-widget overrides take priority when bUseStyleColors is false on the widget.
 */
USTRUCT(BlueprintType)
struct FRammsSliderStyle
{
	GENERATED_BODY()

	/** Thumb (handle) diameter in pixels. Larger = easier to touch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider", meta = (ClampMin = "8.0", ClampMax = "64.0"))
	float ThumbSize = 24.0f;

	/** Track bar thickness in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider", meta = (ClampMin = "1.0", ClampMax = "32.0"))
	float BarThickness = 6.0f;

	/** Track (unfilled portion) color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider")
	FLinearColor TrackColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);

	/** Filled bar (active portion) color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider")
	FLinearColor ActiveBarColor = FLinearColor(0.0f, 0.478f, 0.8f, 1.0f);

	/** Thumb color (normal state) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider")
	FLinearColor ThumbColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	/** Thumb color when hovered (auto-lerped between ThumbColor and ActiveBarColor if default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider")
	FLinearColor ThumbHoveredColor = FLinearColor(-1.0f, 0.0f, 0.0f, 0.0f);

	/** Thumb/bar color when disabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider")
	FLinearColor DisabledColor = FLinearColor(0.3f, 0.3f, 0.3f, 0.3f);

	/** Whether the slider panel background is shown by default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider")
	bool bShowPanelByDefault = true;

	/** Get the effective hovered color (auto-computes if sentinel) */
	FLinearColor GetThumbHoveredColor() const
	{
		if (ThumbHoveredColor.R < 0.0f)
		{
			return FLinearColor::LerpUsingHSV(ThumbColor, ActiveBarColor, 0.3f);
		}
		return ThumbHoveredColor;
	}
};

/**
 * Checkbox / radio button appearance settings
 */
USTRUCT(BlueprintType)
struct FRammsCheckBoxStyle
{
	GENERATED_BODY()

	/** Size of the checkbox box (width & height in pixels) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CheckBox")
	float Size = 20.0f;

	/** Corner radius of the checkbox box */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CheckBox")
	float CornerRadius = 4.0f;

	/** Unchecked background color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CheckBox")
	FLinearColor UncheckedColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);

	/** Checked background color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CheckBox")
	FLinearColor CheckedColor = FLinearColor(0.0f, 0.478f, 0.8f, 1.0f);

	/** Hovered tint (mixed over unchecked/checked color) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CheckBox")
	FLinearColor HoveredTint = FLinearColor(1.0f, 1.0f, 1.0f, 0.12f);

	/** Border color (unchecked state) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CheckBox")
	FLinearColor BorderColor = FLinearColor(0.35f, 0.35f, 0.35f, 1.0f);

	/** Check mark / indicator color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CheckBox")
	FLinearColor CheckMarkColor = FLinearColor::White;

	/** Foreground (check image) color when disabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CheckBox")
	FLinearColor DisabledColor = FLinearColor(0.3f, 0.3f, 0.3f, 0.5f);
};

/**
 * Colour picker appearance settings (gradient bars, swatch, thumb)
 */
USTRUCT(BlueprintType)
struct FRammsColorPickerStyle
{
	GENERATED_BODY()

	/** Height of each gradient bar in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ColorPicker", meta = (ClampMin = "8.0", ClampMax = "64.0"))
	float GradientHeight = 20.0f;

	/** Corner radius for the gradient bars */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ColorPicker", meta = (ClampMin = "0.0", ClampMax = "16.0"))
	float GradientCornerRadius = 4.0f;

	/** Size of the colour swatch square in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ColorPicker", meta = (ClampMin = "12.0", ClampMax = "64.0"))
	float SwatchSize = 28.0f;

	/** Corner radius for the swatch */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ColorPicker", meta = (ClampMin = "0.0", ClampMax = "16.0"))
	float SwatchCornerRadius = 4.0f;

	/** Thumb diameter in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ColorPicker", meta = (ClampMin = "4.0", ClampMax = "64.0"))
	float ThumbSize = 24.0f;

	/** Thumb color (normal state) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ColorPicker")
	FLinearColor ThumbColor = FLinearColor::White;

	/** Thumb color when hovered */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ColorPicker")
	FLinearColor ThumbHoveredColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);

	/** Thumb outline color (for visibility against any gradient colour) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ColorPicker")
	FLinearColor ThumbOutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);

	/** Thumb outline width */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ColorPicker", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float ThumbOutlineWidth = 1.5f;

	/** Swatch border color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ColorPicker")
	FLinearColor SwatchBorderColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);

	/** Get effective thumb diameter (falls back to 90% of gradient height if somehow 0). */
	float GetEffectiveThumbSize(float FallbackGradientHeight) const
	{
		return (ThumbSize > 0.0f) ? ThumbSize : FMath::Max(FallbackGradientHeight * 0.9f, 12.0f);
	}
};

/**
 * DataAsset defining the visual style for RammsUI
 * Create instances in the editor for different themes (Dark, Light, Custom, etc.)
 */
UCLASS(BlueprintType)
class RAMMSUI_API URammsUIStyle : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Style name (e.g., "Dark Theme", "Light Theme") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FString StyleName = TEXT("Default");

	/** Color palette */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FRammsColorPalette Colors;

	/** Typography settings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FRammsTypography Typography;

	/** Spacing values */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FRammsSpacing Spacing;

	/** Border and corner settings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FRammsBorderStyle Border;

	/** Default scrollbar appearance (used by all scroll widgets unless overridden) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FRammsScrollBarStyle ScrollBar;

	/** Default slider appearance (thumb, bar, colors — used unless overridden per widget) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FRammsSliderStyle Slider;

	/** Default checkbox / radio button appearance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FRammsCheckBoxStyle CheckBox;

	/** Default colour picker appearance (gradient bars, swatch, thumb) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FRammsColorPickerStyle ColorPicker;

	/** Default fade in animation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
	FRammsAnimationCurve FadeInCurve = FRammsAnimationCurve();

	/** Default fade out animation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
	FRammsAnimationCurve FadeOutCurve = FRammsAnimationCurve();

	/** Default slide animation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
	FRammsAnimationCurve SlideCurve = FRammsAnimationCurve();

	/** Default scale animation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
	FRammsAnimationCurve ScaleCurve = FRammsAnimationCurve();

	/** Default expand/collapse animation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
	FRammsAnimationCurve ExpandCollapseCurve;

	URammsUIStyle()
	{
		// Set default expand/collapse to slightly slower
		ExpandCollapseCurve.Duration = 0.4f;
		ExpandCollapseCurve.Easing = ERammsUIEasing::EaseInOut;
	}

	/**
	 * Get color with optional opacity override
	 */
	UFUNCTION(BlueprintCallable, Category = "Style")
	FLinearColor GetColorWithOpacity(FLinearColor BaseColor, float Opacity) const
	{
		FLinearColor Result = BaseColor;
		Result.A = Opacity;
		return Result;
	}

	/**
	 * Get interpolated spacing between two sizes
	 */
	UFUNCTION(BlueprintCallable, Category = "Style")
	float GetSpacing(float Small, float Large, float Alpha) const
	{
		return FMath::Lerp(Small, Large, Alpha);
	}

	/**
	 * Create a rounded box FSlateBrush (uniform corner radius)
	 */
	static FSlateBrush MakeRoundedBoxBrush(FLinearColor BackgroundColor, float CornerRadius,
		FLinearColor OutlineColor = FLinearColor::Transparent, float OutlineWidth = 0.0f)
	{
		return MakeRoundedBoxBrushEx(BackgroundColor, FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius), OutlineColor, OutlineWidth);
	}

	/**
	 * Create a rounded box FSlateBrush (per-corner radii: TopLeft, TopRight, BottomRight, BottomLeft)
	 */
	static FSlateBrush MakeRoundedBoxBrushEx(FLinearColor BackgroundColor, FVector4 CornerRadii,
		FLinearColor OutlineColor = FLinearColor::Transparent, float OutlineWidth = 0.0f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(BackgroundColor);
		Brush.OutlineSettings.CornerRadii = CornerRadii;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		if (OutlineWidth > 0.0f)
		{
			Brush.OutlineSettings.Color = FSlateColor(OutlineColor);
			Brush.OutlineSettings.Width = OutlineWidth;
		}
		return Brush;
	}

	/**
	 * Apply a rounded brush to a UBorder widget, syncing to the underlying Slate widget
	 */
	static void ApplyRoundedBrushToBorder(class UBorder* Border, const FSlateBrush& Brush);

	/**
	 * Apply scrollbar styling to a UScrollBox.
	 */
	static void ApplyScrollBarStyle(class UScrollBox* ScrollBox, const FRammsScrollBarStyle& ScrollBarStyle);

	/**
	 * Apply slider styling to a USlider.
	 * Builds rounded-box brushes for thumb and bar from the style settings.
	 *
	 * @param Slider       The USlider to style.
	 * @param SliderStyle  The slider appearance settings.
	 */
	static void ApplySliderStyle(class USlider* Slider, const FRammsSliderStyle& SliderStyle);

	/**
	 * Apply checkbox styling to a UCheckBox.
	 * Builds rounded-box brushes for checked/unchecked/hovered states.
	 *
	 * @param CheckBox      The UCheckBox to style.
	 * @param CBStyle       The checkbox appearance settings.
	 */
	static void ApplyCheckBoxStyle(class UCheckBox* CheckBox, const FRammsCheckBoxStyle& CBStyle);

	// ── Built-in Theme Factories ────────────────────────────────

	/**
	 * Create a transient URammsUIStyle with dark theme defaults.
	 * The returned object is Transient — it will not be serialized.
	 */
	static URammsUIStyle* CreateDefaultDarkTheme();

	/**
	 * Create a transient URammsUIStyle with light theme defaults.
	 * The returned object is Transient — it will not be serialized.
	 */
	static URammsUIStyle* CreateDefaultLightTheme();

	/**
	 * Populate a color palette with light theme colors.
	 */
	static FRammsColorPalette MakeLightPalette();

	/**
	 * Create a typography set using UE's built-in engine fonts (Roboto + DroidSansMono).
	 * Call this to get properly initialized fonts instead of empty FSlateFontInfo defaults.
	 */
	static FRammsTypography MakeDefaultTypography();

	// ── Instance Utilities ──────────────────────────────────────

	/**
	 * Reset this theme's values to the built-in dark theme defaults.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Theme")
	void ResetToDarkDefaults();

	/**
	 * Reset this theme's values to the built-in light theme defaults.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Theme")
	void ResetToLightDefaults();

	/**
	 * Copy all style values from another theme into this one.
	 * Does not change this object's name or outer — only style data.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Theme")
	void CopyFrom(const URammsUIStyle* Source);

#if WITH_EDITOR
	/**
	 * Internal: create a DataAsset in the given content path, save, and register.
	 * Called by URammsUISubsystem theme asset utilities.
	 */
	static URammsUIStyle* CreateAndSaveThemeAsset(const FString& AssetPath, bool bLight);

	/**
	 * Internal: duplicate an existing style into a saveable DataAsset.
	 */
	static URammsUIStyle* DuplicateAsAsset(URammsUIStyle* Source, const FString& AssetPath);
#endif
};
