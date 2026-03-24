// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "RammsLayoutBase.generated.h"

class URammsLayoutHost;
class UPanelWidget;
class UNamedSlot;

/**
 * Abstract base class for layout widgets used with URammsLayoutHost.
 *
 * Each layout defines a visual arrangement with named injection slots where
 * pool widgets get placed at runtime.  Create Blueprint subclasses using the
 * UMG Designer — place a root Canvas Panel, add child widgets (SizeBox,
 * Border, etc.) with stretch anchors for responsive sizing, and drop
 * container widgets where functional widgets should be injected.
 *
 * ## Injection Containers
 *
 * The following widget types are auto-discovered as injection slots:
 *
 * - **NamedSlot** — standard UMG slot widget (recommended for Blueprints)
 * - **Overlay** — good for runtime child management
 * - **SizeBox** — good when you want size constraints on injected content
 *
 * Name each container to match the WidgetTag used by the LayoutHost's pool:
 *
 *   "CameraMain", "ArmTaskSelector", "StatusPanel", "TaskControls", etc.
 *
 * When the LayoutHost transitions to this layout, it matches pool widgets
 * to slots by tag name and injects them automatically.
 *
 * ## Creating a Layout (Blueprint workflow)
 *
 * 1. Create a Widget Blueprint inheriting from RammsLayoutBase
 * 2. In the Designer, build your layout with proper stretch anchors
 * 3. Add NamedSlot (or Overlay) widgets and name them to match pool tags
 * 4. Override GetLayoutSlotNames() if you build slots programmatically
 *
 * ## Creating a Layout (C++ workflow)
 *
 * 1. Subclass URammsLayoutBase
 * 2. Override BuildWidgetTree() to create panels with anchored containers
 * 3. Create slot widgets and register them via RegisterSlot()
 * 4. Override GetLayoutSlotNames() to return available slot names
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Ramms Layout Base"))
class RAMMSUI_API URammsLayoutBase : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	/**
	 * Get the names of all slots this layout provides.
	 * The LayoutHost uses these to decide which pool widgets to inject.
	 * (Named GetLayoutSlotNames to avoid hiding UUserWidget::GetSlotNames)
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Layout")
	TArray<FName>		  GetLayoutSlotNames() const;
	virtual TArray<FName> GetLayoutSlotNames_Implementation() const;

	/**
	 * Get a slot container by its tag name.
	 * Returns nullptr if the slot doesn't exist or hasn't been built yet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	UPanelWidget* GetSlot(FName SlotName) const;

	/**
	 * Inject a widget into the named slot, replacing any existing content.
	 * Works with NamedSlot, Overlay, and SizeBox containers.
	 * @return true if the slot was found and the widget was injected
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	bool InjectWidget(FName SlotName, UWidget* Widget);

	/**
	 * Remove any widget currently in the named slot.
	 * Does NOT destroy the widget — just removes it from the slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void ClearSlot(FName SlotName);

	/** Clear all slots (called by LayoutHost before reparenting) */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void ClearAllSlots();

	/** Human-readable name for this layout (shown in UI or debug) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FText LayoutDisplayName;

	// ── Host Reference & Transition Requests ─────────────────────

	/**
	 * Get the LayoutHost that owns this layout (set automatically on registration).
	 * Returns nullptr if not yet registered with a host.
	 */
	UFUNCTION(BlueprintPure, Category = "Layout")
	URammsLayoutHost* GetOwningHost() const;

	/**
	 * Convenience: request a transition to another layout.
	 * Routes through the UI event bus so any subscriber (including the host)
	 * can respond. Works from any widget — not just layouts.
	 * @param LayoutName - Target layout name
	 * @param bAnimated - Whether to crossfade
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void RequestLayoutTransition(FName LayoutName, bool bAnimated = true);

protected:
	/**
	 * Register a container widget as an injection slot.
	 * Call this from BuildWidgetTree() for programmatically created slots.
	 * Blueprint-placed containers are auto-discovered in NativeOnInitialized.
	 */
	void RegisterSlot(FName SlotName, UPanelWidget* Container);

	virtual void NativeOnInitialized() override;

private:
	/** Map of slot name → container widget (NamedSlot, Overlay, or SizeBox) */
	UPROPERTY()
	TMap<FName, TObjectPtr<UPanelWidget>> SlotMap;

	/** Weak back-reference to the owning host (set by LayoutHost on registration) */
	TWeakObjectPtr<URammsLayoutHost> OwningHost;

	/**
	 * Auto-discover injection containers in the widget tree.
	 * Finds NamedSlot, Overlay, and SizeBox widgets and registers them by name.
	 */
	void DiscoverSlots();

	/** Allow LayoutHost to set the back-reference */
	friend class URammsLayoutHost;
	void SetOwningHost(URammsLayoutHost* Host);
};
