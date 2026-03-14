// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "UI/RammsImageButton.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/UniformGridPanel.h"
#include "RammsMebotController.generated.h"

/**
 * MEBot driving modes
 */
UENUM(BlueprintType)
enum class ERammsMebotMode : uint8
{
	None        UMETA(DisplayName = "None"),
	SelfLevel   UMETA(DisplayName = "Self-Levelling"),
	CurbAscent  UMETA(DisplayName = "Curb Ascent"),
	CurbDescent UMETA(DisplayName = "Curb Descent")
};

/**
 * Controller widget for MEBot wheelchair modes.
 * Displays a grid of image buttons for mode selection:
 * Self-Levelling, Curb Ascent, Curb Descent.
 * Only one mode can be active at a time.
 * Auto-discovers actors implementing IRammsRobotController.
 */
UCLASS(meta = (DisplayName = "Ramms MEBot Controller"))
class RAMMSUI_API URammsMebotController : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	URammsMebotController(const FObjectInitializer& ObjectInitializer);

protected:
	/** Header title */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MEBot")
	FText HeaderTitle = FText::FromString(TEXT("MEBot Control"));

	/** Number of columns in the mode grid */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MEBot", meta = (ClampMin = "1", ClampMax = "6"))
	int32 GridColumns = 3;

	/** Image button size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MEBot")
	FVector2D ButtonImageSize = FVector2D(72.0f, 72.0f);

	/** Icon for self-levelling mode (assign in editor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MEBot|Icons")
	TObjectPtr<UTexture2D> SelfLevelIcon;

	/** Icon for curb ascent mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MEBot|Icons")
	TObjectPtr<UTexture2D> CurbAscentIcon;

	/** Icon for curb descent mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MEBot|Icons")
	TObjectPtr<UTexture2D> CurbDescentIcon;

	/** Currently active mode */
	UPROPERTY(BlueprintReadOnly, Category = "MEBot")
	ERammsMebotMode CurrentMode = ERammsMebotMode::None;

	// Widget references
	UPROPERTY()
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> ModeGrid;

	UPROPERTY()
	TObjectPtr<URammsImageButton> SelfLevelButton;

	UPROPERTY()
	TObjectPtr<URammsImageButton> CurbAscentButton;

	UPROPERTY()
	TObjectPtr<URammsImageButton> CurbDescentButton;

public:
	/** Fired when mode changes */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMebotModeChanged, ERammsMebotMode, NewMode, ERammsMebotMode, PreviousMode);
	UPROPERTY(BlueprintAssignable, Category = "MEBot")
	FOnMebotModeChanged OnModeChanged;

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void ApplyStyle_Implementation() override;
	virtual void SynchronizeProperties() override;

	/** Set the current mode programmatically */
	UFUNCTION(BlueprintCallable, Category = "MEBot")
	void SetMode(ERammsMebotMode NewMode);

	/** Get the current mode */
	UFUNCTION(BlueprintPure, Category = "MEBot")
	ERammsMebotMode GetCurrentMode() const { return CurrentMode; }

	/** Cancel current mode (return to None) */
	UFUNCTION(BlueprintCallable, Category = "MEBot")
	void CancelMode();

	/** Set icon textures at runtime */
	UFUNCTION(BlueprintCallable, Category = "MEBot")
	void SetModeIcon(ERammsMebotMode Mode, UTexture2D* Icon);

protected:
	virtual void ResetCachedWidgets() override;
	virtual void BuildWidgetTree() override;
	void UpdateModeButtons();

	UFUNCTION()
	void OnSelfLevelClicked();

	UFUNCTION()
	void OnCurbAscentClicked();

	UFUNCTION()
	void OnCurbDescentClicked();

	void HandleModeButtonClicked(ERammsMebotMode Mode);

protected:
	virtual void OnRobotControllerResolved(AActor* ControllerActor) override;
};
