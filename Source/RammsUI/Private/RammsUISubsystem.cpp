// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsUISubsystem.h"
#include "UI/RammsBaseWidget.h"

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

	// Clean up stale entries first
	CleanupStaleControllers();

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

	int32 Removed = RegisteredControllers.RemoveAll([Actor](const TWeakObjectPtr<AActor>& Weak) {
		return Weak.Get() == Actor;
	});

	// Also clean up any other stale entries
	CleanupStaleControllers();

	if (Removed > 0)
	{
		OnControllerRegistryChanged.Broadcast(Actor, false);
		UE_LOG(LogTemp, Log, TEXT("URammsUISubsystem: Unregistered robot controller '%s'"), *Actor->GetName());
	}
}

AActor* URammsUISubsystem::FindRobotController()
{
	CleanupStaleControllers();

	for (const TWeakObjectPtr<AActor>& Weak : RegisteredControllers)
	{
		if (AActor* Actor = Weak.Get())
		{
			return Actor;
		}
	}
	return nullptr;
}

AActor* URammsUISubsystem::FindRobotControllerByName(const FString& RobotName)
{
	CleanupStaleControllers();

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

TArray<AActor*> URammsUISubsystem::GetAllRobotControllers()
{
	CleanupStaleControllers();

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

int32 URammsUISubsystem::GetRobotControllerCount()
{
	CleanupStaleControllers();
	return RegisteredControllers.Num();
}

void URammsUISubsystem::CleanupStaleControllers()
{
	RegisteredControllers.RemoveAll([](const TWeakObjectPtr<AActor>& Weak) {
		return !Weak.IsValid();
	});
}

// ── UI Event Bus ──────────────────────────────────────────────────

void URammsUISubsystem::BroadcastTaskAction(ERammsTaskAction Action)
{
	OnTaskAction.Broadcast(Action);
}

void URammsUISubsystem::BroadcastArmTaskChanged(ERammsArmTask NewTask, ERammsArmTask PreviousTask)
{
	OnArmTaskChanged.Broadcast(NewTask, PreviousTask);
}

void URammsUISubsystem::BroadcastSeatStateChanged(ERammsSeatAxis Axis, float NewValue)
{
	OnSeatStateChanged.Broadcast(Axis, NewValue);
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

void URammsUISubsystem::BroadcastCustomUIEventWithPayload(FName EventName, const FInstancedStruct& Payload)
{
	FRammsUIEvent Event;
	Event.EventName = EventName;
	Event.StructPayload = Payload;
	OnCustomUIEvent.Broadcast(Event);
}

void URammsUISubsystem::BroadcastCustomUIEventWithProperties(FName EventName, const TMap<FName, FString>& EventProperties)
{
	FRammsUIEvent Event;
	Event.EventName = EventName;
	Event.Properties = EventProperties;
	OnCustomUIEvent.Broadcast(Event);
}

void URammsUISubsystem::BroadcastLayoutTransitionRequest(FName LayoutName, bool bAnimated,
	ERammsSlideDirection SlideDirection)
{
	OnLayoutTransitionRequested.Broadcast(LayoutName, bAnimated, SlideDirection);
}

void URammsUISubsystem::BroadcastDetections(FName SourceTag, const TArray<FRammsBoundingBox>& Boxes)
{
	OnDetectionsReceived.Broadcast(SourceTag, Boxes);
}

void URammsUISubsystem::ClearDetections(FName SourceTag)
{
	OnDetectionsCleared.Broadcast(SourceTag);
}

// ── Robot State ───────────────────────────────────────────────────

void URammsUISubsystem::BroadcastRobotStateChanged(const FRammsRobotState& State)
{
	CachedRobotState = State;
	bHasRobotState = true;
	OnRobotStateChanged.Broadcast(State);
}

void URammsUISubsystem::UpdateRobotMode(ERammsRobotMode Mode)
{
	CachedRobotState.Mode = Mode;
	bHasRobotState = true;
	OnRobotStateChanged.Broadcast(CachedRobotState);
}

void URammsUISubsystem::UpdateBatteryLevel(float Level)
{
	CachedRobotState.BatteryLevel = FMath::Clamp(Level, 0.0f, 1.0f);
	bHasRobotState = true;
	OnRobotStateChanged.Broadcast(CachedRobotState);
}

void URammsUISubsystem::UpdateSpeed(float SpeedMPS)
{
	CachedRobotState.LinearVelocity = FVector(SpeedMPS, 0.0f, 0.0f);
	bHasRobotState = true;
	OnRobotStateChanged.Broadcast(CachedRobotState);
}

void URammsUISubsystem::UpdateLinearVelocity(FVector Velocity)
{
	CachedRobotState.LinearVelocity = Velocity;
	bHasRobotState = true;
	OnRobotStateChanged.Broadcast(CachedRobotState);
}

void URammsUISubsystem::UpdateAngularVelocity(FRotator Velocity)
{
	CachedRobotState.AngularVelocity = Velocity;
	bHasRobotState = true;
	OnRobotStateChanged.Broadcast(CachedRobotState);
}

void URammsUISubsystem::UpdateEmergencyStop(bool bActive)
{
	CachedRobotState.bEmergencyStop = bActive;
	if (bActive)
	{
		CachedRobotState.Mode = ERammsRobotMode::Emergency;
	}
	bHasRobotState = true;
	OnRobotStateChanged.Broadcast(CachedRobotState);
}

void URammsUISubsystem::UpdateBasePose(FTransform Pose)
{
	CachedRobotState.BasePose = Pose;
	bHasRobotState = true;
	OnRobotStateChanged.Broadcast(CachedRobotState);
}

// ── Key-Value Property Store ──────────────────────────────────────

void URammsUISubsystem::SetProperty(FName Key, const FString& Value)
{
	PropertyStore.Add(Key, Value);
	OnPropertyChanged.Broadcast(Key, Value);
}

FString URammsUISubsystem::GetProperty(FName Key) const
{
	const FString* Found = PropertyStore.Find(Key);
	return Found ? *Found : FString();
}

float URammsUISubsystem::GetPropertyAsFloat(FName Key, float DefaultValue) const
{
	const FString* Found = PropertyStore.Find(Key);
	if (Found && !Found->IsEmpty())
	{
		if (Found->IsNumeric())
		{
			return FCString::Atof(**Found);
		}
	}
	return DefaultValue;
}

int32 URammsUISubsystem::GetPropertyAsInt(FName Key, int32 DefaultValue) const
{
	const FString* Found = PropertyStore.Find(Key);
	if (Found && !Found->IsEmpty())
	{
		if (Found->IsNumeric())
		{
			return FCString::Atoi(**Found);
		}
	}
	return DefaultValue;
}

bool URammsUISubsystem::GetPropertyAsBool(FName Key, bool DefaultValue) const
{
	const FString* Found = PropertyStore.Find(Key);
	if (!Found || Found->IsEmpty())
	{
		return DefaultValue;
	}
	return Found->Equals(TEXT("true"), ESearchCase::IgnoreCase)
		|| Found->Equals(TEXT("1"))
		|| Found->Equals(TEXT("yes"), ESearchCase::IgnoreCase);
}

bool URammsUISubsystem::HasProperty(FName Key) const
{
	return PropertyStore.Contains(Key);
}

void URammsUISubsystem::RemoveProperty(FName Key)
{
	if (PropertyStore.Remove(Key) > 0)
	{
		OnPropertyChanged.Broadcast(Key, FString());
	}
}

TArray<FName> URammsUISubsystem::GetAllPropertyKeys() const
{
	TArray<FName> Keys;
	PropertyStore.GetKeys(Keys);
	return Keys;
}

void URammsUISubsystem::SetProperties(const TMap<FName, FString>& Properties)
{
	for (const auto& Pair : Properties)
	{
		PropertyStore.Add(Pair.Key, Pair.Value);
		OnPropertyChanged.Broadcast(Pair.Key, Pair.Value);
	}
}

// ── Typed Property Setters ────────────────────────────────────────

void URammsUISubsystem::SetPropertyAsByte(FName Key, uint8 Value)
{
	SetProperty(Key, FString::FromInt(static_cast<int32>(Value)));
}

void URammsUISubsystem::SetPropertyAsFloat(FName Key, float Value)
{
	SetProperty(Key, FString::SanitizeFloat(Value));
}

void URammsUISubsystem::SetPropertyAsInt(FName Key, int32 Value)
{
	SetProperty(Key, FString::FromInt(Value));
}

void URammsUISubsystem::SetPropertyAsBool(FName Key, bool Value)
{
	SetProperty(Key, Value ? TEXT("true") : TEXT("false"));
}

uint8 URammsUISubsystem::GetPropertyAsByte(FName Key, uint8 DefaultValue) const
{
	const FString* Found = PropertyStore.Find(Key);
	return (Found && Found->IsNumeric()) ? static_cast<uint8>(FCString::Atoi(**Found)) : DefaultValue;
}

// ── Robot Command Dispatch ────────────────────────────────────────

bool URammsUISubsystem::SendRobotCommand(FName CommandName)
{
	AActor* Controller = FindRobotController();
	if (!Controller)
	{
		UE_LOG(LogTemp, Warning, TEXT("SendRobotCommand('%s'): No robot controller registered"), *CommandName.ToString());
		OnRobotCommandSent.Broadcast(CommandName, false);
		return false;
	}
	bool bAccepted = IRammsRobotController::Execute_SendCommand(Controller, CommandName);
	UE_LOG(LogTemp, Log, TEXT("SendRobotCommand('%s') → %s [%s]"), *CommandName.ToString(), bAccepted ? TEXT("accepted") : TEXT("rejected"), *Controller->GetName());
	OnRobotCommandSent.Broadcast(CommandName, bAccepted);
	return bAccepted;
}

bool URammsUISubsystem::SendRobotCommandWithValue(FName CommandName, uint8 Value)
{
	AActor* Controller = FindRobotController();
	if (!Controller)
	{
		UE_LOG(LogTemp, Warning, TEXT("SendRobotCommandWithValue('%s', %d): No robot controller registered"), *CommandName.ToString(), Value);
		OnRobotCommandSent.Broadcast(CommandName, false);
		return false;
	}
	bool bAccepted = IRammsRobotController::Execute_SendCommandWithValue(Controller, CommandName, Value);
	UE_LOG(LogTemp, Log, TEXT("SendRobotCommandWithValue('%s', %d) → %s [%s]"), *CommandName.ToString(), Value, bAccepted ? TEXT("accepted") : TEXT("rejected"), *Controller->GetName());
	OnRobotCommandSent.Broadcast(CommandName, bAccepted);
	return bAccepted;
}

bool URammsUISubsystem::SendRobotCommandWithPayload(FName CommandName, const FInstancedStruct& Payload)
{
	AActor* Controller = FindRobotController();
	if (!Controller)
	{
		UE_LOG(LogTemp, Warning, TEXT("SendRobotCommandWithPayload('%s'): No robot controller registered"), *CommandName.ToString());
		OnRobotCommandSent.Broadcast(CommandName, false);
		return false;
	}
	bool bAccepted = IRammsRobotController::Execute_SendCommandWithPayload(Controller, CommandName, Payload);
	UE_LOG(LogTemp, Log, TEXT("SendRobotCommandWithPayload('%s') → %s [%s]"), *CommandName.ToString(), bAccepted ? TEXT("accepted") : TEXT("rejected"), *Controller->GetName());
	OnRobotCommandSent.Broadcast(CommandName, bAccepted);
	return bAccepted;
}

// ── Theme Management ──────────────────────────────────────────────

void URammsUISubsystem::SetTheme(URammsUIStyle* NewStyle)
{
	if (NewStyle == CurrentTheme)
	{
		return;
	}

	CurrentTheme = NewStyle;

	UE_LOG(LogTemp, Log, TEXT("URammsUISubsystem::SetTheme('%s')"),
		NewStyle ? *NewStyle->StyleName : TEXT("null"));

	// Propagate to all live RammsBaseWidget instances
	for (TObjectIterator<URammsBaseWidget> It; It; ++It)
	{
		URammsBaseWidget* W = *It;
		if (!IsValid(W) || W->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			continue;
		}
		// Only propagate to widgets that are in a world (constructed and active)
		if (!W->GetWorld())
		{
			continue;
		}
		W->SetStyle(NewStyle);
	}

	OnThemeChanged.Broadcast(NewStyle);
}

void URammsUISubsystem::SetDarkTheme()
{
	if (DefaultDarkTheme)
	{
		SetTheme(DefaultDarkTheme);
		return;
	}
	if (!BuiltinDarkTheme)
	{
		BuiltinDarkTheme = URammsUIStyle::CreateDefaultDarkTheme();
	}
	SetTheme(BuiltinDarkTheme);
}

void URammsUISubsystem::SetLightTheme()
{
	if (DefaultLightTheme)
	{
		SetTheme(DefaultLightTheme);
		return;
	}
	if (!BuiltinLightTheme)
	{
		BuiltinLightTheme = URammsUIStyle::CreateDefaultLightTheme();
	}
	SetTheme(BuiltinLightTheme);
}

int32 URammsUISubsystem::BroadcastRobotCommand(FName CommandName)
{
	CleanupStaleControllers();
	if (RegisteredControllers.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BroadcastRobotCommand('%s'): No robot controllers registered"), *CommandName.ToString());
		OnRobotCommandSent.Broadcast(CommandName, false);
		return 0;
	}
	int32 Accepted = 0;
	for (const TWeakObjectPtr<AActor>& Weak : RegisteredControllers)
	{
		AActor* Actor = Weak.Get();
		if (Actor && IRammsRobotController::Execute_SendCommand(Actor, CommandName))
		{
			++Accepted;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("BroadcastRobotCommand('%s'): %d/%d controllers accepted"), *CommandName.ToString(), Accepted, RegisteredControllers.Num());
	OnRobotCommandSent.Broadcast(CommandName, Accepted > 0);
	return Accepted;
}

int32 URammsUISubsystem::BroadcastRobotCommandWithValue(FName CommandName, uint8 Value)
{
	CleanupStaleControllers();
	if (RegisteredControllers.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BroadcastRobotCommandWithValue('%s', %d): No robot controllers registered"), *CommandName.ToString(), Value);
		OnRobotCommandSent.Broadcast(CommandName, false);
		return 0;
	}
	int32 Accepted = 0;
	for (const TWeakObjectPtr<AActor>& Weak : RegisteredControllers)
	{
		AActor* Actor = Weak.Get();
		if (Actor && IRammsRobotController::Execute_SendCommandWithValue(Actor, CommandName, Value))
		{
			++Accepted;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("BroadcastRobotCommandWithValue('%s', %d): %d/%d controllers accepted"), *CommandName.ToString(), Value, Accepted, RegisteredControllers.Num());
	OnRobotCommandSent.Broadcast(CommandName, Accepted > 0);
	return Accepted;
}

int32 URammsUISubsystem::BroadcastRobotCommandWithPayload(FName CommandName, const FInstancedStruct& Payload)
{
	CleanupStaleControllers();
	if (RegisteredControllers.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BroadcastRobotCommandWithPayload('%s'): No robot controllers registered"), *CommandName.ToString());
		OnRobotCommandSent.Broadcast(CommandName, false);
		return 0;
	}
	int32 Accepted = 0;
	for (const TWeakObjectPtr<AActor>& Weak : RegisteredControllers)
	{
		AActor* Actor = Weak.Get();
		if (Actor && IRammsRobotController::Execute_SendCommandWithPayload(Actor, CommandName, Payload))
		{
			++Accepted;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("BroadcastRobotCommandWithPayload('%s'): %d/%d controllers accepted"), *CommandName.ToString(), Accepted, RegisteredControllers.Num());
	OnRobotCommandSent.Broadcast(CommandName, Accepted > 0);
	return Accepted;
}
