// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UI/RammsBaseWidget.h"
#include "RammsFloatingPanel.generated.h"

class SWindow;
class UBorder;
class UOverlay;
class UPanelWidget;
class USizeBox;
class UTextBlock;
class URammsButton;
class URammsFloatingPanel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRammsFloatingPanelChanged, URammsFloatingPanel*, Panel);

/**
 * A HUD panel the user arranges: window chrome around any widget. Its title bar
 * moves it (drag), minimises it to the bar, hides it (the layout's panel tray
 * brings it back) and pops it out into a window of its own -- closing that
 * window docks it back; a grip at its bottom-right corner resizes it. Clicking
 * it brings it to the front. Its place, size and state are kept per user
 * (URammsHUDLayoutUserSettings, by PanelId) and restored next time, its place
 * as a fraction of the room the viewport leaves around it, so a panel against
 * an edge stays there when the window is resized.
 *
 * It sits in a canvas (a layout's): the layout calls UpdateLayout each tick.
 * The content goes in GetContentSlot(), a layout injection slot like any
 * other; content that implements IRammsFloatingPanelContent gives the title
 * and drops its own frame.
 *
 * A popped-out panel is not seen over Pixel Streaming, which streams only the
 * game viewport; pop-outs are off when the command line asks for streaming,
 * and on platforms without desktop windows. Keyboard focus follows the mouse
 * into its window: click back into the game view to drive with the keyboard.
 */
UCLASS(meta = (DisplayName = "Ramms Floating Panel"))
class RAMMSUI_API URammsFloatingPanel : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	/** The key its state is kept by; unique among the HUD's panels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floating Panel")
	FName PanelId;

	/** The title bar's text, unless the content gives one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floating Panel")
	FText Title;

	/** Where it first appears: its top-left as a fraction of the room the
	 *  viewport leaves around it (0,0 top-left; 1,1 bottom-right). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floating Panel")
	FVector2D DefaultPlacement = FVector2D::ZeroVector;

	/** Its body's first size (Slate units); 0 on an axis: the content's own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floating Panel")
	FVector2D DefaultSize = FVector2D::ZeroVector;

	/** The least a resize leaves its body (Slate units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floating Panel")
	FVector2D MinSize = FVector2D(200.0, 60.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floating Panel")
	bool bResizable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floating Panel")
	bool bAllowPopOut = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floating Panel")
	bool bAllowClose = true;

	/** Its state changed: shown, hidden, minimised, popped out or docked. */
	UPROPERTY(BlueprintAssignable, Category = "Floating Panel")
	FOnRammsFloatingPanelChanged OnPanelChanged;

	/** Where the content goes (a layout registers it as an injection slot). */
	UPanelWidget* GetContentSlot() const;

	UFUNCTION(BlueprintPure, Category = "Floating Panel")
	UWidget* GetContent() const;

	UFUNCTION(BlueprintCallable, Category = "Floating Panel")
	void SetContent(UWidget* Content);

	UFUNCTION(BlueprintCallable, Category = "Floating Panel")
	void SetMinimized(bool bInMinimized);

	UFUNCTION(BlueprintPure, Category = "Floating Panel")
	bool IsMinimized() const { return bMinimized; }

	/** Hide it (the user's choice; the panel tray shows it again). */
	UFUNCTION(BlueprintCallable, Category = "Floating Panel")
	void SetPanelHidden(bool bInHidden);

	UFUNCTION(BlueprintPure, Category = "Floating Panel")
	bool IsPanelHidden() const { return bUserHidden; }

	/** Into a window of its own; nothing when pop-outs are not available. */
	UFUNCTION(BlueprintCallable, Category = "Floating Panel")
	void PopOut();

	/** Back from its window into the viewport. */
	UFUNCTION(BlueprintCallable, Category = "Floating Panel")
	void DockBack();

	UFUNCTION(BlueprintPure, Category = "Floating Panel")
	bool IsPoppedOut() const;

	/** Its window, brought to the front. */
	UFUNCTION(BlueprintCallable, Category = "Floating Panel")
	void FocusWindow();

	UFUNCTION(BlueprintCallable, Category = "Floating Panel")
	void SetPlacement(FVector2D InPlacement);

	/** Its placement (negative: never placed, at DefaultPlacement). */
	UFUNCTION(BlueprintPure, Category = "Floating Panel")
	FVector2D GetPlacement() const { return Placement; }

	UFUNCTION(BlueprintCallable, Category = "Floating Panel")
	void SetBodySize(FVector2D InSize);

	UFUNCTION(BlueprintPure, Category = "Floating Panel")
	FVector2D GetBodySize() const { return BodySize; }

	/** Back where it first appeared, its own size, shown, docked. */
	UFUNCTION(BlueprintCallable, Category = "Floating Panel")
	void ResetToDefault();

	UFUNCTION(BlueprintCallable, Category = "Floating Panel")
	void BringToFront();

	/** Whether whatever supplies it wants it shown (a panel with nothing to
	 *  show is not) -- apart from the user hiding it. */
	void SetWanted(bool bInWanted);
	bool IsWanted() const { return bWanted; }

	/** Shown in the viewport: wanted, not hidden, not popped out. */
	UFUNCTION(BlueprintPure, Category = "Floating Panel")
	bool IsShownInViewport() const;

	/** The title shown now: the content's, else Title. */
	UFUNCTION(BlueprintPure, Category = "Floating Panel")
	FText GetDisplayTitle() const;

	/** Place it in a canvas of CanvasSize (Slate units), Margin from its
	 *  edges and TopInset from its top (clear of the layout's panel tray),
	 *  and keep it up: the layout calls this every tick, shown or not (a
	 *  collapsed or popped-out panel is not ticked by Slate). */
	void UpdateLayout(const FVector2D& CanvasSize, float Margin, float TopInset, float DeltaSeconds);

	/** Whether panels can pop out here: a desktop platform, the HUD settings
	 *  allow it, and no Pixel Streaming asked for on the command line. */
	static bool IsPopOutAvailable();

