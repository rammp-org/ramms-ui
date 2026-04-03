// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "UI/RammsImageButton.h"
#include "RammsRobotTypes.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "RammsArmController.generated.h"

/**
 * Controller widget for the 6DOF robot arm.
 * Provides Home and Retract action buttons.
 * Auto-discovers actors implementing IRammsRobotController.
 */
UCLASS(meta = (DisplayName = "Ramms Arm Controller"))
class RAMMSUI_API URammsArmController : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	URammsArmController(const FObjectInitializer& ObjectInitializer);

protected:
	/** Header title */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arm")
	FText HeaderTitle = FText::FromString(TEXT("Arm Control"));

	/** Image button size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arm")
	FVector2D ButtonImageSize = FVector2D(64.0f, 64.0f);

	/** Icon for Home action */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arm|Icons")
	TObjectPtr<UTexture2D> HomeIcon;

	/** Icon for Retract action */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arm|Icons")
	TObjectPtr<UTexture2D> RetractIcon;

	// Widget references (Transient — rebuilt programmatically)
	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY(Transient)
	TObjectPtr<URammsImageButton> HomeButton;

	UPROPERTY(Transient)
	TObjectPtr<URammsImageButton> RetractButton;

public:
	/** Fired when an arm action is triggered */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnArmAction, ERammsArmAction, Action);
	UPROPERTY(BlueprintAssignable, Category = "Arm")
	FOnArmAction OnArmAction;

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void ApplyStyle_Implementation() override;
	virtual void SynchronizeProperties() override;

	/** Set icon for an action */
	UFUNCTION(BlueprintCallable, Category = "Arm")
	void SetActionIcon(ERammsArmAction Action, UTexture2D* Icon);

	/** Enable/disable a specific action button */
	UFUNCTION(BlueprintCallable, Category = "Arm")
	void SetActionEnabled(ERammsArmAction Action, bool bEnabled);

protected:
	virtual void	 ResetCachedWidgets() override;
	virtual void	 BuildWidgetTree() override;
	virtual UWidget* GetRootWidgetForValidation() override { return PanelBorder; }

	UFUNCTION()
	void OnHomeClicked();

	UFUNCTION()
	void OnRetractClicked();

protected:
	virtual void OnRobotControllerResolved(AActor* ControllerActor) override;
};
