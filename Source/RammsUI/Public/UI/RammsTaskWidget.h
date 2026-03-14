// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "UI/RammsImageButton.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "RammsTaskWidget.generated.h"

/**
 * Task action types
 */
UENUM(BlueprintType)
enum class ERammsTaskAction : uint8
{
	Exit   UMETA(DisplayName = "Exit"),
	Cancel UMETA(DisplayName = "Cancel")
};

/**
 * Task widget with Exit and Cancel action buttons.
 * Used during active tasks/modes to provide abort/complete controls.
 */
UCLASS(meta = (DisplayName = "Ramms Task Widget"))
class RAMMSUI_API URammsTaskWidget : public URammsBaseWidget
{
	GENERATED_BODY()

protected:
	/** Header title (e.g., current task name) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FText HeaderTitle = FText::FromString(TEXT("Active Task"));

	/** Status text shown between header and buttons */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FText StatusText;

	/** Image button size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FVector2D ButtonImageSize = FVector2D(48.0f, 48.0f);

	/** Icon for Exit action */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task|Icons")
	TObjectPtr<UTexture2D> ExitIcon;

	/** Icon for Cancel action */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task|Icons")
	TObjectPtr<UTexture2D> CancelIcon;

	// Widget references
	UPROPERTY()
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusLabel;

	UPROPERTY()
	TObjectPtr<URammsImageButton> ExitButton;

	UPROPERTY()
	TObjectPtr<URammsImageButton> CancelButton;

public:
	/** Fired when a task action is triggered */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaskAction, ERammsTaskAction, Action);
	UPROPERTY(BlueprintAssignable, Category = "Task")
	FOnTaskAction OnTaskAction;

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void ApplyStyle_Implementation() override;
	virtual void SynchronizeProperties() override;

	/** Set the header/task title */
	UFUNCTION(BlueprintCallable, Category = "Task")
	void SetHeaderTitle(FText Title);

	/** Set the status text */
	UFUNCTION(BlueprintCallable, Category = "Task")
	void SetStatusText(FText Text);

	/** Set icon for an action */
	UFUNCTION(BlueprintCallable, Category = "Task")
	void SetActionIcon(ERammsTaskAction Action, UTexture2D* Icon);

	/** Enable/disable a specific action button */
	UFUNCTION(BlueprintCallable, Category = "Task")
	void SetActionEnabled(ERammsTaskAction Action, bool bEnabled);

protected:
	virtual void BuildWidgetTree() override;

	UFUNCTION()
	void OnExitClicked();

	UFUNCTION()
	void OnCancelClicked();
};