protected:
	virtual void   BuildWidgetTree() override;
	virtual void   ResetCachedWidgets() override;
	virtual void   ApplyStyle_Implementation() override;
	virtual void   NativeConstruct() override;
	virtual void   NativeDestruct() override;
	virtual void   BeginDestroy() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void   NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	UFUNCTION()
	void HandleMinimizeClicked();
	UFUNCTION()
	void HandleCloseClicked();
	UFUNCTION()
	void HandlePopOutClicked();

	void TickPanel(float DeltaSeconds);
	/** The user asked its window to close (the window's X): dock back. */
	void HandleWindowCloseRequested(const TSharedRef<SWindow>& Window);
	/** Its window was destroyed otherwise -- with the game's window, at
	 *  shutdown: the content comes back, and it stays popped out for next
	 *  time. */
	void HandleWindowClosed(const TSharedRef<SWindow>& Window);
	/** What a popped-out window shows: the content on the panel's background. */
	TSharedRef<SWidget> MakeWindowContent(UWidget* Content);
	/** The popped-out content back in the panel (unless the panel was given
	 *  other content meanwhile). */
	void DockContent();
	/** Tear its window down at once, keeping it popped out for next time. */
	void TearDownWindow();
	void CloseWindow(bool bKeepPoppedOut);
	void ApplyVisibility();
	void ApplySize();
	/** The size its placement is a fraction of the room around: the whole
	 *  panel's, even while minimised (its title bar stays where the panel's
	 *  top is); its own when that is not known yet. */
	FVector2D GetPlacementSize() const { return bMinimized && ExpandedSize.Y > 0.0 ? ExpandedSize : GetDesiredSize(); }
	void	  LoadState();
	void	  SaveState();
	void	  EndDrag();
	void	  Changed();

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> FrameSize;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> Shadow;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> FrameBorder;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> Accent;
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> TitleSize;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> TitleBar;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<URammsButton> PopOutButton;
	UPROPERTY(Transient)
	TObjectPtr<URammsButton> MinimizeButton;
	UPROPERTY(Transient)
	TObjectPtr<URammsButton> CloseButton;
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> BodyArea;
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> BodyBox;
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> ContentSlot;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> Grip;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GripText;

	/** The content while it is in its own window, and that window. */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> PoppedContent;
	TSharedPtr<SWindow> PoppedWindow;
	FSlateBrush			WindowBrush;

	/** The content last seen in the slot (told it is hosted). */
	TWeakObjectPtr<UWidget> HostedContent;

	enum class EDrag : uint8
	{
		None,
		Move,
		Resize,
	};
	EDrag	  Drag = EDrag::None;
	FVector2D DragStartMouse = FVector2D::ZeroVector;
	FVector2D DragStartPosition = FVector2D::ZeroVector;
	FVector2D DragStartSize = FVector2D::ZeroVector;
	/** The panel around its body (title bar, frame) when a resize began. */
	FVector2D DragChrome = FVector2D::ZeroVector;

	FVector2D Placement = FVector2D(-1.0, -1.0);
	FVector2D BodySize = FVector2D::ZeroVector;
	/** The largest body the viewport has room for (zero: not known yet). A
	 *  larger BodySize is shown at this size, and kept for a larger viewport. */
	FVector2D FitSize = FVector2D::ZeroVector;
	/** The whole panel's size, when last seen not minimised (zero: not yet). */
	FVector2D ExpandedSize = FVector2D::ZeroVector;
	FVector2D WindowPosition = FVector2D::ZeroVector;
	FVector2D WindowSize = FVector2D::ZeroVector;
	FVector2D LastCanvasSize = FVector2D::ZeroVector;
	float	  LastMargin = 8.0f;
	float	  LastTopInset = 8.0f;
	bool	  bMinimized = false;
	bool	  bUserHidden = false;
	bool	  bWanted = true;
	bool	  bStateLoaded = false;
	/** To pop out once there is content (and it is wanted). */
	bool bPopOutPending = false;
	/** A resize just ended: its placement follows from where it is now, once
	 *  its new size is laid out. */
	bool bPlaceFromPosition = false;
	/** Its window is closing because the HUD is going (keep it popped out
	 *  for next time; do not dock the content). */
	bool  bTearingDown = false;
	float TitleRefreshSeconds = 0.0f;
	float TitleWidth = 0.0f;

	static int32 NextZOrder;
};
