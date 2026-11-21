// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UI/RammsUIStyle.h"
#include "RammsControlHUDSettings.generated.h"

class URammsLayoutBase;

/** A tab of the robot panel in the Sim look: the control-surface groups it shows, in this order. */
USTRUCT()
struct FRammsHudTab
{
	GENERATED_BODY()

	/** The tab's label. */
	UPROPERTY(EditAnywhere, Category = "Tab")
	FName Name;

	/** Group names (FRammsControlAxis::Group) shown under this tab. */
	UPROPERTY(EditAnywhere, Category = "Tab")
	TArray<FName> Groups;
};

/**
 * Project Settings > Plugins > Ramms Control HUD. Governs the one spawn
 * path for the sim's UI: URammsControlHUDSubsystem builds a layout host with
 * the control-surface panel and drive joystick for every local player as
 * soon as a robot registers a control surface.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Ramms Control HUD"))
class RAMMSUI_API URammsControlHUDSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URammsControlHUDSettings();

	/** Spawn the HUD automatically for local players (worlds with a control surface). */
	UPROPERTY(Config, EditAnywhere, Category = "HUD")
	bool bAutoSpawn = true;

	/** Layout class; empty = URammsSimLayout. Slots: SurfacePanel, Joystick, Status, CameraCapturePanel. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD", meta = (AllowAbstract = "false"))
	TSoftClassPtr<URammsLayoutBase> LayoutClass;

	/** The sim layout's panels float: each in a URammsFloatingPanel the user
	 *  moves, resizes, minimises, hides and pops out, its place kept per user
	 *  (URammsHUDLayoutUserSettings), with a panel tray at the top to bring
	 *  hidden ones back. Off: the fixed columns of before. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD")
	bool bFloatingPanels = true;

	/** Floating panels may pop out into windows of their own (desktop only;
	 *  never while Pixel Streaming is asked for on the command line, which
	 *  streams only the game viewport). */
	UPROPERTY(Config, EditAnywhere, Category = "HUD")
	bool bAllowPopOut = true;

	UPROPERTY(Config, EditAnywhere, Category = "HUD")
	bool bShowSurfacePanel = true;

	UPROPERTY(Config, EditAnywhere, Category = "HUD")
	bool bShowJoystick = true;

	/** Show the camera capture panel: a live feed per registered camera, plus
	 *  capture / serialization / rate controls. It costs nothing when no camera
	 *  is registered -- the panel builds no feeds and the subsystem does no work
	 *  -- so it is on, like the other panels. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD")
	bool bShowCameraCapturePanel = true;

	// -- Camera capture panel: depth view ------------------------------------
	//
	// Here rather than on the widget because the HUD creates that widget from
	// the C++ class at runtime -- there is no instance in the editor to select,
	// so its own EditAnywhere properties were unreachable in practice.

	/** Near and far of the depth colour ramp, in METRES -- the units the
	 *  material's plane inputs take, which is what its DepthScaleToCM parameter
	 *  is there to reconcile with the centimetres it samples. 0.1 and 10 are the
	 *  material's own defaults and read correctly; larger values push every real
	 *  surface past the far plane and flatten the view. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera Capture|Depth", meta = (ClampMin = "0.0"))
	float DepthColormapMinMetres = 0.1f;

	UPROPERTY(Config, EditAnywhere, Category = "Camera Capture|Depth", meta = (ClampMin = "0.01"))
	float DepthColormapMaxMetres = 10.0f;

	/**
	 * Repeat the ramp past the far plane instead of clamping.
	 *
	 * Clamping makes every surface beyond the far plane the same colour, so a
	 * range tight enough to resolve nearby detail throws away everything behind
	 * it. Repeating gives one band per (Max - Min) of depth and keeps all
	 * distances distinguishable -- contour lines rather than a gradient. The
	 * trade is that colour no longer tells you absolute depth, only depth within
	 * a band, so it is for looking at structure rather than reading distances.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Camera Capture|Depth")
	bool bDepthColormapRepeat = false;

	/** 0 grayscale, 1 jet, 2 turbo, 3 inferno. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera Capture|Depth", meta = (ClampMin = "0", ClampMax = "3"))
	int32 DepthColormapIndex = 2;

	/** Motion vector magnitude that saturates the colour wheel. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera Capture|Motion", meta = (ClampMin = "0.001"))
	float MotionSensitivity = 20.0f;

	/** Put the player in Game-and-UI input mode with a visible cursor so the
	 *  HUD receives clicks and touches while keys still reach the game. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD")
	bool bGameAndUIInputMode = true;

	/** Remove the engine's touch interface (DefaultTouchInterface virtual
	 *  joystick): it overlays the viewport and swallows pointer events, and the
	 *  HUD's own joystick replaces it. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD")
	bool bReplaceEngineTouchInterface = true;

	/** Panel groups that start expanded (empty = all). */
	UPROPERTY(Config, EditAnywhere, Category = "HUD")
	TArray<FName> ExpandedGroups;

	/** Theme; empty = the subsystem's current theme (or the default dark theme). */
	UPROPERTY(Config, EditAnywhere, Category = "HUD")
	TSoftObjectPtr<URammsUIStyle> Style;

	// -- The Sim look ---------------------------------------------------------
	//
	// Opt-in, per project: Classic leaves the HUD exactly as it was.

	/** How the HUD looks and lays itself out. Sim: the robot panel in tabs,
	 *  one-line rows, segmented choices, a status strip and prompts, in the sim
	 *  theme (URammsUIStyle::CreateSimTheme over Style's fonts). A Style asset
	 *  whose own HudLook is Sim is used as it is, whatever this says. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD|Sim Look")
	ERammsHudLook Look = ERammsHudLook::Classic;

	/** The robot panel's tabs, in order. A group no tab names gets a tab of its
	 *  own; a tab none of whose groups the robot has is left out. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD|Sim Look")
	TArray<FRammsHudTab> Tabs;

	/** Controls drawn as a group's main action (filled). Ids, with * as a wildcard. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD|Sim Look")
	TArray<FString> PrimaryControls;

	/** Controls drawn as stopping or destroying something (red). Ids, with * as a wildcard. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD|Sim Look")
	TArray<FString> DangerControls;

	/** Controls whose state the status strip shows as chips, in order: Enum
	 *  and read-only controls. One the robot does not have is skipped. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD|Sim Look")
	TArray<FName> StatusControls;

	/** Action controls given a button on the panel tray, in order, so they are
	 *  one click away whichever tab is showing. One the robot does not have is skipped. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD|Sim Look")
	TArray<FName> TrayControls;

	/** Show what a Text control reports as a prompt at the bottom of the screen
	 *  while it wants attention (ERammsControlTextSeverity above Info): a crossing
	 *  paused for the rider, a fault. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD|Sim Look")
	bool bShowPrompts = true;

	/** Shortest time a prompt stays up, in seconds, so that one the robot raised and
	 *  cleared within a moment can be read rather than flickering by. Counted from when
	 *  the message last changed (its numbers counting down are not a change).
	 *  0 = a prompt goes as soon as the robot clears it. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD|Sim Look", meta = (ClampMin = "0.0", ClampMax = "10.0", Units = "s"))
	float PromptMinSeconds = 1.5f;

	/** How long a prompt takes to fade in and to fade out, in seconds. 0 = no fade. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD|Sim Look", meta = (ClampMin = "0.0", ClampMax = "2.0", Units = "s"))
	float PromptFadeSeconds = 0.2f;

	/** Whether Id is one of Patterns (ids; * matches any run of characters). */
	static bool MatchesAny(const TArray<FString>& Patterns, FName Id);

	virtual FName GetCategoryName() const override { return FName("Plugins"); }
};
