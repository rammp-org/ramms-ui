// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "RammsRobotTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "RammsSeatController.generated.h"

/**
 * One axis row in the seat controller (icon + label + -/+ buttons + value).
 * Internal struct — not exposed individually to Blueprint.
 */
USTRUCT()
struct FRammsSeatAxisRow
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UHorizontalBox> Row;

	UPROPERTY()
	TObjectPtr<UImage> Icon;

	UPROPERTY()
	TObjectPtr<UTextBlock> Label;

	UPROPERTY()
	TObjectPtr<UButton> DecreaseButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> DecreaseLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY()
	TObjectPtr<UButton> IncreaseButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> IncreaseLabel;
};

/**
 * Controller widget for seat adjustments.
 * Provides three axes: Elevation, Lateral Tilt, and Anterior/Posterior Tilt.
 * Each axis displays an icon, label, decrease/increase buttons, and current value.
 * Auto-discovers actors implementing IRammsRobotController.
 */
UCLASS(meta = (DisplayName = "Ramms Seat Controller"))
class RAMMSUI_API URammsSeatController : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	URammsSeatController(const FObjectInitializer& ObjectInitializer);

protected:
	// ── Configuration ────────────────────────────────────────────

	/** Header title text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	FText HeaderTitle = FText::FromString(TEXT("Seat Control"));

	/** Increment/decrement step per button press (normalized units) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float StepSize = 0.1f;

	/** Minimum value for elevation axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat|Limits")
	float ElevationMin = 0.0f;

	/** Maximum value for elevation axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat|Limits")
	float ElevationMax = 1.0f;

	/** Minimum value for tilt axes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat|Limits")
	float TiltMin = -1.0f;

	/** Maximum value for tilt axes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat|Limits")
	float TiltMax = 1.0f;

	/** Icon size for axis icons */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	FVector2D IconSize = FVector2D(28.0f, 28.0f);

	// ── Axis Icons (assign in editor or Blueprint) ───────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat|Icons")
	TObjectPtr<UTexture2D> ElevationIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat|Icons")
	TObjectPtr<UTexture2D> LateralTiltIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat|Icons")
	TObjectPtr<UTexture2D> AnteriorPosteriorTiltIcon;

	// ── Current State ────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, Category = "Seat")
	FRammsSeatState CurrentState;

	// ── Widget References ────────────────────────────────────────

	UPROPERTY()
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> MainVBox;

	UPROPERTY()
	FRammsSeatAxisRow ElevationRow;

	UPROPERTY()
	FRammsSeatAxisRow LateralTiltRow;

	UPROPERTY()
	FRammsSeatAxisRow APTiltRow;

public:
	/** Fired when any seat axis value changes */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSeatValueChanged, ERammsSeatAxis, Axis, float, NewValue);
	UPROPERTY(BlueprintAssignable, Category = "Seat")
	FOnSeatValueChanged OnValueChanged;

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void ApplyStyle_Implementation() override;
	virtual void SynchronizeProperties() override;

	/** Set a specific axis value programmatically */
	UFUNCTION(BlueprintCallable, Category = "Seat")
	void SetAxisValue(ERammsSeatAxis Axis, float Value);

	/** Get a specific axis value */
	UFUNCTION(BlueprintPure, Category = "Seat")
	float GetAxisValue(ERammsSeatAxis Axis) const;

	/** Get the full seat state */
	UFUNCTION(BlueprintPure, Category = "Seat")
	FRammsSeatState GetSeatState() const { return CurrentState; }

	/** Set icon for a specific axis at runtime */
	UFUNCTION(BlueprintCallable, Category = "Seat")
	void SetAxisIcon(ERammsSeatAxis Axis, UTexture2D* Icon);

protected:
	virtual void ResetCachedWidgets() override;
	virtual void BuildWidgetTree() override;

	/** Build one axis row and add it to the VBox */
	FRammsSeatAxisRow BuildAxisRow(const FName& RowName, const FText& LabelText,
		UTexture2D* AxisIcon, ERammsSeatAxis Axis);

	/** Update the displayed value text for an axis */
	void UpdateValueDisplay(ERammsSeatAxis Axis);

	/** Update all value displays */
	void UpdateAllValueDisplays();

	/** Get min/max for an axis */
	void GetAxisLimits(ERammsSeatAxis Axis, float& OutMin, float& OutMax) const;

	/** Get the row struct for an axis */
	FRammsSeatAxisRow& GetAxisRow(ERammsSeatAxis Axis);

	// Button handlers — one per axis per direction
	UFUNCTION()
	void OnElevationDecrease();
	UFUNCTION()
	void OnElevationIncrease();
	UFUNCTION()
	void OnLateralTiltDecrease();
	UFUNCTION()
	void OnLateralTiltIncrease();
	UFUNCTION()
	void OnAPTiltDecrease();
	UFUNCTION()
	void OnAPTiltIncrease();

	void HandleAxisAdjust(ERammsSeatAxis Axis, float Delta);

	virtual void OnRobotControllerResolved(AActor* ControllerActor) override;
};
