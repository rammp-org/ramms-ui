// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "RammsControlTypes.h"
#include "RammsControlRow.generated.h"

class URammsAxisControl;
class URammsButton;
class UButton;
class UHorizontalBox;
class UTextBlock;

/**
 * One control of a control surface, rendered from its FRammsControlAxis:
 *
 *  - Position / Velocity  -> URammsAxisControl (label, value, reset, slider, +/-)
 *  - Continuous (unpaired) -> label + hold buttons (- / +) that set the axis
 *                            while pressed and release it on let-go
 *  - Action                -> URammsButton
 *
 * Owns its delegate handlers (it knows its Id, sink and source), so the
 * panel just lays rows out and calls Refresh for readback.
 */
UCLASS(meta = (DisplayName = "Ramms Control Row"))
class RAMMSUI_API URammsControlRow : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	/** Bind this row to a control on a sink. Builds the row's widgets. */
	UFUNCTION(BlueprintCallable, Category = "Control Surface")
	void Setup(UObject* InSink, const FRammsControlAxis& InAxis, ERammsControlSource InSource);

	/** Pull the live value into the display (skipped briefly after user input). */
	UFUNCTION(BlueprintCallable, Category = "Control Surface")
	void Refresh();

	UFUNCTION(BlueprintPure, Category = "Control Surface")
	FName GetControlId() const { return Axis.Id; }

	UFUNCTION(BlueprintPure, Category = "Control Surface")
	FRammsControlAxis GetAxis() const { return Axis; }

	/** The slider's value (the target it shows), for Position / Velocity rows. */
	UFUNCTION(BlueprintPure, Category = "Control Surface")
	float GetTargetValue() const;

	// --- Scripted interaction (tests, demos, remote UIs) -----------------------

	/** As if the slider were moved to Value (Position / Velocity rows). */
	UFUNCTION(BlueprintCallable, Category = "Control Surface")
	void SimulateValue(float Value);

	/** As if the action button were clicked. */
	UFUNCTION(BlueprintCallable, Category = "Control Surface")
	void SimulateAction();

	/** As if the + (or -) hold button were pressed (Continuous rows). */
	UFUNCTION(BlueprintCallable, Category = "Control Surface")
	void SimulateHold(bool bPlus);

	/** As if a hold button were released. */
	UFUNCTION(BlueprintCallable, Category = "Control Surface")
	void SimulateRelease();

	/** Value a hold button pushes (Continuous axes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	float HoldValue = 1.0f;

	/** Fixed width of the changing-number columns (live readback, rate value),
	 *  so a sign flip or an extra digit never re-lays out the row and panel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	float ValueColumnWidth = 76.0f;

	/** Seconds after a user edit during which readback leaves the widget alone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	float ReadbackHoldOff = 0.75f;

	virtual void ApplyStyle_Implementation() override;
	virtual void NativeDestruct() override;

protected:
	virtual void	 BuildWidgetTree() override;
	virtual void	 ResetCachedWidgets() override;
	virtual UWidget* GetRootWidgetForValidation() override;

private:
	UFUNCTION()
	void OnAxisValue(float NewValue);
	UFUNCTION()
	void OnActionClicked();
	UFUNCTION()
	void OnPlusPressed();
	UFUNCTION()
	void OnPlusReleased();
	UFUNCTION()
	void OnMinusPressed();
	UFUNCTION()
	void OnMinusReleased();
	UFUNCTION()
	void OnEnumPrevClicked();
	UFUNCTION()
	void OnEnumNextClicked();

	/** Step an Enum control's index, wrapping, and send it. */
	void StepEnum(int32 Delta);

	void	 SetAxis(float Value);
	void	 ReleaseAxis();
	FText	 UnitsText() const;
	int32	 Decimals() const;
	UButton* MakeHoldButton(const TCHAR* Name, const TCHAR* Label, UTextBlock*& OutLabel);

	UPROPERTY(Transient)
	TObjectPtr<UObject> Sink;

	UPROPERTY(Transient)
	FRammsControlAxis Axis;

	ERammsControlSource Source = ERammsControlSource::Touch;
	double				LastUserInputTime = -1000.0;
	bool				bRefreshing = false;
	/** A hold button is down: the axis is ours until released (or we go away). */
	bool bHoldActive = false;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> RootBox;
	UPROPERTY(Transient)
	TObjectPtr<URammsAxisControl> AxisControl;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LiveLabel;
	UPROPERTY(Transient)
	TObjectPtr<URammsButton> ActionButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RateLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RateValue;

	/** Shows the active choice of an Enum control. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EnumValue;
	/** Position / Velocity rows: the live readback, separate from the slider's target. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LiveValue;
	/** A target has been commanded through this row (the slider shows it). */
	bool bHasTarget = false;
	/** The slider was seeded once from the live pose. */
	bool bTargetSeeded = false;
	void RefreshInternal(bool bPeriodic);
	UPROPERTY(Transient)
	TObjectPtr<UButton> MinusButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> PlusButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MinusLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlusLabel;
};
