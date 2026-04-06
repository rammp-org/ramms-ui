// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/RammsBaseWidget.h"
#include "UI/RammsLayoutBase.h"
#include "UI/RammsUIStyle.h"
#include "RammsLayoutHost.generated.h"

class UOverlay;

/**
 * Detected display orientation based on viewport aspect ratio.
 */
UENUM(BlueprintType)
enum class ERammsOrientation : uint8
{
	Landscape,
	Portrait
};

/**
 * Orientation override mode for the layout host.
 */
UENUM(BlueprintType)
enum class ERammsOrientationOverride : uint8
{
	/** Detect automatically from viewport aspect ratio */
	Auto,
	/** Always treat as landscape */
	ForceLandscape,
	/** Always treat as portrait */
	ForcePortrait
};

/**
 * Entry in the widget pool: a shared widget identified by a tag.
 * The LayoutHost injects pool widgets into layout NamedSlots by matching
 * WidgetTag to slot name.
 */
USTRUCT(BlueprintType)
struct RAMMSUI_API FRammsPoolEntry
{
	GENERATED_BODY()

	/** Tag matching a NamedSlot name in layouts (e.g., "CameraMain") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget Pool")
	FName WidgetTag;

	/** The shared widget instance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget Pool")
	TObjectPtr<URammsBaseWidget> Widget;
};

/**
 * Root host widget that manages layout transitions using an Overlay.
 *
 * ## Architecture
 *
 *   LayoutHost (added to viewport once)
 *   └─ Overlay
 *      ├─ WBP_SystemOverview  (URammsLayoutBase subclass)
 *      ├─ WBP_BaseCentered    (URammsLayoutBase subclass)
 *      └─ WBP_ArmCentered    (URammsLayoutBase subclass)
 *
 * Each layout is a URammsLayoutBase subclass with NamedSlots.
 * Functional widgets (camera, arm tasks, status) live in a **widget pool**
 * and are reparented into the active layout's slots on transition.
 *
 * All layouts are direct children of the Overlay, stacked on top of each
 * other. Visibility is controlled per-layout: inactive layouts are Collapsed,
 * the active layout is SelfHitTestInvisible with full opacity.
 *
 * ## Usage
 *
 * 1. Create layout Blueprint widgets inheriting URammsLayoutBase
 * 2. Create/configure the LayoutHost (C++ or Blueprint)
 * 3. Register layouts: AddLayout(MyLayoutClass, "SystemOverview")
 * 4. Register pool widgets: AddPoolWidget("CameraMain", MyCameraWidget)
 * 5. Transition: TransitionToLayout("SystemOverview") or by index
 *
 * ## Transition Animation
 *
 * True crossfade: the outgoing layout fades out (opacity 1→0) while the
 * incoming layout fades in (opacity 0→1). Both are visible simultaneously
 * in the Overlay during the transition. Pool widgets are reparented into the
 * incoming layout at the start of the transition. Hit-testing on both
 * layouts is disabled during the crossfade and restored on the incoming
 * layout when the transition completes.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Ramms Layout Host"))
class RAMMSUI_API URammsLayoutHost : public UUserWidget
{
	GENERATED_BODY()

public:
	URammsLayoutHost(const FObjectInitializer& ObjectInitializer);

	// ── Layout Registration ──────────────────────────────────────

	/**
	 * Add a layout by class. Instantiates the widget and adds it to the switcher.
	 * @param LayoutClass - Must be a subclass of URammsLayoutBase
	 * @param LayoutName - Unique name for this layout (used in TransitionToLayout)
	 * @return The created layout instance, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout Host")
	URammsLayoutBase* AddLayout(TSubclassOf<URammsLayoutBase> LayoutClass, FName LayoutName);

	/**
	 * Add an already-created layout widget instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout Host")
	void AddLayoutInstance(URammsLayoutBase* Layout, FName LayoutName);

	/**
	 * Get a registered layout by name.
	 */
	UFUNCTION(BlueprintPure, Category = "Layout Host")
	URammsLayoutBase* GetLayout(FName LayoutName) const;

	/**
	 * Get the currently active layout.
	 */
	UFUNCTION(BlueprintPure, Category = "Layout Host")
	URammsLayoutBase* GetActiveLayout() const;

	/**
	 * Get the name of the currently active layout.
	 */
	UFUNCTION(BlueprintPure, Category = "Layout Host")
	FName GetActiveLayoutName() const { return ActiveLayoutName; }

	/**
	 * Get names of all registered layouts.
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout Host")
	TArray<FName> GetLayoutNames() const;

	// ── Widget Pool ──────────────────────────────────────────────

	/**
	 * Register a shared widget in the pool.
	 * The tag must match a NamedSlot name in your layouts.
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout Host|Pool")
	void AddPoolWidget(FName WidgetTag, URammsBaseWidget* Widget);

	/**
	 * Remove a widget from the pool (does not destroy it).
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout Host|Pool")
	void RemovePoolWidget(FName WidgetTag);

	/**
	 * Get a pool widget by tag.
	 */
	UFUNCTION(BlueprintPure, Category = "Layout Host|Pool")
	URammsBaseWidget* GetPoolWidget(FName WidgetTag) const;

