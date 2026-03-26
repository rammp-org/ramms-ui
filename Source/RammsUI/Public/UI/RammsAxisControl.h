// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "RammsRobotTypes.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "RammsAxisControl.generated.h"

/**
 * Reusable axis control widget.
 *
 * Displays an icon, label, current value, reset-to-default button,
 * optional slider, and optional large +/- increment buttons.
 * Configurable via FRammsAxisConfig for range, icon, default value, units, etc.
 *
 * Layout:  HBox( Icon,  VBox( HBox(Label, Value, ResetBtn),
 *                             HBox([MinusBtn], [Slider], [PlusBtn]) ) )
 *
 * Can be used standalone in Blueprints / named slots, or embedded
 * programmatically as a child of another widget (e.g., URammsSeatController).
 */
UCLASS(meta = (DisplayName = "Ramms Axis Control"))
class RAMMSUI_API URammsAxisControl : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	URammsAxisControl(const FObjectInitializer& ObjectInitializer);

	// ── Configuration ──────────────────────────────────────────────

	/** Axis configuration (icon, range, default, display, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axis Control")
	FRammsAxisConfig Config;

	// ── Delegates ──────────────────────────────────────────────────

	/** Fired when the value changes via user interaction (slider or reset button). */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAxisValueChanged, float, NewValue);
	UPROPERTY(BlueprintAssignable, Category = "Axis Control")
	FOnAxisValueChanged OnValueChanged;

	// ── Public API ─────────────────────────────────────────────────

	/** Set value programmatically (does NOT fire OnValueChanged). */
	UFUNCTION(BlueprintCallable, Category = "Axis Control")
	void SetValue(float NewValue);

	/** Get the current value. */
	UFUNCTION(BlueprintPure, Category = "Axis Control")
	float GetValue() const { return CurrentValue; }

	/** Reset to the config's default value (fires OnValueChanged if value changed). */
	UFUNCTION(BlueprintCallable, Category = "Axis Control")
	void ResetToDefault();

	/** Apply a new configuration at runtime, updating all child widgets. */
	UFUNCTION(BlueprintCallable, Category = "Axis Control")
	void ApplyConfig(const FRammsAxisConfig& NewConfig);

	/** Set the icon texture at runtime. */
	UFUNCTION(BlueprintCallable, Category = "Axis Control")
	void SetIcon(UTexture2D* NewIcon);

	// ── Lifecycle ──────────────────────────────────────────────────

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void ApplyStyle_Implementation() override;
	virtual void SynchronizeProperties() override;

protected:
	virtual void ResetCachedWidgets() override;
	virtual void BuildWidgetTree() override;

	UPROPERTY()
	float CurrentValue = 0.0f;

	// ── Cached Widgets ─────────────────────────────────────────────

	UPROPERTY()
	TObjectPtr<UHorizontalBox> OuterHBox;

	UPROPERTY()
	TObjectPtr<USizeBox> IconSizeBox;

	UPROPERTY()
	TObjectPtr<UImage> IconImage;

	UPROPERTY()
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY()
	TObjectPtr<UButton> ResetButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> ResetLabel;

	UPROPERTY()
	TObjectPtr<USlider> InnerSlider;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> ControlRow;

	UPROPERTY()
	TObjectPtr<UButton> DecrementButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> DecrementLabel;

	UPROPERTY()
	TObjectPtr<UButton> IncrementButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> IncrementLabel;

	// ── Internal Helpers ───────────────────────────────────────────

	void UpdateValueDisplay();
	void UpdateSliderFromValue();
	void ApplyConfigToWidgets();

	/** Convert Config.StepSize (actual value units) to normalized [0,1] for USlider. */
	float GetNormalizedStepSize() const;

	/** Get effective step for +/- buttons (StepSize or 5% of range when continuous). */
	float GetEffectiveButtonStep() const;

	/** Snap a value to the configured step grid (no-op when StepSize == 0). */
	float SnapToStep(float Value) const;

	UFUNCTION()
	void OnSliderChanged(float Value);

	UFUNCTION()
	void OnResetClicked();

	UFUNCTION()
	void OnDecrementClicked();

	UFUNCTION()
	void OnIncrementClicked();
};
