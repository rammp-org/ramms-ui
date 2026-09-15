// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsControlHUDSubsystem.h"
#include "RammsControlHUDSettings.h"
#include "RammsUISubsystem.h"
#include "UI/RammsControlSurfacePanel.h"
#include "UI/RammsLayoutHost.h"
#include "UI/RammsSimLayout.h"
#include "UI/RammsSurfaceJoystick.h"
#include "UI/RammsUIStyle.h"
#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

URammsControlHUDSettings::URammsControlHUDSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("Ramms Control HUD");
}

URammsControlHUDSubsystem* URammsControlHUDSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	if (const ULocalPlayer* LP = World->GetFirstLocalPlayerFromController())
	{
		return LP->GetSubsystem<URammsControlHUDSubsystem>();
	}
	return nullptr;
}

void URammsControlHUDSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void URammsControlHUDSubsystem::Deinitialize()
{
	DestroyHUD();
	if (UWorld* World = SubscribedWorld.Get())
	{
		if (URammsUISubsystem* UI = World->GetSubsystem<URammsUISubsystem>())
		{
			UI->OnControlSurfaceRegistryChanged.RemoveDynamic(this, &URammsControlHUDSubsystem::OnRegistryChanged);
		}
	}
	SubscribedWorld = nullptr;
	Super::Deinitialize();
}

void URammsControlHUDSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	DestroyHUD();
	PC = NewPlayerController;
	if (!NewPlayerController)
	{
		return;
	}
	UWorld* World = NewPlayerController->GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	if (SubscribedWorld.Get() != World)
	{
		if (UWorld* Old = SubscribedWorld.Get())
		{
			if (URammsUISubsystem* OldUI = Old->GetSubsystem<URammsUISubsystem>())
			{
				OldUI->OnControlSurfaceRegistryChanged.RemoveDynamic(this, &URammsControlHUDSubsystem::OnRegistryChanged);
			}
		}
		if (URammsUISubsystem* UI = World->GetSubsystem<URammsUISubsystem>())
		{
			UI->OnControlSurfaceRegistryChanged.AddUniqueDynamic(this, &URammsControlHUDSubsystem::OnRegistryChanged);
			SubscribedWorld = World;
		}
	}
	TrySpawn();
}

void URammsControlHUDSubsystem::OnRegistryChanged(UObject* /*Provider*/, bool bRegistered)
{
	if (bRegistered && !Host)
	{
		TrySpawn();
	}
}

void URammsControlHUDSubsystem::TrySpawn()
{
	const URammsControlHUDSettings* Settings = GetDefault<URammsControlHUDSettings>();
	APlayerController*				Controller = PC.Get();
	if (!Settings || !Settings->bAutoSpawn || !Controller || !Controller->IsLocalPlayerController() || Host)
	{
		return;
	}
	// Only once a robot is controllable; the registry callback retries.
	UWorld*			   World = Controller->GetWorld();
	URammsUISubsystem* UI = World ? World->GetSubsystem<URammsUISubsystem>() : nullptr;
	if (!UI || UI->GetControlSurfaceCount() == 0)
	{
		return;
	}
	SpawnHUD();
}

bool URammsControlHUDSubsystem::SpawnHUD()
{
	APlayerController* Controller = PC.Get();
	if (!Controller || !Controller->IsLocalPlayerController())
	{
		return false;
	}
	DestroyHUD();
	const URammsControlHUDSettings* Settings = GetDefault<URammsControlHUDSettings>();

	Host = CreateWidget<URammsLayoutHost>(Controller, URammsLayoutHost::StaticClass());
	if (!Host)
	{
		return false;
	}
	URammsUIStyle* Style = Settings ? Settings->Style.LoadSynchronous() : nullptr;
	if (!Style)
	{
		if (URammsUISubsystem* UI = Controller->GetWorld()->GetSubsystem<URammsUISubsystem>())
		{
			Style = UI->GetTheme();
		}
	}
	if (!Style)
	{
		Style = URammsUIStyle::CreateDefaultDarkTheme();
	}
	Host->Style = Style;

	UClass* LayoutClass = Settings ? Settings->LayoutClass.LoadSynchronous() : nullptr;
	if (!LayoutClass)
	{
		LayoutClass = URammsSimLayout::StaticClass();
	}
	Host->AddLayout(LayoutClass, FName("Sim"));

	if (!Settings || Settings->bShowSurfacePanel)
	{
		SurfacePanel = CreateWidget<URammsControlSurfacePanel>(Controller, URammsControlSurfacePanel::StaticClass());
		if (SurfacePanel)
		{
			if (Settings)
			{
				SurfacePanel->ExpandedGroups = Settings->ExpandedGroups;
			}
			Host->AddPoolWidget(FName("SurfacePanel"), SurfacePanel);
		}
	}
	if (!Settings || Settings->bShowJoystick)
	{
		Joystick = CreateWidget<URammsSurfaceJoystick>(Controller, URammsSurfaceJoystick::StaticClass());
		if (Joystick)
		{
			Joystick->SetRadii(80.0f, 28.0f);
			Host->AddPoolWidget(FName("Joystick"), Joystick);
		}
	}
	Host->AddToPlayerScreen(10);
	Host->TransitionToLayout(FName("Sim"), false);

	// Make the HUD reachable: the game viewport otherwise captures the mouse
	// (game-only input mode, hidden cursor), and the engine's virtual joystick
	// (DefaultTouchInterface, shown whenever the mouse fakes touch in PIE)
	// sits above every widget and swallows presses.
	if (!Settings || Settings->bReplaceEngineTouchInterface)
	{
		Controller->ActivateTouchInterface(nullptr);
	}
	if (!Settings || Settings->bGameAndUIInputMode)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Controller->SetInputMode(Mode);
		Controller->bShowMouseCursor = true;
	}
	UE_LOG(LogTemp, Log, TEXT("[RammsControlHUD] spawned for %s (layout %s)"), *Controller->GetName(), *LayoutClass->GetName());
	return true;
}

void URammsControlHUDSubsystem::DestroyHUD()
{
	if (Host)
	{
		Host->RemoveFromParent();
	}
	Host = nullptr;
	SurfacePanel = nullptr;
	Joystick = nullptr;
}