	/**
	 * Get all pool entries.
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout Host|Pool")
	TArray<FRammsPoolEntry> GetPoolEntries() const { return WidgetPool; }

	// ── Transitions ──────────────────────────────────────────────

	/**
	 * Transition to a layout by name with optional crossfade animation.
	 * @param LayoutName - Must match a name passed to AddLayout()
	 * @param bAnimated - Whether to crossfade (true) or instant-switch (false)
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout Host")
	void TransitionToLayout(FName LayoutName, bool bAnimated = true);

	/**
	 * Transition to a layout by index in registration order.
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout Host")
	void TransitionToLayoutIndex(int32 Index, bool bAnimated = true);

	/** Crossfade duration in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout Host|Transition", meta = (ClampMin = "0.0"))
	float CrossfadeDuration = 0.3f;

	/** Style to propagate to all layouts and pool widgets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout Host|Style")
	TObjectPtr<URammsUIStyle> Style;

	// ── Delegates ────────────────────────────────────────────────

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLayoutTransition, FName, NewLayoutName, FName, PreviousLayoutName);

	/** Fired when a layout transition begins */
	UPROPERTY(BlueprintAssignable, Category = "Layout Host")
	FOnLayoutTransition OnLayoutTransitionStarted;

	/** Fired when a layout transition completes */
	UPROPERTY(BlueprintAssignable, Category = "Layout Host")
	FOnLayoutTransition OnLayoutTransitionCompleted;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOrientationChanged, ERammsOrientation, NewOrientation);

	/** Fired when the detected orientation changes */
	UPROPERTY(BlueprintAssignable, Category = "Layout Host|Orientation")
	FOnOrientationChanged OnOrientationChanged;

	// ── Orientation ─────────────────────────────────────────────

	/** Override orientation detection (Auto = detect from viewport aspect ratio) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout Host|Orientation")
	ERammsOrientationOverride OrientationOverride = ERammsOrientationOverride::Auto;

	/**
	 * Explicit mapping of landscape layout names to portrait variants.
	 * Example: { "3D" → "3D_Portrait", "Arm" → "Arm_Portrait" }
	 * If a layout is not in this map, suffix fallback is attempted
	 * (e.g., "3D" → "3D_Portrait" or "3D_Landscape" → "3D_Portrait").
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout Host|Orientation")
	TMap<FName, FName> PortraitLayoutMap;

	/** Whether to animate orientation-triggered transitions (default: true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout Host|Orientation")
	bool bAnimateOrientationTransition = true;

	/** Set orientation override at runtime */
	UFUNCTION(BlueprintCallable, Category = "Layout Host|Orientation")
	void SetOrientationOverride(ERammsOrientationOverride Override);

	/** Get the current detected orientation */
	UFUNCTION(BlueprintPure, Category = "Layout Host|Orientation")
	ERammsOrientation GetCurrentOrientation() const { return CurrentOrientation; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Build the root widget tree (Overlay → WidgetSwitcher) */
	void BuildHostTree();

private:
	/** Root overlay — layouts are direct children, stacked for crossfade */
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> RootOverlay;

	/** Registered layouts: name → instance */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<URammsLayoutBase>> LayoutMap;

	/** Ordered list of layout names */
	UPROPERTY(Transient)
	TArray<FName> LayoutOrder;

	/** Name of the active layout */
	FName ActiveLayoutName;

	/** Shared widget pool */
	UPROPERTY()
	TArray<FRammsPoolEntry> WidgetPool;

	// ── Crossfade State ──────────────────────────────────────────

	bool  bTransitioning = false;
	float TransitionAlpha = 0.0f;
	FName TransitionFromName;
	FName TransitionToName;

	/** Perform the actual reparenting of pool widgets into the target layout */
	void InjectPoolWidgets(URammsLayoutBase* Layout);

	/** Remove all pool widgets from their current slots */
	void ExtractPoolWidgets();

	/** Complete the transition (set final visibility, fire delegate) */
	void FinishTransition();

	/** Handler for UISubsystem layout transition requests */
	UFUNCTION()
	void HandleLayoutTransitionRequest(FName LayoutName, bool bAnimated);

	/** Whether we are subscribed to the subsystem event */
	bool bSubscribedToTransitionRequests = false;

	// ── Orientation State ────────────────────────────────────────

	ERammsOrientation CurrentOrientation = ERammsOrientation::Landscape;
	bool			  bOrientationInitialized = false;

	/** Resolve the effective orientation considering the override */
	ERammsOrientation ComputeEffectiveOrientation() const;

	/** Check viewport and handle orientation change if needed */
	void UpdateOrientationCheck();

	/**
	 * Resolve the layout name to use for the given orientation.
	 * Uses PortraitLayoutMap first, then suffix fallback (_Portrait / _Landscape).
	 */
	FName ResolveLayoutForOrientation(FName BaseName, ERammsOrientation Orientation) const;

	/**
	 * Given a layout name that may be orientation-specific, find the "base" name.
	 * Strips _Portrait / _Landscape suffix, or does reverse lookup in PortraitLayoutMap.
	 */
	FName GetBaseLayoutName(FName LayoutName) const;
};
