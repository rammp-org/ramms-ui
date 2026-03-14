// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Components/Slider.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "RammsSlider.generated.h"

/**
 * Styled slider widget with value label, optional panel background, and
 * touch-friendly customization for thumb/bar appearance.
 *
 * Features:
 * - Optional styled panel background (rounded corners, border) matching RammsPanel
 * - Configurable thumb size, bar thickness, and colors
 * - Label with units suffix
 * - Works in Canvas Panel, Vertical/Horizontal Box, or inside other containers
 */
UCLASS(meta = (DisplayName = "Ramms Slider"))
class RAMMSUI_API URammsSlider : public URammsBaseWidget
{
	GENERATED_BODY()

protected:
	// ── Slider Values ──

	/** Minimum value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider")
	float MinValue = 0.0f;

	/** Maximum value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider")
	float MaxValue = 1.0f;

	/** Current value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider")
	float Value = 0.5f;

	/** Step size (0 = continuous) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider")
	float StepSize = 0.0f;

	/** Label text (e.g., "Speed", "Volume") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider", meta = (ExposeOnSpawn = true))
	FText LabelText = FText::FromString(TEXT("Value"));

	/** Units suffix (e.g., "°", "m/s", "%") — appended to value display */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider")
	FText UnitsText;

	/** Show current value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider")
	bool bShowValue = true;

	/** Number of decimal places to show */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider", meta = (ClampMin = "0", ClampMax = "3"))
	int32 DecimalPlaces = 2;

	// ── Panel Settings ──

	/** Wrap the slider in a styled panel with background and rounded corners */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider|Panel")
	bool bShowPanel = true;

	/** Show border outline on the panel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider|Panel", meta = (EditCondition = "bShowPanel"))
	bool bShowPanelBorder = true;

	/** Inner padding within the panel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider|Panel", meta = (EditCondition = "bShowPanel"))
	FMargin PanelPadding = FMargin(12.0f, 8.0f);

	// ── Slider Appearance ──

	/** Thumb (handle) diameter in pixels. Larger = easier to touch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider|Appearance", meta = (ClampMin = "8.0", ClampMax = "64.0"))
	float ThumbSize = 24.0f;

	/** Track bar thickness in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider|Appearance", meta = (ClampMin = "1.0", ClampMax = "32.0"))
	float BarThickness = 6.0f;

	/** Use colors from the URammsUIStyle asset. When false, uses the manual color overrides below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider|Appearance")
	bool bUseStyleColors = true;

	/** Track (unfilled) color — used when bUseStyleColors is false */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider|Appearance",
		meta = (EditCondition = "!bUseStyleColors"))
	FLinearColor TrackColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);

	/** Filled bar color — used when bUseStyleColors is false */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider|Appearance",
		meta = (EditCondition = "!bUseStyleColors"))
	FLinearColor ActiveBarColor = FLinearColor(0.0f, 0.478f, 0.8f, 1.0f);

	/** Thumb color — used when bUseStyleColors is false */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slider|Appearance",
		meta = (EditCondition = "!bUseStyleColors"))
	FLinearColor ThumbColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	// ── Cached Widgets ──

	UPROPERTY()
	TObjectPtr<UBorder> ContainerBorder;

	UPROPERTY()
	TObjectPtr<UVerticalBox> ContentVBox;

	UPROPERTY()
	TObjectPtr<USlider> InnerSlider;

	UPROPERTY()
	TObjectPtr<UTextBlock> SliderLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> ValueLabel;

public:
	/** On value changed delegate */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRammsSliderValueChanged, float, NewValue);
	UPROPERTY(BlueprintAssignable, Category = "Slider")
	FOnRammsSliderValueChanged OnValueChanged;

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void ApplyStyle_Implementation() override;

	/** Set slider value */
	UFUNCTION(BlueprintCallable, Category = "Slider")
	void SetValue(float NewValue);

	/** Get slider value */
	UFUNCTION(BlueprintPure, Category = "Slider")
	float GetValue() const { return Value; }

	/** Set value range */
	UFUNCTION(BlueprintCallable, Category = "Slider")
	void SetRange(float Min, float Max);

	/** Set label text */
	UFUNCTION(BlueprintCallable, Category = "Slider")
	void SetLabel(FText Label);

	/** Set units suffix text */
	UFUNCTION(BlueprintCallable, Category = "Slider")
	void SetUnits(FText Units);

	/** Show or hide the styled panel background */
	UFUNCTION(BlueprintCallable, Category = "Slider")
	void SetShowPanel(bool bShow);

protected:
	virtual void SynchronizeProperties() override;
	virtual void ResetCachedWidgets() override;
	virtual void BuildWidgetTree() override;

	UFUNCTION()
	void OnSliderValueChanged(float NewValue);

	/** Update value label text */
	void UpdateValueLabel();

	/** Apply slider bar/thumb styling */
	void ApplySliderAppearance();
};
