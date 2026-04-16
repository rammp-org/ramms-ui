// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsRemoteBridge.h"
#include "UI/RammsBaseWidget.h"
#include "UI/RammsStatusPanel.h"
#include "UI/RammsNotificationWidget.h"
#include "UI/RammsNotificationContainer.h"
#include "RammsUISubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UObjectIterator.h"

// ── Static members ─────────────────────────────────────────────────

TWeakObjectPtr<URammsNotificationContainer> URammsRemoteBridge::NotificationContainer;
TWeakObjectPtr<URammsUISubsystem>			URammsRemoteBridge::CachedSubsystem;
TWeakObjectPtr<UWorld>						URammsRemoteBridge::CachedWorld;
TArray<TWeakObjectPtr<URammsStatusPanel>>	URammsRemoteBridge::CachedPanels;
bool										URammsRemoteBridge::bPanelCacheDirty = true;
TWeakObjectPtr<URammsUIStyle>				URammsRemoteBridge::CachedStyle;

// ── Cache infrastructure ───────────────────────────────────────────

UWorld* URammsRemoteBridge::GetPlayWorld()
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

URammsUISubsystem* URammsRemoteBridge::GetCachedSubsystem()
{
	// Fast path — cached pointer still valid and world hasn't changed
	UWorld* World = GetPlayWorld();
	if (CachedSubsystem.IsValid() && CachedWorld.Get() == World)
	{
		return CachedSubsystem.Get();
	}

	// World changed or cache stale — refresh everything
	CachedWorld = World;
	bPanelCacheDirty = true;
	CachedStyle.Reset();

	if (World)
	{
		CachedSubsystem = World->GetSubsystem<URammsUISubsystem>();
		return CachedSubsystem.Get();
	}

	CachedSubsystem.Reset();
	return nullptr;
}

TArray<URammsStatusPanel*> URammsRemoteBridge::GetCachedPanels()
{
	// If world changed, GetCachedSubsystem already set bPanelCacheDirty
	GetCachedSubsystem();

	// Prune stale weak pointers
	if (!bPanelCacheDirty)
	{
		for (int32 i = CachedPanels.Num() - 1; i >= 0; --i)
		{
			if (!CachedPanels[i].IsValid())
			{
				CachedPanels.RemoveAtSwap(i);
				bPanelCacheDirty = true;
			}
		}
	}

	// Full rescan only when dirty (world change, stale ptr found, or first call)
	if (bPanelCacheDirty)
	{
		CachedPanels.Reset();
		for (TObjectIterator<URammsStatusPanel> It; It; ++It)
		{
			URammsStatusPanel* Panel = *It;
			if (IsValid(Panel) && !Panel->HasAnyFlags(RF_ClassDefaultObject))
			{
				CachedPanels.Add(Panel);
			}
		}
		bPanelCacheDirty = (CachedPanels.Num() == 0);
	}

	// Build raw-pointer array for callers
	TArray<URammsStatusPanel*> Result;
	Result.Reserve(CachedPanels.Num());
	for (const TWeakObjectPtr<URammsStatusPanel>& Weak : CachedPanels)
	{
		if (URammsStatusPanel* P = Weak.Get())
		{
			Result.Add(P);
		}
	}
	return Result;
}

URammsUIStyle* URammsRemoteBridge::GetCachedStyle()
{
	// Always source the style from the subsystem's active theme so we
	// automatically pick up SetTheme() changes without a manual cache
	// invalidation.
	if (URammsUISubsystem* Sub = GetCachedSubsystem())
	{
		URammsUIStyle* Theme = Sub->GetTheme();
		if (Theme)
		{
			CachedStyle = Theme;
			return Theme;
		}
	}

	// Fallback: scan widgets (e.g. if subsystem has no theme set yet)
	if (CachedStyle.IsValid())
	{
		return CachedStyle.Get();
	}

	for (TObjectIterator<URammsBaseWidget> It; It; ++It)
	{
		if (IsValid(*It) && !It->HasAnyFlags(RF_ClassDefaultObject))
		{
			URammsUIStyle* S = It->GetStyle();
			if (S)
			{
				CachedStyle = S;
				return S;
			}
		}
	}
	return nullptr;
}

void URammsRemoteBridge::InvalidateCache()
{
	CachedSubsystem.Reset();
	CachedWorld.Reset();
	CachedPanels.Reset();
	bPanelCacheDirty = true;
	CachedStyle.Reset();
	NotificationContainer.Reset();
}

// ── UI Widget Discovery ────────────────────────────────────────────
// These are infrequent diagnostic calls — TObjectIterator is acceptable.

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

// ── Status Panel (cached) ──────────────────────────────────────────

int32 URammsRemoteBridge::SetRobotMode(ERammsRobotMode Mode)
{
	TArray<URammsStatusPanel*> Panels = GetCachedPanels();
	for (URammsStatusPanel* Panel : Panels)
	{
		FRammsRobotState State = Panel->GetCachedRobotState();
		State.Mode = Mode;
		Panel->ApplyRemoteState(State);
	}
	if (URammsUISubsystem* Sub = GetCachedSubsystem())
	{
		Sub->UpdateRobotMode(Mode);
	}
	return Panels.Num();
}

