// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsUISubsystem.h"

void URammsUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("URammsUISubsystem initialized"));
}

void URammsUISubsystem::Deinitialize()
{
	RegisteredControllers.Empty();
	Super::Deinitialize();
}

// ── Robot Controller Registry ─────────────────────────────────────

void URammsUISubsystem::RegisterRobotController(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	if (!Actor->GetClass()->ImplementsInterface(URammsRobotController::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("URammsUISubsystem::RegisterRobotController: %s does not implement IRammsRobotController"), *Actor->GetName());
		return;
	}

	// Avoid duplicates
	for (const TWeakObjectPtr<AActor>& Existing : RegisteredControllers)
	{
		if (Existing.Get() == Actor)
		{
			return;
		}
	}

	RegisteredControllers.Add(Actor);
	OnControllerRegistryChanged.Broadcast(Actor, true);

	UE_LOG(LogTemp, Log, TEXT("URammsUISubsystem: Registered robot controller '%s'"), *Actor->GetName());
}

void URammsUISubsystem::UnregisterRobotController(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	int32 Removed = RegisteredControllers.RemoveAll([Actor](const TWeakObjectPtr<AActor>& Weak)
	{
		return Weak.Get() == Actor;
	});

	if (Removed > 0)
	{
		OnControllerRegistryChanged.Broadcast(Actor, false);
		UE_LOG(LogTemp, Log, TEXT("URammsUISubsystem: Unregistered robot controller '%s'"), *Actor->GetName());
	}
}

AActor* URammsUISubsystem::FindRobotController() const
{
	for (const TWeakObjectPtr<AActor>& Weak : RegisteredControllers)
	{
		if (AActor* Actor = Weak.Get())
		{
			return Actor;
		}
	}
	return nullptr;
}

AActor* URammsUISubsystem::FindRobotControllerByName(const FString& RobotName) const
{
	for (const TWeakObjectPtr<AActor>& Weak : RegisteredControllers)
	{
		AActor* Actor = Weak.Get();
		if (!Actor)
		{
			continue;
		}

		if (IRammsRobotController::Execute_GetRobotName(Actor) == RobotName)
		{
			return Actor;
		}
	}
	return nullptr;
}

TArray<AActor*> URammsUISubsystem::GetAllRobotControllers() const
{
	TArray<AActor*> Result;
	for (const TWeakObjectPtr<AActor>& Weak : RegisteredControllers)
	{
		if (AActor* Actor = Weak.Get())
		{
			Result.Add(Actor);
		}
	}
	return Result;
}

void URammsUISubsystem::CleanupStaleControllers()
{
	RegisteredControllers.RemoveAll([](const TWeakObjectPtr<AActor>& Weak)
	{
		return !Weak.IsValid();
	});
}

// ── UI Event Bus ──────────────────────────────────────────────────

void URammsUISubsystem::BroadcastTaskAction(ERammsTaskAction Action)
{
	OnTaskAction.Broadcast(Action);
}

void URammsUISubsystem::BroadcastToolbarItemClicked(FName ItemID)
{
	OnToolbarItemClicked.Broadcast(ItemID);
}

void URammsUISubsystem::BroadcastToolbarItemToggled(FName ItemID, bool bIsActive)
{
	OnToolbarItemToggled.Broadcast(ItemID, bIsActive);
}

void URammsUISubsystem::BroadcastOverlayToggle(ERammsOverlayType Overlay, bool bVisible)
{
	OnOverlayToggle.Broadcast(Overlay, bVisible);
}

void URammsUISubsystem::BroadcastVisualizationLayerToggle(ERammsVisualizationLayer Layer, bool bEnabled)
{
	OnVisualizationLayerToggle.Broadcast(Layer, bEnabled);
}

void URammsUISubsystem::BroadcastHighlight(ERammsHighlightTarget Target, bool bHighlighted, FLinearColor Color)
{
	OnHighlight.Broadcast(Target, bHighlighted, Color);
}

void URammsUISubsystem::BroadcastCustomUIEvent(const FRammsUIEvent& Event)
{
	OnCustomUIEvent.Broadcast(Event);
}
