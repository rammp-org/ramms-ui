// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Interfaces/IRammsStateProvider.h"
#include "UI/RammsNotificationWidget.h"
#include "UI/RammsNotificationContainer.h"
#include "RammsRemoteBridge.generated.h"

/**
 * Static function library for Remote Control API integration (UI-specific).
 * Provides functions callable via PUT /remote/object/call on the CDO path:
 *   /Script/RammsUI.Default__URammsRemoteBridge
 *
 * For generic actor/component discovery, use URammsCoreBridge in RammsCore.
 *
 * These use TObjectIterator to find runtime widget instances without needing
 * a world context, making them accessible from the Remote Control HTTP API.
 */
UCLASS()
class RAMMSUI_API URammsRemoteBridge : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ── UI Widget Discovery ───────────────────────────────────────

	/**
	 * Get object paths for all live URammsBaseWidget instances.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Remote")
	static TArray<FString> GetAllRammsWidgetPaths();

	/**
	 * Get object paths filtered by class name substring.
	 * e.g. "StatusPanel", "Toolbar", "MebotController"
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Remote")
	static TArray<FString> FindRammsWidgets(const FString& ClassNameFilter);

	// ── Status Panel ───────────────────────────────────────────────

	/**
	 * Set the robot state on all active StatusPanel widgets.
	 * Returns the number of panels updated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Remote")
	static int32 SetRobotMode(ERammsRobotMode Mode);

	UFUNCTION(BlueprintCallable, Category = "Ramms|Remote")
	static int32 SetBatteryLevel(float Level);

	UFUNCTION(BlueprintCallable, Category = "Ramms|Remote")
	static int32 SetSpeed(float SpeedMPS);

	UFUNCTION(BlueprintCallable, Category = "Ramms|Remote")
	static int32 SetEmergencyStop(bool bActive);

	/**
	 * Set the full robot state at once. Returns number of panels updated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Remote")
	static int32 SetRobotState(FRammsRobotState State);

	/**
	 * Get the current robot state from the first found state provider.
	 * Returns false if no provider is found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Remote")
	static bool GetRobotState(FRammsRobotState& OutState);

	// ── Notifications ─────────────────────────────────────────────

	/**
	 * Show a notification toast in the UI.
	 * Creates a new notification widget, adds it to the viewport, and calls Show().
	 * Notifications auto-stack vertically from the top-right.
	 *
	 * @param Message  Notification body text
	 * @param Level    Severity (Info, Success, Warning, Error)
	 * @param Duration Auto-dismiss time in seconds (0 = manual dismiss only)
	 * @param Title    Optional bold title line
	 * @return true if the notification was created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Remote")
	static bool ShowNotification(const FString& Message,
		ERammsNotificationLevel					Level = ERammsNotificationLevel::Info,
		float									Duration = 4.0f,
		const FString&							Title = TEXT(""));

	/**
	 * Dismiss all active notifications.
	 * @return Number of notifications dismissed
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Remote")
	static int32 DismissAllNotifications();

	// ── Key-Value Properties ──────────────────────────────────────

	/**
	 * Set a named property in the UI subsystem's property store.
	 * Broadcasts OnPropertyChanged to all subscribers.
	 * @return true if a play world with the subsystem was found
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Remote")
	static bool SetProperty(FName Key, const FString& Value);

	/**
	 * Set multiple properties at once.
	 * @return true if a play world with the subsystem was found
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Remote")
	static bool SetProperties(const TMap<FName, FString>& Properties);

	/**
	 * Get a property value from the UI subsystem.
	 * @return The property value, or empty string if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Remote")
	static FString GetProperty(FName Key);

	/**
	 * Get all property keys currently stored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Remote")
	static TArray<FName> GetAllPropertyKeys();

	/**
	 * Remove a property from the store. Broadcasts OnPropertyChanged with empty value.
	 * @return true if the property existed and was removed
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Remote")
	static bool RemoveProperty(FName Key);

private:
	/** Lazily-created viewport container for notifications */
	static TWeakObjectPtr<URammsNotificationContainer> NotificationContainer;

	/** Get or create the notification container widget */
	static URammsNotificationContainer* GetOrCreateContainer();
};
