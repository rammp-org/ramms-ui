// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "UI/RammsImageButton.h"
#include "RammsRobotTypes.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/UniformGridPanel.h"
#include "RammsArmTaskWidget.generated.h"

/**
 * Controller widget for arm-level task selection.
 * Displays a grid of image buttons for mutually exclusive tasks:
 * Open Door, Order Drink, Drink.
 * Only one task can be active at a time (toggle to cancel).
 * Auto-discovers actors implementing IRammsRobotController.
 * Broadcasts task changes via both its own delegate and the UI event bus.
 */
UCLASS(meta = (DisplayName = "Ramms Arm Task Widget"))
class RAMMSUI_API URammsArmTaskWidget : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	URammsArmTaskWidget(const FObjectInitializer& ObjectInitializer);

protected:
	/** Header title */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arm Task")
	FText HeaderTitle = FText::FromString(TEXT("Arm Tasks"));

	/** Number of columns in the task grid */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arm Task", meta = (ClampMin = "1", ClampMax = "6"))
	int32 GridColumns = 3;

	/** Image button size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arm Task")
	FVector2D ButtonImageSize = FVector2D(72.0f, 72.0f);

	/** Icon for Open Door task (assign in editor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arm Task|Icons")
	TObjectPtr<UTexture2D> OpenDoorIcon;

	/** Icon for Order Drink task */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arm Task|Icons")
	TObjectPtr<UTexture2D> OrderDrinkIcon;

	/** Icon for Drink task */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arm Task|Icons")
	TObjectPtr<UTexture2D> DrinkIcon;

	/** Icon for Stabilization task */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arm Task|Icons")
	TObjectPtr<UTexture2D> StabilizationIcon;

	/** Currently active task */
	UPROPERTY(BlueprintReadOnly, Category = "Arm Task")
	ERammsArmTask CurrentTask = ERammsArmTask::None;

	// Widget references
	UPROPERTY()
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> TaskGrid;

	UPROPERTY()
	TObjectPtr<URammsImageButton> OpenDoorButton;

	UPROPERTY()
	TObjectPtr<URammsImageButton> OrderDrinkButton;

	UPROPERTY()
	TObjectPtr<URammsImageButton> DrinkButton;

	UPROPERTY()
	TObjectPtr<URammsImageButton> StabilizationButton;

public:
	/** Fired when arm task changes */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnArmTaskChanged, ERammsArmTask, NewTask, ERammsArmTask, PreviousTask);
	UPROPERTY(BlueprintAssignable, Category = "Arm Task")
	FOnArmTaskChanged OnTaskChanged;

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void ApplyStyle_Implementation() override;
	virtual void SynchronizeProperties() override;

	/** Set the current task programmatically */
	UFUNCTION(BlueprintCallable, Category = "Arm Task")
	void SetTask(ERammsArmTask NewTask);

	/** Get the current task */
	UFUNCTION(BlueprintPure, Category = "Arm Task")
	ERammsArmTask GetCurrentTask() const { return CurrentTask; }

	/** Cancel the current task (return to None) */
	UFUNCTION(BlueprintCallable, Category = "Arm Task")
	void CancelTask();

	/** Set icon textures at runtime */
	UFUNCTION(BlueprintCallable, Category = "Arm Task")
	void SetTaskIcon(ERammsArmTask Task, UTexture2D* Icon);

protected:
	virtual void ResetCachedWidgets() override;
	virtual void BuildWidgetTree() override;
	void		 UpdateTaskButtons();

	UFUNCTION()
	void OnOpenDoorClicked();

	UFUNCTION()
	void OnOrderDrinkClicked();

	UFUNCTION()
	void OnDrinkClicked();

	UFUNCTION()
	void OnStabilizationClicked();

	void HandleTaskButtonClicked(ERammsArmTask Task);

protected:
	virtual void OnRobotControllerResolved(AActor* ControllerActor) override;
};
