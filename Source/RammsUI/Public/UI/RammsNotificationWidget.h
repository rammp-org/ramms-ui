// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "RammsNotificationWidget.generated.h"

/**
 * Notification severity levels
 */
UENUM(BlueprintType)
enum class ERammsNotificationLevel : uint8
{
	Info,
	Success,
	Warning,
	Error
};

/**
 * Toast/notification popup widget.
 * Auto-dismisses after a configurable duration.
 * Can be stacked by a notification manager.
 */
UCLASS(meta = (DisplayName = "Ramms Notification"))
class RAMMSUI_API URammsNotificationWidget : public URammsBaseWidget
{
	GENERATED_BODY()

protected:
	/** Notification message */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification")
	FText Message = FText::FromString(TEXT("Notification"));

	/** Optional title */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification")
	FText Title;

	/** Severity level (affects color) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification")
	ERammsNotificationLevel Level = ERammsNotificationLevel::Info;

	/** Auto-dismiss duration (0 = manual dismiss only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification", meta = (ClampMin = "0.0"))
	float Duration = 4.0f;

	/** Whether notification can be dismissed by clicking */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification")
	bool bDismissOnClick = true;

	// Widget references (Transient — rebuilt programmatically)
	UPROPERTY(Transient)
	TObjectPtr<UBorder> NotificationBorder;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> AccentBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageLabel;

	/** Time remaining before auto-dismiss */
	float TimeRemaining = 0.0f;

	/** Has been dismissed */
	bool bDismissed = false;

public:
	/** Fired when notification is dismissed (click or timeout) */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNotificationDismissed);
	UPROPERTY(BlueprintAssignable, Category = "Notification")
	FOnNotificationDismissed OnDismissed;

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void ApplyStyle_Implementation() override;

	/** Show the notification with specified parameters */
	UFUNCTION(BlueprintCallable, Category = "Notification")
	void Show(FText InMessage, ERammsNotificationLevel InLevel = ERammsNotificationLevel::Info, float InDuration = 4.0f, FText InTitle = FText());

	/** Pre-set content before the widget is added to a parent (avoids double animation) */
	void SetNotificationContent(FText InMessage, ERammsNotificationLevel InLevel, float InDuration, FText InTitle);

	/** Dismiss the notification */
	UFUNCTION(BlueprintCallable, Category = "Notification")
	void Dismiss();

	/** Get the notification level */
	UFUNCTION(BlueprintPure, Category = "Notification")
	ERammsNotificationLevel GetLevel() const { return Level; }

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual void	 ResetCachedWidgets() override;
	virtual void	 BuildWidgetTree() override;
	virtual UWidget* GetRootWidgetForValidation() override { return NotificationBorder; }
	FLinearColor	 GetAccentColor() const;
};
