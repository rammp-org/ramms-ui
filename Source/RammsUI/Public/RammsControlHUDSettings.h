// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RammsControlHUDSettings.generated.h"

class URammsLayoutBase;
class URammsUIStyle;

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

	/** Layout class; empty = URammsSimLayout. Slots: SurfacePanel, Joystick, Status. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD", meta = (AllowAbstract = "false"))
	TSoftClassPtr<URammsLayoutBase> LayoutClass;

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

	/** Near and far of the depth colour ramp, in CENTIMETRES, which is what the
	 *  capture produces. A viewing choice: it decides which distances the ramp
	 *  spans, nothing about what is captured. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera Capture|Depth", meta = (ClampMin = "0.0"))
	float DepthColormapMinCM = 10.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Camera Capture|Depth", meta = (ClampMin = "0.1"))
	float DepthColormapMaxCM = 1500.0f;

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

	virtual FName GetCategoryName() const override { return FName("Plugins"); }
};
