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

	/** Fired when the reset button is pressed. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAxisResetPressed, float, NewValue);
	UPROPERTY(BlueprintAssignable, Category = "Axis Control")
	FOnAxisResetPressed OnResetPressed;

	/** Fired when the increment (+) button is pressed. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAxisIncrementPressed, float, NewValue);
	UPROPERTY(BlueprintAssignable, Category = "Axis Control")
	FOnAxisIncrementPressed OnIncrementPressed;

	/** Fired when the decrement (−) button is pressed. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAxisDecrementPressed, float, NewValue);
	UPROPERTY(BlueprintAssignable, Category = "Axis Control")
	FOnAxisDecrementPressed OnDecrementPressed;

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
	virtual void	 ResetCachedWidgets() override;
	virtual void	 BuildWidgetTree() override;
	virtual UWidget* GetRootWidgetForValidation() override { return OuterHBox; }

	UPROPERTY()
	float CurrentValue = 0.0f;

	// ── Cached Widgets (Transient — rebuilt programmatically) ─────

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> OuterHBox;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> IconSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UImage> IconImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ResetButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResetLabel;

	UPROPERTY(Transient)
	TObjectPtr<USlider> InnerSlider;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ControlRow;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DecrementButton;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> DecrementSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DecrementLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> IncrementButton;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> IncrementSizeBox;

	UPROPERTY(Transient)
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
