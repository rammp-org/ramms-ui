// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "RammsRobotTypes.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "RammsSeatController.generated.h"

class URammsAxisControl;

/**
 * Controller widget for seat adjustments.
 * Provides three axes: Elevation, Lateral Tilt, and Anterior/Posterior Tilt.
 * Each axis is configured independently via FRammsAxisConfig and rendered
 * using URammsAxisControl child widgets (icon + label + value + reset + slider).
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

	/** Elevation axis configuration (range, icon, default, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat|Elevation")
	FRammsAxisConfig ElevationConfig;

	/** Lateral tilt axis configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat|Lateral Tilt")
	FRammsAxisConfig LateralTiltConfig;

	/** Anterior/Posterior tilt axis configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat|A/P Tilt")
	FRammsAxisConfig APTiltConfig;

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
	TObjectPtr<URammsAxisControl> ElevationControl;

	UPROPERTY()
	TObjectPtr<URammsAxisControl> LateralTiltControl;

	UPROPERTY()
	TObjectPtr<URammsAxisControl> APTiltControl;

public:
	/** Fired when any seat axis value changes (from user interaction). */
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

	/** Reset a specific axis to its home/default position */
	UFUNCTION(BlueprintCallable, Category = "Seat")
	void ResetAxis(ERammsSeatAxis Axis);

	/** Reset all axes to home/default positions */
	UFUNCTION(BlueprintCallable, Category = "Seat")
	void ResetAllAxes();

protected:
	virtual void ResetCachedWidgets() override;
	virtual void BuildWidgetTree() override;

	/** Get the axis control for a given axis */
	URammsAxisControl* GetAxisControl(ERammsSeatAxis Axis) const;

	/** Get the mutable config reference for a given axis */
	FRammsAxisConfig& GetAxisConfig(ERammsSeatAxis Axis);

	// Per-axis delegate handlers (required by UFUNCTION binding)
	UFUNCTION()
	void OnElevationChanged(float Value);
	UFUNCTION()
	void OnLateralTiltChanged(float Value);
	UFUNCTION()
	void OnAPTiltChanged(float Value);

	/** Common handler for axis value changes from user interaction */
	void HandleAxisChanged(ERammsSeatAxis Axis, float Value);

	virtual void OnRobotControllerResolved(AActor* ControllerActor) override;
};
