// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/RammsBaseWidget.h"
#include "UI/RammsLayoutPresetAsset.h"
#include "RammsLayoutManager.generated.h"

/**
 * Layout preset configurations
 */
UENUM(BlueprintType)
enum class ERammsLayoutPreset : uint8
{
	/** Single fullscreen widget */
	SingleFullscreen,

	/** Multiple widgets in grid */
	Grid,

	/** Picture-in-picture (one large, others in corners) */
	PictureInPicture,

	/** Side-by-side split */
	SideBySide,

	/** Custom (user-defined positions) */
	Custom
};

/**
 * Widget layout info
 */
USTRUCT(BlueprintType)
struct FRammsWidgetLayout
{
	GENERATED_BODY()

	/** Widget to manage */
	UPROPERTY()
	TObjectPtr<URammsBaseWidget> Widget;

	/** Tag for matching against preset entries */
	UPROPERTY()
	FName WidgetTag;

	/** Target position (0-1 normalized) */
	UPROPERTY()
	FVector2D TargetPosition = FVector2D::ZeroVector;

	/** Target size (0-1 normalized) */
	UPROPERTY()
	FVector2D TargetSize = FVector2D(0.5f, 0.5f);

	/** Z-order (higher = on top) */
	UPROPERTY()
	int32 ZOrder = 0;

	/** Is widget visible */
	UPROPERTY()
	bool bVisible = true;
};

/**
 * Layout manager component for organizing and animating UI widgets
 * Attach to player controller or game mode
 */
UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class RAMMSUI_API URammsLayoutManager : public UActorComponent
{
	GENERATED_BODY()

protected:
	/** Registered widgets */
	UPROPERTY()
	TArray<FRammsWidgetLayout> ManagedWidgets;

	/** Current layout preset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	ERammsLayoutPreset CurrentPreset = ERammsLayoutPreset::Custom;

	/** Animation duration for layout transitions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0.0"))
	float TransitionDuration = 0.5f;

	/** Grid layout: columns */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Layout", meta = (ClampMin = "1"))
	int32 GridColumns = 2;

	/** Grid layout: rows */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Layout", meta = (ClampMin = "1"))
	int32 GridRows = 2;

	/** Spacing between widgets (pixels) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float WidgetSpacing = 10.0f;

public:
	URammsLayoutManager();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Add a widget to be managed
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void AddWidget(URammsBaseWidget* Widget, int32 ZOrder = 0);

	/**
	 * Remove a widget from management
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void RemoveWidget(URammsBaseWidget* Widget);

	/**
	 * Clear all managed widgets
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void ClearWidgets();

	/**
	 * Transition to a layout preset
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void TransitionTo(ERammsLayoutPreset Preset, bool bAnimated = true);

	/**
	 * Set custom position for a widget (normalized 0-1)
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void SetWidgetPosition(URammsBaseWidget* Widget, FVector2D Position, FVector2D Size, bool bAnimated = true);

	/**
	 * Bring widget to front
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void BringToFront(URammsBaseWidget* Widget);

	/**
	 * Send widget to back
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void SendToBack(URammsBaseWidget* Widget);

	/**
	 * Get number of managed widgets
	 */
	UFUNCTION(BlueprintPure, Category = "Layout")
	int32 GetWidgetCount() const { return ManagedWidgets.Num(); }

	// ── Data Asset Preset Support ────────────────────────────────

	/**
	 * Transition to a layout defined by a data asset preset.
	 * Resolves entries by WidgetTag against registered widgets.
	 * Widgets not referenced by the preset are hidden.
	 * @param PresetAsset - The layout preset data asset
	 * @param bAnimated - Whether to animate the transition
	 * @param bAutoCreateWidgets - If true, auto-create widgets for entries with a WidgetClass but no matching tag
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void TransitionToPreset(URammsLayoutPresetAsset* PresetAsset, bool bAnimated = true, bool bAutoCreateWidgets = false);

	/**
	 * Get the currently active preset asset (null if using built-in presets)
	 */
	UFUNCTION(BlueprintPure, Category = "Layout")
	URammsLayoutPresetAsset* GetActivePresetAsset() const { return ActivePresetAsset; }

	/**
	 * Set a tag on a managed widget so it can be resolved by preset entries.
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void SetWidgetTag(URammsBaseWidget* Widget, FName Tag);

	/**
	 * Find a managed widget by its tag.
	 * @return The first widget with the matching tag, or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	URammsBaseWidget* FindWidgetByTag(FName Tag) const;

	/** Fired when transitioning to a new preset */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPresetChanged, URammsLayoutPresetAsset*, NewPreset);
	UPROPERTY(BlueprintAssignable, Category = "Layout")
	FOnPresetChanged OnPresetChanged;

protected:
	/** Apply layout based on current preset */
	void ApplyLayout(bool bAnimated);

	/** Apply fullscreen layout */
	void ApplyFullscreenLayout(bool bAnimated);

	/** Apply grid layout */
	void ApplyGridLayout(bool bAnimated);

	/** Apply picture-in-picture layout */
	void ApplyPIPLayout(bool bAnimated);

	/** Apply side-by-side layout */
	void ApplySideBySideLayout(bool bAnimated);

	/** Update Z-order of all widgets */
	void UpdateZOrder();

	/** Currently active preset asset (null if using built-in presets) */
	UPROPERTY()
	TObjectPtr<URammsLayoutPresetAsset> ActivePresetAsset;
};
