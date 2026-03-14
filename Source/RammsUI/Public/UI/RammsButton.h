// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "RammsButton.generated.h"

/**
 * Button state for visual feedback
 */
UENUM(BlueprintType)
enum class ERammsButtonState : uint8
{
	Normal,
	Hovered,
	Pressed,
	Disabled
};

/**
 * Styled button widget with hover/press states
 */
UCLASS(meta = (DisplayName = "Ramms Button"))
class RAMMSUI_API URammsButton : public URammsBaseWidget
{
	GENERATED_BODY()

protected:
	/** Button text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FText ButtonText = FText::FromString(TEXT("Button"));

	/** Whether button is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	bool bButtonEnabled = true;

	/** Use primary color (accent) instead of secondary */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	bool bUsePrimaryColor = false;

	/** Optional icon texture shown to the left of button text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	TObjectPtr<UTexture2D> IconTexture;

	// Widget references (built programmatically)
	UPROPERTY()
	TObjectPtr<UButton> InnerButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> ButtonLabel;

	UPROPERTY()
	TObjectPtr<UImage> ButtonIcon;

	/** Current button state */
	ERammsButtonState CurrentState = ERammsButtonState::Normal;

public:
	/** On button clicked delegate */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRammsButtonClicked);
	UPROPERTY(BlueprintAssignable, Category = "Button")
	FOnRammsButtonClicked OnClicked;

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void ApplyStyle_Implementation() override;

	/**
	 * Set icon texture (nullptr to hide)
	 */
	UFUNCTION(BlueprintCallable, Category = "Button")
	void SetIcon(UTexture2D* Texture);

	/**
	 * Set button text
	 */
	UFUNCTION(BlueprintCallable, Category = "Button")
	void SetText(FText Text);

	/**
	 * Set button enabled state
	 */
	UFUNCTION(BlueprintCallable, Category = "Button")
	void SetEnabled(bool bEnabled);

	/**
	 * Is button enabled?
	 */
	UFUNCTION(BlueprintPure, Category = "Button")
	bool IsButtonEnabled() const { return bButtonEnabled; }

protected:
	UFUNCTION()
	void OnButtonClicked();

	UFUNCTION()
	void OnButtonHovered();

	UFUNCTION()
	void OnButtonUnhovered();

	UFUNCTION()
	void OnButtonPressed();

	UFUNCTION()
	void OnButtonReleased();

	/** Update visual state */
	void UpdateVisualState();

	/** Build widget tree programmatically */
	virtual void BuildWidgetTree() override;
};
