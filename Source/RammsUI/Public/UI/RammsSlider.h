// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "RammsSlider.generated.h"

/**
 * Styled slider widget with value label
 */
UCLASS(meta = (DisplayName = "Ramms Slider"))
class RAMMSUI_API URammsSlider : public URammsBaseWidget
{
	GENERATED_BODY()

protected:
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

	// Widget references (built programmatically)
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

	/**
	 * Set slider value
	 */
	UFUNCTION(BlueprintCallable, Category = "Slider")
	void SetValue(float NewValue);

	/**
	 * Get slider value
	 */
	UFUNCTION(BlueprintPure, Category = "Slider")
	float GetValue() const { return Value; }

	/**
	 * Set value range
	 */
	UFUNCTION(BlueprintCallable, Category = "Slider")
	void SetRange(float Min, float Max);

	/**
	 * Set label text
	 */
	UFUNCTION(BlueprintCallable, Category = "Slider")
	void SetLabel(FText Label);

	/**
	 * Set units suffix text
	 */
	UFUNCTION(BlueprintCallable, Category = "Slider")
	void SetUnits(FText Units);

protected:
	virtual void SynchronizeProperties() override;
	/** Build the widget tree programmatically */
	virtual void ResetCachedWidgets() override;
	virtual void BuildWidgetTree() override;

	UFUNCTION()
	void OnSliderValueChanged(float NewValue);

	/** Update value label text */
	void UpdateValueLabel();
};
