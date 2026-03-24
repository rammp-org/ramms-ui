// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/RammsBaseWidget.h"
#include "RammsLayoutPresetAsset.generated.h"

/**
 * Describes how a single widget should be placed within a layout preset.
 * Uses normalized (0-1) coordinates for resolution independence.
 */
USTRUCT(BlueprintType)
struct RAMMSUI_API FRammsLayoutPresetEntry
{
	GENERATED_BODY()

	/**
	 * Tag used to match this entry to a widget at runtime.
	 * Set a matching tag on the widget via WidgetTag property.
	 * Example: "CameraMain", "ArmTaskSelector", "StatusPanel"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FName WidgetTag;

	/**
	 * Widget class to create if no existing widget with WidgetTag is found.
	 * Leave null if the widget is always created externally.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	TSubclassOf<URammsBaseWidget> WidgetClass;

	/** Target position (0-1 normalized, top-left origin) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FVector2D Position = FVector2D::ZeroVector;

	/** Target size (0-1 normalized) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FVector2D Size = FVector2D(0.5f, 0.5f);

	/** Z-order (higher values draw on top) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0"))
	int32 ZOrder = 0;

	/** Whether this widget should be visible in this layout */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bVisible = true;
};

/**
 * Data asset defining a named layout preset.
 *
 * Create instances in the editor (right-click → Miscellaneous → Data Asset →
 * RammsLayoutPresetAsset) and assign them to URammsLayoutManager.
 *
 * Each preset defines which widgets to show, where to place them, and in
 * what z-order. The layout manager resolves entries by WidgetTag, matching
 * against registered widgets. If a WidgetClass is specified and no matching
 * widget exists, the manager can optionally auto-create it.
 *
 * Example presets:
 *   - System Overview: mostly 3D viewport with small status overlays
 *   - Base Centered: sensor streams, base cameras, navigation data
 *   - Arm Centered: arm task selection, arm pose, wrist camera streams
 */
UCLASS(BlueprintType)
class RAMMSUI_API URammsLayoutPresetAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Human-readable name for this preset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout Preset")
	FText PresetDisplayName;

	/** Description of the preset's purpose */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout Preset", meta = (MultiLine = true))
	FText PresetDescription;

	/** Widget entries that define this layout */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout Preset")
	TArray<FRammsLayoutPresetEntry> Entries;

	/** Transition duration override (-1 to use the manager's default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout Preset", meta = (ClampMin = "-1.0"))
	float TransitionDurationOverride = -1.0f;
};
