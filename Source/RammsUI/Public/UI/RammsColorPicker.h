// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "RammsColorPicker.generated.h"

/**
 * Color picker widget with a hue gradient slider, optional saturation and
 * brightness sliders, and a live color preview swatch.
 *
 * Each slider is overlaid on a procedurally generated gradient texture so the
 * user sees the available colours as they drag. The slider bar itself is made
 * transparent; only the thumb is visible, sliding over the gradient image.
 *
 * Usage:
 *   - Hue-only mode (default): single rainbow slider + swatch.
 *   - Full HSV mode: enable bShowSaturation and/or bShowBrightness for
 *     additional sliders whose gradients update in real time.
 */
UCLASS(meta = (DisplayName = "Ramms Color Picker"))
class RAMMSUI_API URammsColorPicker : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	// ── Color Value ──

	/** Current hue in degrees (0–360). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Picker",
		meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float Hue = 0.0f;

	/** Current saturation (0–1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Picker",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Saturation = 1.0f;

	/** Current brightness / value (0–1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Picker",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Brightness = 1.0f;

	// ── Display Options ──

	/** Label displayed above the picker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Picker",
		meta = (ExposeOnSpawn = true))
	FText LabelText = FText::FromString(TEXT("Color"));

	/** Show a saturation slider below the hue slider. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Picker")
	bool bShowSaturation = false;

	/** Show a brightness slider below the saturation slider. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Picker")
	bool bShowBrightness = false;

	/** Show the hex colour value next to the swatch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Picker")
	bool bShowHexValue = false;

	// ── Panel Settings ──

	/** Wrap the picker in a styled panel with background and rounded corners. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Picker|Panel")
	bool bShowPanel = true;

	/** Show border outline on the panel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Picker|Panel",
		meta = (EditCondition = "bShowPanel"))
	bool bShowPanelBorder = true;

	/** Inner padding within the panel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Picker|Panel",
		meta = (EditCondition = "bShowPanel"))
	FMargin PanelPadding = FMargin(12.0f, 8.0f);

	// ── Appearance ──

	/** Override the style asset's gradient height */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Picker|Appearance",
		meta = (InlineEditConditionToggle))
	bool bOverrideGradientHeight = false;

	/** Height of each gradient bar in pixels (used when bOverrideGradientHeight is true). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Picker|Appearance",
		meta = (EditCondition = "bOverrideGradientHeight", ClampMin = "8.0", ClampMax = "64.0"))
	float GradientHeight = 20.0f;

	/** Override the style asset's swatch size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Picker|Appearance",
		meta = (InlineEditConditionToggle))
	bool bOverrideSwatchSize = false;

	/** Size of the colour swatch square in pixels (used when bOverrideSwatchSize is true). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Picker|Appearance",
		meta = (EditCondition = "bOverrideSwatchSize", ClampMin = "12.0", ClampMax = "64.0"))
	float SwatchSize = 28.0f;

	// ── Events ──

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnColorChanged, FLinearColor, NewColor);

	/** Fired whenever the user changes any HSV component. */
	UPROPERTY(BlueprintAssignable, Category = "Color Picker")
	FOnColorChanged OnColorChanged;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHueChanged, float, NewHue);

	/** Fired when only the hue slider changes (convenience for hue-only use). */
	UPROPERTY(BlueprintAssignable, Category = "Color Picker")
	FOnHueChanged OnHueChanged;

	// ── Public API ──

	/** Set the colour from an FLinearColor (converts to HSV internally). */
	UFUNCTION(BlueprintCallable, Category = "Color Picker")
	void SetColor(FLinearColor Color);

	/** Get the current colour as an FLinearColor. */
	UFUNCTION(BlueprintPure, Category = "Color Picker")
	FLinearColor GetColor() const;

	/** Set hue directly (0–360). Does not broadcast OnColorChanged. */
	UFUNCTION(BlueprintCallable, Category = "Color Picker")
	void SetHue(float NewHue);

	/** Set saturation directly (0–1). Does not broadcast OnColorChanged. */
	UFUNCTION(BlueprintCallable, Category = "Color Picker")
	void SetSaturation(float NewSaturation);

	/** Set brightness directly (0–1). Does not broadcast OnColorChanged. */
	UFUNCTION(BlueprintCallable, Category = "Color Picker")
	void SetBrightness(float NewBrightness);

	/** Set the label text. */
	UFUNCTION(BlueprintCallable, Category = "Color Picker")
	void SetLabel(FText Label);

	/** Toggle saturation slider visibility at runtime. */
	UFUNCTION(BlueprintCallable, Category = "Color Picker")
	void SetShowSaturation(bool bShow);

	/** Toggle brightness slider visibility at runtime. */
	UFUNCTION(BlueprintCallable, Category = "Color Picker")
	void SetShowBrightness(bool bShow);

	// ── Overrides ──

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void ApplyStyle_Implementation() override;

