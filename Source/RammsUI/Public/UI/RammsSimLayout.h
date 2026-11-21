// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsLayoutBase.h"
#include "RammsControlTypes.h"
#include "RammsSimLayout.generated.h"

class UBorder;
class UCanvasPanel;
class UHorizontalBox;
class UOverlay;
class URammsButton;
class URammsFloatingPanel;
class URammsSimLayout;

/** One of the panel tray's buttons: shows, restores, focuses or hides a
 *  panel (or the joystick, or every panel). */
UCLASS()
class RAMMSUI_API URammsPanelTrayEntry : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<URammsFloatingPanel> Panel;
	TWeakObjectPtr<URammsSimLayout>		Layout;
	/** Sim look: an Action control this entry triggers (URammsControlHUDSettings::TrayControls); None for a panel's entry. */
	FName Control;
	/** Not a panel: "Joystick", or "All" (hide / show every panel). */
	FName Special;
	/** Its words; a panel's or the joystick's get a mark for their state. */
	FText Label;

	UPROPERTY(Transient)
	TObjectPtr<URammsButton> Button;

	UFUNCTION()
	void HandleClicked();

	/** The button's look for its panel's state now. */
	void Refresh();
};

/**
 * The simulator's default layout: the control-surface panel, the camera
 * capture panel, a drive joystick bottom-left, and any panel a component on
 * the controlled pawn offers (IRammsHUDPanelProvider) -- the curb controller's
 * debug graphs, for one. Slots (pool tags): "SurfacePanel", "Joystick",
 * "Status", "CameraCapturePanel". Built in code so no Blueprint asset is
 * needed; subclass in Blueprint to restyle.
 *
 * With bFloatingPanels (the default; URammsControlHUDSettings) each panel
 * floats in a URammsFloatingPanel the user moves, resizes, minimises, hides and
 * pops out into a window of its own, its place kept per user; a panel tray at
 * the top brings hidden ones back, hides or shows them all, and resets the
 * layout. Off, the panels sit in the fixed columns of before.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Ramms Sim Layout"))
class RAMMSUI_API URammsSimLayout : public URammsLayoutBase
{
	GENERATED_BODY()

public:
	/** Fraction of the viewport width the right-hand panel column takes
	 *  (fixed layout). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0.15", ClampMax = "0.5"))
	float PanelColumnFraction = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FVector2D JoystickAreaSize = FVector2D(200.0f, 200.0f);

	/** Width of the camera capture column, in pixels (fixed layout). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "120.0"))
	float CameraPanelWidth = 380.0f;

	/** Panels float (and URammsControlHUDSettings::bFloatingPanels agrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bFloatingPanels = true;

	/** How far floating panels keep from the viewport's edges (Slate units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0.0"))
	float FloatingMargin = 8.0f;

	virtual TArray<FName> GetLayoutSlotNames_Implementation() const override;

	UFUNCTION(BlueprintPure, Category = "Layout|Floating")
	TArray<URammsFloatingPanel*> GetFloatingPanels() const;

	UFUNCTION(BlueprintPure, Category = "Layout|Floating")
	URammsFloatingPanel* FindFloatingPanel(FName PanelId) const;

	/** Every panel back where it first appeared, shown and docked; the
	 *  stored layout forgotten (ramms.UI.ResetLayout). */
	UFUNCTION(BlueprintCallable, Category = "Layout|Floating")
	void ResetPanelLayout();

	/** Hide or show every panel and the joystick at once (ramms.UI.Panels). */
	UFUNCTION(BlueprintCallable, Category = "Layout|Floating")
	void SetAllPanelsHidden(bool bHidden);

	UFUNCTION(BlueprintPure, Category = "Layout|Floating")
	bool AreAllPanelsHidden() const;

	UFUNCTION(BlueprintCallable, Category = "Layout|Floating")
	void SetJoystickHidden(bool bHidden);

	UFUNCTION(BlueprintPure, Category = "Layout|Floating")
	bool IsJoystickHidden() const { return bJoystickHidden; }

	bool UsesFloatingPanels() const { return RootCanvas != nullptr && bFloating; }

	// --- Sim look: status strip, prompts, tray actions ---------------------------

	/** Trigger an Action control on the robot the HUD's panel is showing (a tray button's click).
	 *  Does nothing outside the Sim look's floating layout, which is where the strip and tray live. */
	UFUNCTION(BlueprintCallable, Category = "Layout|Sim Look")
	void TriggerControl(FName ControlId);

	/** What the status strip shows, a "Name: value" per chip (empty outside the Sim look). */
	UFUNCTION(BlueprintPure, Category = "Layout|Sim Look")
	TArray<FString> GetStatusChips() const;

	/** The prompts on show, in the order the robot lists their controls (empty outside the Sim look). One
	 *  the robot has just cleared counts until it has faded out (URammsControlHUDSettings::PromptMinSeconds). */
	UFUNCTION(BlueprintPure, Category = "Layout|Sim Look")
	TArray<FString> GetPrompts() const;

