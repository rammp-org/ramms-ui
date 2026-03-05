// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/Image.h"
#include "Components/NamedSlot.h"
#include "RammsToolbar.generated.h"

/**
 * Toolbar orientation
 */
UENUM(BlueprintType)
enum class ERammsToolbarOrientation : uint8
{
	Horizontal,
	Vertical
};

/**
 * Info about a single toolbar item
 */
USTRUCT(BlueprintType)
struct FRammsToolbarItem
{
	GENERATED_BODY()

	/** Unique item ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemID;

	/** Display text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Label;

	/** Optional tooltip */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Tooltip;

	/** Optional icon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTexture2D> Icon;

	/** Is this a toggle button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsToggle = false;

	/** Is toggled on (only if bIsToggle) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsActive = false;

	/** Is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsEnabled = true;
};

/**
 * Toolbar widget - a row/column of action buttons.
 * Supports text labels, icons, toggle states, and dynamic item management.
 * Use BindWidgetOptional pattern: create a WBP subclass to provide custom layout,
 * or use pure C++ for programmatic construction.
 */
UCLASS()
class RAMMSUI_API URammsToolbar : public URammsBaseWidget
{
	GENERATED_BODY()

protected:
	/** Toolbar orientation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toolbar")
	ERammsToolbarOrientation Orientation = ERammsToolbarOrientation::Horizontal;

	/** Items defined at design time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toolbar")
	TArray<FRammsToolbarItem> Items;

	/** Spacing between items */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (ClampMin = "0.0"))
	float ItemSpacing = 4.0f;

	// Widget references — BindWidgetOptional so a WBP can provide these
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ToolbarBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ItemContainer; // HorizontalBox or VerticalBox

	/** Named slot for additional custom content in the toolbar */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UNamedSlot> ToolbarContentSlot;

	/** Map of item ID to button widget */
	UPROPERTY()
	TMap<FName, TObjectPtr<UButton>> ItemButtons;

public:
	/** Fired when a toolbar item is clicked */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnToolbarItemClicked, FName, ItemID);
	UPROPERTY(BlueprintAssignable, Category = "Toolbar")
	FOnToolbarItemClicked OnItemClicked;

	/** Fired when a toggle item changes state */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnToolbarItemToggled, FName, ItemID, bool, bIsActive);
	UPROPERTY(BlueprintAssignable, Category = "Toolbar")
	FOnToolbarItemToggled OnItemToggled;

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void ApplyStyle_Implementation() override;
	virtual void SynchronizeProperties() override;

	/** Get the item container panel to add child widgets */
	UFUNCTION(BlueprintPure, Category = "Toolbar")
	UPanelWidget* GetItemContainer() const { return ItemContainer; }

	/** Add an item to the toolbar at runtime */
	UFUNCTION(BlueprintCallable, Category = "Toolbar")
	void AddItem(FRammsToolbarItem Item);

	/** Remove an item by ID */
	UFUNCTION(BlueprintCallable, Category = "Toolbar")
	void RemoveItem(FName ItemID);

	/** Set item enabled state */
	UFUNCTION(BlueprintCallable, Category = "Toolbar")
	void SetItemEnabled(FName ItemID, bool bEnabled);

	/** Set toggle state */
	UFUNCTION(BlueprintCallable, Category = "Toolbar")
	void SetItemActive(FName ItemID, bool bActive);

	/** Is item active (for toggle items) */
	UFUNCTION(BlueprintPure, Category = "Toolbar")
	bool IsItemActive(FName ItemID) const;

	/** Get number of items */
	UFUNCTION(BlueprintPure, Category = "Toolbar")
	int32 GetItemCount() const { return Items.Num(); }

protected:
	void BuildWidgetTree();
	void RebuildItems();
	UButton* CreateItemButton(const FRammsToolbarItem& Item);

	UFUNCTION()
	void OnButtonClicked();

	void UpdateButtonVisual(FName ItemID);
};