protected:
	virtual void SynchronizeProperties() override;
	virtual void ResetCachedWidgets() override;
	virtual void BuildWidgetTree() override;

private:
	// ── Gradient Texture Generation ──

	/** Width of the gradient textures in pixels. */
	static constexpr int32 GRADIENT_WIDTH = 256;

	/** Generate a hue rainbow texture (S=1, V=1, H varies 0–360). */
	UTexture2D* GenerateHueGradient() const;

	/** Generate a saturation gradient (current H, V=1, S varies 0–1). */
	UTexture2D* GenerateSaturationGradient() const;

	/** Generate a brightness gradient (current H+S, V varies 0–1). */
	UTexture2D* GenerateBrightnessGradient() const;

	/** Update a texture's pixel data in-place. */
	static void UpdateTextureData(UTexture2D* Texture, const TArray<FColor>& Pixels);

	/** Rebuild the saturation and brightness gradient textures to reflect current HSV. */
	void RefreshDependentGradients();

	/** Rebuild only the saturation gradient (depends on Hue). */
	void RefreshSaturationGradient();

	/** Rebuild only the brightness gradient (depends on Hue + Saturation). */
	void RefreshBrightnessGradient();

	/** Update the swatch colour and hex label. */
	void UpdateSwatchAndLabels();

	/** Make a slider's bar fully transparent so the gradient image shows through. */
	void MakeSliderBarTransparent(USlider* Slider) const;

	/** Update the swatch image brush (rounded solid-colour box). */
	void UpdateSwatchBrush(const FRammsColorPickerStyle& PS);

	/** Resolve the effective picker style from the theme + per-widget overrides. */
	FRammsColorPickerStyle ResolvePickerStyle() const;

	// ── Widget Callbacks ──

	UFUNCTION()
	void OnHueSliderChanged(float NewValue);

	UFUNCTION()
	void OnSaturationSliderChanged(float NewValue);

	UFUNCTION()
	void OnBrightnessSliderChanged(float NewValue);

	/** Broadcast colour change delegates. */
	void BroadcastChange();

	// ── Cached Widgets ──

	UPROPERTY()
	TObjectPtr<UBorder> ContainerBorder;

	UPROPERTY()
	TObjectPtr<UVerticalBox> ContentVBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> PickerLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> HexLabel;

	UPROPERTY()
	TObjectPtr<UImage> SwatchImage;

	UPROPERTY()
	TObjectPtr<class USizeBox> SwatchSizeBoxWidget;

	// Hue row
	UPROPERTY()
	TObjectPtr<UOverlay> HueOverlay;

	UPROPERTY()
	TObjectPtr<UImage> HueGradientImage;

	UPROPERTY()
	TObjectPtr<USlider> HueSlider;

	// Saturation row
	UPROPERTY()
	TObjectPtr<UOverlay> SatOverlay;

	UPROPERTY()
	TObjectPtr<UImage> SatGradientImage;

	UPROPERTY()
	TObjectPtr<USlider> SatSlider;

	// Brightness row
	UPROPERTY()
	TObjectPtr<UOverlay> BriOverlay;

	UPROPERTY()
	TObjectPtr<UImage> BriGradientImage;

	UPROPERTY()
	TObjectPtr<USlider> BriSlider;

	// Cached gradient textures (managed per-instance)
	UPROPERTY()
	TObjectPtr<UTexture2D> HueTexture;

	UPROPERTY()
	TObjectPtr<UTexture2D> SatTexture;

	UPROPERTY()
	TObjectPtr<UTexture2D> BriTexture;
};
