// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsRemoteBridge.h"
#include "UI/RammsBaseWidget.h"
#include "UI/RammsStatusPanel.h"
#include "UI/RammsNotificationWidget.h"
#include "UI/RammsNotificationContainer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UObjectIterator.h"

// Static member
TWeakObjectPtr<URammsNotificationContainer> URammsRemoteBridge::NotificationContainer;

// ── UI Widget Discovery ────────────────────────────────────────────

TArray<FString> URammsRemoteBridge::GetAllRammsWidgetPaths()
{
	TArray<FString> Paths;
	for (TObjectIterator<URammsBaseWidget> It; It; ++It)
	{
		URammsBaseWidget* Widget = *It;
		if (IsValid(Widget) && !Widget->HasAnyFlags(RF_ClassDefaultObject))
		{
			Paths.Add(Widget->GetPathName());
		}
	}
	return Paths;
}

TArray<FString> URammsRemoteBridge::FindRammsWidgets(const FString& ClassNameFilter)
{
	TArray<FString> Paths;
	for (TObjectIterator<URammsBaseWidget> It; It; ++It)
	{
		URammsBaseWidget* Widget = *It;
		if (IsValid(Widget) && !Widget->HasAnyFlags(RF_ClassDefaultObject))
		{
			FString ClassName = Widget->GetClass()->GetName();
			if (ClassNameFilter.IsEmpty() || ClassName.Contains(ClassNameFilter))
			{
				Paths.Add(Widget->GetPathName());
			}
		}
	}
	return Paths;
}

// ── StatusPanel helpers ────────────────────────────────────────────

static TArray<URammsStatusPanel*> FindAllStatusPanels()
{
	TArray<URammsStatusPanel*> Panels;
	for (TObjectIterator<URammsStatusPanel> It; It; ++It)
	{
		URammsStatusPanel* Panel = *It;
		if (IsValid(Panel) && !Panel->HasAnyFlags(RF_ClassDefaultObject))
		{
			Panels.Add(Panel);
		}
	}
	return Panels;
}

int32 URammsRemoteBridge::SetRobotMode(ERammsRobotMode Mode)
{
	TArray<URammsStatusPanel*> Panels = FindAllStatusPanels();
	for (URammsStatusPanel* Panel : Panels)
	{
		FRammsRobotState State = Panel->GetCachedRobotState();
		State.Mode = Mode;
		Panel->ApplyRemoteState(State);
	}
	return Panels.Num();
}

int32 URammsRemoteBridge::SetBatteryLevel(float Level)
{
	TArray<URammsStatusPanel*> Panels = FindAllStatusPanels();
	for (URammsStatusPanel* Panel : Panels)
	{
		FRammsRobotState State = Panel->GetCachedRobotState();
		State.BatteryLevel = FMath::Clamp(Level, 0.0f, 1.0f);
		Panel->ApplyRemoteState(State);
	}
	return Panels.Num();
}

int32 URammsRemoteBridge::SetSpeed(float SpeedMPS)
{
	TArray<URammsStatusPanel*> Panels = FindAllStatusPanels();
	for (URammsStatusPanel* Panel : Panels)
	{
		FRammsRobotState State = Panel->GetCachedRobotState();
		// Set speed as forward velocity
		State.LinearVelocity = FVector(SpeedMPS, 0.0f, 0.0f);
		Panel->ApplyRemoteState(State);
	}
	return Panels.Num();
}

int32 URammsRemoteBridge::SetEmergencyStop(bool bActive)
{
	TArray<URammsStatusPanel*> Panels = FindAllStatusPanels();
	for (URammsStatusPanel* Panel : Panels)
	{
		FRammsRobotState State = Panel->GetCachedRobotState();
		State.bEmergencyStop = bActive;
		if (bActive)
		{
			State.Mode = ERammsRobotMode::Emergency;
		}
		Panel->ApplyRemoteState(State);
	}
	return Panels.Num();
}

