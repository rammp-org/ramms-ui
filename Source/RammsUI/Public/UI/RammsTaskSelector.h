// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "RammsTaskSelector.generated.h"

class URammsImageButton;
class UTexture2D;

/**
 * Maps an enum value to a button icon texture.
 * Used in URammsTaskSelector to assign icons to auto-generated enum entries.
 */
USTRUCT(BlueprintType)
struct RAMMSUI_API FRammsTaskIconMapping
{
	GENERATED_BODY()

	/** Enum value to map (underlying value from TaskEnum). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	int64 EnumValue = 0;

	/** Icon texture for the button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	TObjectPtr<UTexture2D> Icon;
};

/**
 * Defines a single task entry in a URammsTaskSelector.
 */
USTRUCT(BlueprintType)
struct RAMMSUI_API FRammsTaskDefinition
{
	GENERATED_BODY()

	/** Enum value this task maps to (underlying value from the TaskEnum). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	int64 EnumValue = 0;

	/** Display label.  If empty and a TaskEnum is set, the enum's display name is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FText Label;

	/** Button image.  Null shows a placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	TObjectPtr<UTexture2D> Image;

	/** Whether this task button is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	bool bEnabled = true;
};

/**
 * Data-driven task button grid with enum binding and mutual exclusion.
 *
 * Assign a Blueprint enum (TaskEnum) and the widget auto-generates one
 * URammsImageButton per enum value.  Manual FRammsTaskDefinition entries
 * override auto-generated ones (matched by EnumValue) for custom images,
 * labels, or enabled state.
 *
 * Layout is configurable: horizontal or vertical, with optional wrapping
 * via MaxPerRow.  Exactly one task may be active at a time (radio-group
 * behaviour).  bAllowDeselect controls whether clicking the active task
 * deselects it.
 */
UCLASS(meta = (DisplayName = "Ramms Task Selector"))
class RAMMSUI_API URammsTaskSelector : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	// ── Configuration ────────────────────────────────────────────

	/** The Blueprint enum that defines valid task values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tasks")
	TObjectPtr<UEnum> TaskEnum;

	/** Ordered list of task entries.  When bAutoGenerateFromEnum is true,
	 *  entries here override auto-generated ones (matched by EnumValue). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tasks")
	TArray<FRammsTaskDefinition> Tasks;

	/** When true, auto-populate buttons from every enum value.  Manual
	 *  Tasks entries override the auto-generated label/image/enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tasks")
	bool bAutoGenerateFromEnum = true;

	/** Per-enum-value icon overrides.  When bAutoGenerateFromEnum is true,
	 *  these assign icons to auto-generated entries without needing a full
	 *  FRammsTaskDefinition override.  Full Tasks entries take priority. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tasks",
		meta = (EditCondition = "bAutoGenerateFromEnum"))
	TArray<FRammsTaskIconMapping> IconOverrides;

	/** Allow clicking the active task to deselect it (nothing selected). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tasks")
	bool bAllowDeselect = true;

	// ── Layout ───────────────────────────────────────────────────

	/** Primary layout axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	TEnumAsByte<EOrientation> Orientation = Orient_Horizontal;

	/** Max buttons per row (horizontal) or column (vertical) before wrapping.
	 *  0 = single line, no wrapping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0"))
	int32 MaxPerRow = 0;

	/** Shared image size for all buttons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FVector2D ButtonImageSize = FVector2D(64.0, 64.0);

	/** Spacing between buttons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0"))
	float ButtonSpacing = 8.0f;

	/** When true, all buttons are sized uniformly (same width and height).
	 *  The size is determined by ButtonFixedSize if non-zero, otherwise by
	 *  the largest button's natural size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bUniformButtonSize = true;

	/** Fixed button size (width, height).  0 on either axis means auto
	 *  (use largest button's natural size on that axis).  Only used when
	 *  bUniformButtonSize is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout",
		meta = (EditCondition = "bUniformButtonSize"))
	FVector2D ButtonFixedSize = FVector2D::ZeroVector;

	/** Enable text wrapping on button labels.  When true, labels auto-wrap
	 *  within the button width instead of expanding the button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bWrapLabelText = true;

	/** Show labels on all buttons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bShowLabels = true;

	/** Padding around the button content area inside the background border. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float ContentPadding = 4.0f;

	// ── State ────────────────────────────────────────────────────

	/** Currently selected enum value.  INDEX_NONE (-1) = nothing selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tasks")
	int64 SelectedValue = INDEX_NONE;

	// ── Delegates ────────────────────────────────────────────────

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaskSelected, int64, EnumValue);

	/** Fires when a task button is selected. */
	UPROPERTY(BlueprintAssignable, Category = "Tasks")
	FOnTaskSelected OnTaskSelected;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTaskDeselected);

	/** Fires when the active task is deselected (bAllowDeselect mode). */
	UPROPERTY(BlueprintAssignable, Category = "Tasks")
	FOnTaskDeselected OnTaskDeselected;

	// ── Public API ───────────────────────────────────────────────

	/** Programmatically select a task by enum value. */
	UFUNCTION(BlueprintCallable, Category = "Tasks")
	void SelectTask(int64 EnumValue);

	/** Clear the current selection. */
	UFUNCTION(BlueprintCallable, Category = "Tasks")
	void ClearSelection();

	/** Enable or disable a specific task by enum value. */
	UFUNCTION(BlueprintCallable, Category = "Tasks")
	void SetTaskEnabled(int64 EnumValue, bool bEnabled);

	/** Rebuild all buttons from the current Tasks array / enum. */
	UFUNCTION(BlueprintCallable, Category = "Tasks")
	void RebuildButtons();

	// ── Lifecycle ────────────────────────────────────────────────

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void SynchronizeProperties() override;
	virtual void ApplyStyle_Implementation() override;

protected:
	virtual void	 BuildWidgetTree() override;
	virtual void	 ResetCachedWidgets() override;
	virtual UWidget* GetRootWidgetForValidation() override { return RootBorder; }

private:
	/** Merge auto-generated enum entries with manual overrides. */
	TArray<FRammsTaskDefinition> BuildMergedTaskList() const;

	/** Create or recreate the layout container based on Orientation/MaxPerRow. */
	void RebuildContainer();

	/** Handle a child ImageButton click. */
	UFUNCTION()
	void OnButtonClicked();

	/** Resolve the enum value for a clicked button. */
	int64 GetEnumValueForButton(URammsImageButton* Button) const;

	// ── Cached widgets (Transient — rebuilt programmatically) ───

	UPROPERTY(Transient)
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY(Transient)
	TObjectPtr<UPanelWidget> ButtonContainer;

	/** Map EnumValue → ImageButton for fast lookup. */
	UPROPERTY(Transient)
	TMap<int64, TObjectPtr<URammsImageButton>> ButtonMap;
};
