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
};
