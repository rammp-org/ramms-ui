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