protected:
	virtual void BuildWidgetTree() override;
	virtual void ResetCachedWidgets() override;
	virtual void ApplyStyle_Implementation() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	void				 BuildFixed(UCanvasPanel* Root);
	void				 BuildFloating(UCanvasPanel* Root);
	URammsFloatingPanel* AddFloatingPanel(FName Id, const FText& Title, const FVector2D& DefaultPlacement, bool bResizable);
	void				 PollProviders();
	void				 RebuildTray();

	UFUNCTION()
	void HandlePanelChanged(URammsFloatingPanel* Panel);
	UFUNCTION()
	void HandleResetClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient)
	TArray<TObjectPtr<URammsFloatingPanel>> FloatingPanels;
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> JoystickSlot;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> TrayBorder;
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> TrayRow;
	UPROPERTY(Transient)
	TArray<TObjectPtr<URammsPanelTrayEntry>> TrayEntries;
	UPROPERTY(Transient)
	TObjectPtr<URammsButton> ResetButton;

	// --- Sim look ---------------------------------------------------------------

	/** Read the robot's surface for the status strip and the prompts; rebuild them when it changes. */
	void PollStatus();
	void RebuildStatus(UObject* Surface);
	/** Show the drive stick unless the user hid it or the robot has nothing for it to drive. */
	void ApplyJoystickVisibility();
	void StyleStatus();
	/** Fade the prompts in and out, and keep one that came and went up for long enough to read. */
	void TickPrompts(float DeltaSeconds);

	/** Where a prompt is in its life: what the robot says now, and how its frame is drawn. */
	struct FPromptState
	{
		/** Severity the robot reports now (ERammsControlTextSeverity); 0 = it has nothing to say. */
		uint8 Wanted = 0;
		/** Severity the frame is drawn for; stays through the hold and the fade after the robot goes quiet. */
		uint8 Drawn = 0;
		/** 0 = hidden .. 1 = fully on show. */
		float Alpha = 0.0f;
		/** How long the message on show has been up, from when it (not its numbers) last changed. */
		float ShownSeconds = 0.0f;
	};

	UPROPERTY(Transient)
	TObjectPtr<UBorder> StatusBorder;
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> StatusRow;
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UTextBlock>> StatusNames;
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UTextBlock>> StatusValues;
	UPROPERTY(Transient)
	TObjectPtr<class UVerticalBox> PromptBox;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> PromptFrames;
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UTextBlock>> PromptTexts;

	/** The controls behind the chips and the prompts, in step with the arrays above. */
	TArray<FRammsControlAxis> StatusAxes;
	TArray<FRammsControlAxis> PromptAxes;
	TArray<FPromptState>	  PromptStates;
	/** Tray controls the robot has, for RebuildTray. */
	TArray<FRammsControlAxis> TrayAxes;

	TWeakObjectPtr<UObject> StatusSurface;
	/** The input source the panel acts as, used for the tray's buttons too. */
	ERammsControlSource StatusSource = ERammsControlSource::Touch;
	int32				StatusVersion = -1;
	float				StatusPollSeconds = 0.0f;

	/** Panels offered by components (IRammsHUDPanelProvider): who offered
	 *  each, and whether it wants it shown. */
	TMap<FName, TWeakObjectPtr<UObject>> ProviderOf;
	TMap<FName, TFunction<bool()>>		 ProviderWanted;

	bool  bFloating = false;
	bool  bJoystickHidden = false;
	/** Sim look: the robot on show has neither of the drive stick's controls, so the stick is left out. */
	bool  bStickUnused = false;
	bool  bTrayDirty = true;
	float ProviderPollSeconds = 0.0f;
};