int32 URammsRemoteBridge::SetRobotState(FRammsRobotState State)
{
	TArray<URammsStatusPanel*> Panels = FindAllStatusPanels();
	for (URammsStatusPanel* Panel : Panels)
	{
		Panel->ApplyRemoteState(State);
	}
	return Panels.Num();
}

bool URammsRemoteBridge::GetRobotState(FRammsRobotState& OutState)
{
	// Try to get state from the first StatusPanel
	TArray<URammsStatusPanel*> Panels = FindAllStatusPanels();
	if (Panels.Num() > 0)
	{
		OutState = Panels[0]->GetCachedRobotState();
		return true;
	}

	// Try to find a state provider via object iteration
	for (TObjectIterator<UObject> It; It; ++It)
	{
		if (It->GetClass()->ImplementsInterface(URammsStateProvider::StaticClass()))
		{
			IRammsStateProvider* Provider = Cast<IRammsStateProvider>(*It);
			if (Provider)
			{
				return Provider->GetRobotState(OutState);
			}
		}
	}

	return false;
}

// ── Notifications ──────────────────────────────────────────────────

// Helper: get the first play world
static UWorld* GetPlayWorld()
{
	if (!GEngine)
		return nullptr;

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
		{
			return Context.World();
		}
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Editor)
		{
			return Context.World();
		}
	}
	return nullptr;
}

// Helper: get the style from any existing RAMMS widget
static URammsUIStyle* GetExistingStyle()
{
	for (TObjectIterator<URammsBaseWidget> It; It; ++It)
	{
		if (IsValid(*It) && !It->HasAnyFlags(RF_ClassDefaultObject))
		{
			URammsUIStyle* S = It->GetStyle();
			if (S)
				return S;
		}
	}
	return nullptr;
}

URammsNotificationContainer* URammsRemoteBridge::GetOrCreateContainer()
{
	// Return existing container if still valid and in viewport
	if (NotificationContainer.IsValid() && NotificationContainer->IsInViewport())
	{
		return NotificationContainer.Get();
	}

	UWorld* World = GetPlayWorld();
	if (!World)
		return nullptr;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
		return nullptr;

	// Create the container widget
	URammsNotificationContainer* Container = CreateWidget<URammsNotificationContainer>(PC);
	if (!Container)
		return nullptr;

	// Add to viewport at high z-order — container is full-screen with internal right-alignment
	Container->AddToViewport(100);

	NotificationContainer = Container;

	UE_LOG(LogTemp, Log, TEXT("ShowNotification: Created notification container"));
	return Container;
}

bool URammsRemoteBridge::ShowNotification(const FString& Message,
	ERammsNotificationLevel Level, float Duration, const FString& Title)
{
	URammsNotificationContainer* Container = GetOrCreateContainer();
	if (!Container)
	{
		UE_LOG(LogTemp, Warning, TEXT("ShowNotification: Could not create notification container"));
		return false;
	}

	UWorld* World = GetPlayWorld();
	if (!World)
		return false;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
		return false;

	// Create the notification widget
	URammsNotificationWidget* Notification = CreateWidget<URammsNotificationWidget>(PC);
	if (!Notification)
		return false;

	// Apply style
	URammsUIStyle* ExistingStyle = GetExistingStyle();
	if (ExistingStyle)
	{
		Notification->SetStyle(ExistingStyle);
	}

	// Set content before adding to container (avoids double SlideIn from NativeConstruct)
	Notification->SetNotificationContent(
		FText::FromString(Message),
		Level,
		Duration,
		Title.IsEmpty() ? FText() : FText::FromString(Title));

	// Add to the container's vertical box
	Container->AddNotification(Notification);

	UE_LOG(LogTemp, Log, TEXT("ShowNotification: '%s' (Level=%d, Duration=%.1f)"),
		*Message, (int32)Level, Duration);
	return true;
}

int32 URammsRemoteBridge::DismissAllNotifications()
{
	if (!NotificationContainer.IsValid())
		return 0;

	return NotificationContainer->DismissAll();
}