int32 URammsRemoteBridge::SetBatteryLevel(float Level)
{
	TArray<URammsStatusPanel*> Panels = GetCachedPanels();
	for (URammsStatusPanel* Panel : Panels)
	{
		FRammsRobotState State = Panel->GetCachedRobotState();
		State.BatteryLevel = FMath::Clamp(Level, 0.0f, 1.0f);
		Panel->ApplyRemoteState(State);
	}
	if (URammsUISubsystem* Sub = GetCachedSubsystem())
	{
		Sub->UpdateBatteryLevel(Level);
	}
	return Panels.Num();
}

int32 URammsRemoteBridge::SetSpeed(float SpeedMPS)
{
	TArray<URammsStatusPanel*> Panels = GetCachedPanels();
	for (URammsStatusPanel* Panel : Panels)
	{
		FRammsRobotState State = Panel->GetCachedRobotState();
		State.LinearVelocity = FVector(SpeedMPS, 0.0f, 0.0f);
		Panel->ApplyRemoteState(State);
	}
	if (URammsUISubsystem* Sub = GetCachedSubsystem())
	{
		Sub->UpdateSpeed(SpeedMPS);
	}
	return Panels.Num();
}

int32 URammsRemoteBridge::SetEmergencyStop(bool bActive)
{
	TArray<URammsStatusPanel*> Panels = GetCachedPanels();
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
	if (URammsUISubsystem* Sub = GetCachedSubsystem())
	{
		Sub->UpdateEmergencyStop(bActive);
	}
	return Panels.Num();
}

int32 URammsRemoteBridge::SetRobotState(FRammsRobotState State)
{
	TArray<URammsStatusPanel*> Panels = GetCachedPanels();
	for (URammsStatusPanel* Panel : Panels)
	{
		Panel->ApplyRemoteState(State);
	}

	if (URammsUISubsystem* Sub = GetCachedSubsystem())
	{
		Sub->BroadcastRobotStateChanged(State);
	}

	return Panels.Num();
}

bool URammsRemoteBridge::GetRobotState(FRammsRobotState& OutState)
{
	// Use cached panels — no TObjectIterator scan
	TArray<URammsStatusPanel*> Panels = GetCachedPanels();
	if (Panels.Num() > 0)
	{
		OutState = Panels[0]->GetCachedRobotState();
		return true;
	}

	// Fall back to subsystem's cached robot state — but only if a state
	// has actually been broadcast.  Otherwise return false to honour the
	// "no provider found" contract.
	if (URammsUISubsystem* Sub = GetCachedSubsystem())
	{
		if (Sub->HasRobotState())
		{
			OutState = Sub->GetCachedRobotState();
			return true;
		}
	}

	return false;
}

// ── Notifications ──────────────────────────────────────────────────

URammsNotificationContainer* URammsRemoteBridge::GetOrCreateContainer()
{
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

	URammsNotificationContainer* Container = CreateWidget<URammsNotificationContainer>(PC);
	if (!Container)
		return nullptr;

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

	URammsNotificationWidget* Notification = CreateWidget<URammsNotificationWidget>(PC);
	if (!Notification)
		return false;

	// Use cached style instead of TObjectIterator scan
	URammsUIStyle* ExistingStyle = GetCachedStyle();
	if (ExistingStyle)
	{
		Notification->SetStyle(ExistingStyle);
	}

	Notification->SetNotificationContent(
		FText::FromString(Message),
		Level,
		Duration,
		Title.IsEmpty() ? FText() : FText::FromString(Title));

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

// ── Key-Value Properties (cached subsystem) ───────────────────────

bool URammsRemoteBridge::SetProperty(FName Key, const FString& Value)
{
	URammsUISubsystem* Sub = GetCachedSubsystem();
	if (!Sub)
		return false;

	Sub->SetProperty(Key, Value);
	return true;
}

bool URammsRemoteBridge::SetProperties(const TMap<FName, FString>& Properties)
{
	URammsUISubsystem* Sub = GetCachedSubsystem();
	if (!Sub)
		return false;

	Sub->SetProperties(Properties);
	return true;
}

FString URammsRemoteBridge::GetProperty(FName Key)
{
	URammsUISubsystem* Sub = GetCachedSubsystem();
	return Sub ? Sub->GetProperty(Key) : FString();
}

TArray<FName> URammsRemoteBridge::GetAllPropertyKeys()
{
	URammsUISubsystem* Sub = GetCachedSubsystem();
	return Sub ? Sub->GetAllPropertyKeys() : TArray<FName>();
}

bool URammsRemoteBridge::RemoveProperty(FName Key)
{
	URammsUISubsystem* Sub = GetCachedSubsystem();
	if (!Sub || !Sub->HasProperty(Key))
		return false;

	Sub->RemoveProperty(Key);
	return true;
}
