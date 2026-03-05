// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/RammsNotificationWidget.h"
#include "RammsNotificationContainer.generated.h"

class UVerticalBox;

/**
 * Viewport-level container that holds and stacks notification toasts.
 * Anchored to the top-right of the screen; notifications are added
 * to a vertical box and automatically removed when dismissed.
 */
UCLASS()
class RAMMSUI_API URammsNotificationContainer : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Add a notification to the stack. The container takes ownership of layout. */
	void AddNotification(URammsNotificationWidget* Notification);

	/** Dismiss and remove all notifications. Returns count dismissed. */
	int32 DismissAll();

	/** Get the number of active (non-dismissed) notifications. */
	int32 GetActiveCount() const;

protected:
	UPROPERTY()
	TObjectPtr<UVerticalBox> NotificationStack;

	/** Track active notifications for cleanup */
	TArray<TWeakObjectPtr<URammsNotificationWidget>> ActiveNotifications;

	/** Notifications pending removal after dismiss animation */
	TArray<TWeakObjectPtr<URammsNotificationWidget>> PendingRemoval;

	/** Style-driven spacing between notifications */
	float ItemSpacing = 4.0f;

	/** Padding from screen edges */
	float EdgePadding = 16.0f;

	/** Notification width */
	float NotificationWidth = 350.0f;

	/** Timer for delayed removal of dismissed notifications */
	float RemovalTimer = 0.0f;
};
