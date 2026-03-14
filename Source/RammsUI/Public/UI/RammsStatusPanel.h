// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Interfaces/IRammsStateProvider.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "RammsStatusPanel.generated.h"

/**
 * Collapsible status panel showing robot state
 * Displays: speed, battery level, mode
 * Integrates with IRammsStateProvider
 */
UCLASS(meta = (DisplayName = "Ramms Status Panel"))
class RAMMSUI_API URammsStatusPanel : public URammsBaseWidget
{
	GENERATED_BODY()

protected:
	/** State provider (optional - can be set at runtime) */
	UPROPERTY(BlueprintReadWrite, Category = "State")
	TScriptInterface<IRammsStateProvider> StateProvider;

	/** Whether panel is expanded */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	bool bIsExpanded = true;

	/** Update frequency in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display", meta = (ClampMin = "0.016"))
	float UpdateFrequency = 0.1f;

	// Widget references (built programmatically)
	UPROPERTY()
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY()
	TObjectPtr<UButton> ToggleButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> ContentBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> SpeedText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BatteryText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ModeText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ConnectionText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ArmText;

	/** Time since last update */
	float TimeSinceUpdate = 0.0f;

	/** Delegate handle for state updates */
	FDelegateHandle StateUpdateHandle;

	/** Whether remote state has been applied (suppresses "--" placeholder) */
	bool bHasRemoteState = false;

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Apply style */
	virtual void ApplyStyle_Implementation() override;

	/**
	 * Set the state provider
	 */
	UFUNCTION(BlueprintCallable, Category = "State")
	void SetStateProvider(TScriptInterface<IRammsStateProvider> Provider);

	/**
	 * Toggle expanded/collapsed state
	 */
	UFUNCTION(BlueprintCallable, Category = "Display")
	void ToggleExpand();

	/**
	 * Set expanded state
	 */
	UFUNCTION(BlueprintCallable, Category = "Display")
	void SetExpanded(bool bExpanded, bool bAnimated = true);

	/**
	 * Is panel expanded?
	 */
	UFUNCTION(BlueprintPure, Category = "Display")
	bool IsExpanded() const { return bIsExpanded; }

	/**
	 * Apply robot state directly to the display, bypassing the state provider.
	 * Used by the remote control bridge to set values externally.
	 */
	UFUNCTION(BlueprintCallable, Category = "Remote")
	void ApplyRemoteState(const FRammsRobotState& State);

	/**
	 * Get the current cached robot state (from last provider update or remote call).
	 */
	UFUNCTION(BlueprintCallable, Category = "Remote")
	FRammsRobotState GetCachedRobotState() const { return CachedState; }

protected:
	/** Build the widget tree programmatically */
	virtual void ResetCachedWidgets() override;
	virtual void BuildWidgetTree() override;

	/** Update display with current state */
	void UpdateDisplay();

	/** Callback when robot state updates */
	void OnRobotStateUpdate(const FRammsRobotState& State);

	/** Handle toggle button click */
	UFUNCTION()
	void OnToggleClicked();

	/** Get color for battery level */
	FLinearColor GetBatteryColor(float BatteryLevel) const;

	/** Get color for mode */
	FLinearColor GetModeColor(ERammsRobotMode Mode) const;

	/** Cached state for remote queries */
	FRammsRobotState CachedState;

	/** Apply a state snapshot to the display widgets */
	void ApplyStateToDisplay(const FRammsRobotState& State);
};
