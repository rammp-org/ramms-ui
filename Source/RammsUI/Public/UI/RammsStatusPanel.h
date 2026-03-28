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

// ── Status Field Data Types ────────────────────────────────────────────────

/** Where a status field reads its value from */
UENUM(BlueprintType)
enum class ERammsStatusFieldSource : uint8
{
	RobotState	  UMETA(DisplayName = "Robot State"),
	PropertyStore UMETA(DisplayName = "Property Store"),
	Custom		  UMETA(DisplayName = "Custom")
};

/** How a status field value is formatted for display */
UENUM(BlueprintType)
enum class ERammsStatusValueFormat : uint8
{
	Auto	 UMETA(DisplayName = "Auto"),
	Float	 UMETA(DisplayName = "Float"),
	Integer	 UMETA(DisplayName = "Integer"),
	Percent	 UMETA(DisplayName = "Percent"),
	Boolean	 UMETA(DisplayName = "Boolean"),
	EnumName UMETA(DisplayName = "Enum Name"),
	RawText	 UMETA(DisplayName = "Raw Text")
};

/**
 * Configuration for a single status panel row.
 * Describes what to display, where to get the data, and how to format it.
 */
USTRUCT(BlueprintType)
struct FRammsStatusField
{
	GENERATED_BODY()

	/**
	 * Lookup key. For RobotState source: "Speed", "Battery", "Mode", "EmergencyStop",
	 * "LinearVelocity", "AngularVelocity". For PropertyStore: the property key name.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status")
	FName Key;

	/** Display label shown beside the value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status")
	FText Label;

	/** Where to read the value from */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status")
	ERammsStatusFieldSource Source = ERammsStatusFieldSource::RobotState;

	/** How to format the value for display */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status")
	ERammsStatusValueFormat Format = ERammsStatusValueFormat::Auto;

	/**
	 * Enum type for EnumName format. Supports C++ and Blueprint enum assets.
	 * Used to resolve byte values into display names.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status",
		meta = (EditCondition = "Format == ERammsStatusValueFormat::EnumName"))
	TObjectPtr<UEnum> EnumType = nullptr;

	/** Units suffix (e.g. "m/s", "%") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status")
	FText Units;

	/** Decimal places for float display */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status",
		meta = (ClampMin = "0", ClampMax = "4"))
	int32 DecimalPlaces = 1;

	/** Warning threshold — value at or below this shows warning color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Thresholds")
	float WarningThreshold = 0.0f;

	/** Critical threshold — value at or below this shows error color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Thresholds")
	float CriticalThreshold = 0.0f;

	/** Whether threshold-based coloring is enabled for this field */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Thresholds")
	bool bUseThresholdColors = false;
};

// ── Status Panel Widget ────────────────────────────────────────────────────

/**
 * Data-driven collapsible status panel.
 * Configurable rows that resolve values from robot state or the property store.
 * Supports custom enums, threshold colors, and live updates.
 */
UCLASS(meta = (DisplayName = "Ramms Status Panel"))
class RAMMSUI_API URammsStatusPanel : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	URammsStatusPanel();

protected:
	/** Header title text. Leave empty to hide the header label. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FText HeaderTitle = FText::FromString(TEXT("Robot Status"));

	/** Status fields to display. Rows are created dynamically from this array. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display",
		meta = (TitleProperty = "Label"))
	TArray<FRammsStatusField> Fields;

	/** State provider (optional — can be set at runtime) */
	UPROPERTY(BlueprintReadWrite, Category = "State")
	TScriptInterface<IRammsStateProvider> StateProvider;

	/** Whether panel is expanded */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	bool bIsExpanded = true;

	/** Update frequency in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display",
		meta = (ClampMin = "0.016"))
	float UpdateFrequency = 0.1f;

	// Widget references (built programmatically)
	UPROPERTY()
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY()
	TObjectPtr<UButton> ToggleButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> HeaderRow;

	UPROPERTY()
	TObjectPtr<UVerticalBox> ContentBox;

	/** Dynamic row text blocks (one per entry in Fields) */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> FieldWidgets;

	float			TimeSinceUpdate = 0.0f;
	FDelegateHandle StateUpdateHandle;
	bool			bHasRemoteState = false;

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual void ApplyStyle_Implementation() override;
	virtual void SynchronizeProperties() override;

	UFUNCTION(BlueprintCallable, Category = "State")
	void SetStateProvider(TScriptInterface<IRammsStateProvider> Provider);

	UFUNCTION(BlueprintCallable, Category = "Display")
	void ToggleExpand();

	UFUNCTION(BlueprintCallable, Category = "Display")
	void SetExpanded(bool bExpanded, bool bAnimated = true);

	UFUNCTION(BlueprintPure, Category = "Display")
	bool IsExpanded() const { return bIsExpanded; }

	/**
	 * Apply robot state directly, bypassing the state provider.
	 * Used by the remote control bridge.
	 */
	UFUNCTION(BlueprintCallable, Category = "Remote")
	void ApplyRemoteState(const FRammsRobotState& State);

	UFUNCTION(BlueprintCallable, Category = "Remote")
	FRammsRobotState GetCachedRobotState() const { return CachedState; }

	/**
	 * Set a specific field's value directly from Blueprint.
	 * Only applies to fields with Source == Custom.
	 */
	UFUNCTION(BlueprintCallable, Category = "Display")
	void SetCustomFieldValue(FName Key, const FString& Value);

protected:
	virtual void ResetCachedWidgets() override;
	virtual void BuildWidgetTree() override;

	/** Build or rebuild the data-driven content rows */
	void BuildFieldRows();

	/** Update all field displays */
	void UpdateDisplay();

	void OnRobotStateUpdate(const FRammsRobotState& State);

	UFUNCTION()
	void OnSubsystemStateChanged(const FRammsRobotState& State);

	UFUNCTION()
	void OnSubsystemPropertyChanged(FName Key, const FString& Value);

	UFUNCTION()
	void OnToggleClicked();

	FLinearColor GetBatteryColor(float BatteryLevel) const;
	FLinearColor GetModeColor(ERammsRobotMode Mode) const;

	FRammsRobotState CachedState;

	/** Manually set values for Custom-source fields */
	TMap<FName, FString> CustomFieldValues;

	/** Refresh all field row texts and colors from current data */
	void RefreshAllFields();
};
