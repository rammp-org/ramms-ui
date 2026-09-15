// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "RammsControlHUDSubsystem.generated.h"

class APlayerController;
class URammsControlSurfacePanel;
class URammsLayoutHost;
class URammsSurfaceJoystick;

/**
 * The single spawn path for the sim's UI. One per local player; when that
 * player gets a controller and a robot has registered a control surface (or
 * as soon as one does), it creates a URammsLayoutHost with the layout from
 * URammsControlHUDSettings, pools a URammsControlSurfacePanel and a
 * URammsSurfaceJoystick, and adds the host to the player's screen. Nothing
 * in a game mode or player controller has to know about it, so every
 * controller — the chair's, the MuJoCo test game mode's plain
 * APlayerController, the time-trial variant's — gets the same UI.
 */
UCLASS()
class RAMMSUI_API URammsControlHUDSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	/** The HUD subsystem of the first local player in WorldContextObject's world
	 *  (Blueprint / Python convenience; null when there is no local player). */
	UFUNCTION(BlueprintPure, Category = "Ramms|HUD", meta = (WorldContext = "WorldContextObject"))
	static URammsControlHUDSubsystem* Get(const UObject* WorldContextObject);

	/** Build (or rebuild) the HUD for the current player controller now. */
	UFUNCTION(BlueprintCallable, Category = "Ramms|HUD")
	bool SpawnHUD();

	UFUNCTION(BlueprintCallable, Category = "Ramms|HUD")
	void DestroyHUD();

	UFUNCTION(BlueprintPure, Category = "Ramms|HUD")
	URammsLayoutHost* GetHost() const { return Host; }

	UFUNCTION(BlueprintPure, Category = "Ramms|HUD")
	URammsControlSurfacePanel* GetSurfacePanel() const { return SurfacePanel; }

	UFUNCTION(BlueprintPure, Category = "Ramms|HUD")
	URammsSurfaceJoystick* GetJoystick() const { return Joystick; }

private:
	UFUNCTION()
	void OnRegistryChanged(UObject* Provider, bool bRegistered);

	void TrySpawn();

	UPROPERTY(Transient)
	TObjectPtr<URammsLayoutHost> Host;
	UPROPERTY(Transient)
	TObjectPtr<URammsControlSurfacePanel> SurfacePanel;
	UPROPERTY(Transient)
	TObjectPtr<URammsSurfaceJoystick> Joystick;

	TWeakObjectPtr<APlayerController> PC;
	TWeakObjectPtr<UWorld>			  SubscribedWorld;
};
