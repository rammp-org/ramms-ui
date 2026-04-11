// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
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
 * Button color variant — maps to style palette colors
 */
UENUM(BlueprintType)
enum class ERammsButtonVariant : uint8
{
	Secondary UMETA(DisplayName = "Secondary"),
	Primary	  UMETA(DisplayName = "Primary"),
	Success	  UMETA(DisplayName = "Success"),
	Warning	  UMETA(DisplayName = "Warning"),
	Error	  UMETA(DisplayName = "Error"),
	Ghost	  UMETA(DisplayName = "Ghost")
};

/**
 * Styled button widget with rounded corners, clipping, and color variants.
 * Uses a Border root for visual rendering and a transparent UButton for interaction.
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

	/** Color variant — determines background color from style palette */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	ERammsButtonVariant Variant = ERammsButtonVariant::Secondary;

	/** Optional icon texture shown to the left of button text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	TObjectPtr<UTexture2D> IconTexture;

	/** Allow text to wrap to multiple lines */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	bool bAutoWrapText = false;

	/** When true, the button uses CustomSize instead of auto-sizing to content */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bUseCustomSize = false;

	/** Fixed button size in pixels (only used when bUseCustomSize is true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (EditCondition = "bUseCustomSize"))
	FVector2D CustomSize = FVector2D(120.0f, 40.0f);

	/** Content padding inside the button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FMargin ContentPadding = FMargin(12.0f, 6.0f);

	// Widget references (built programmatically — Transient prevents stale
	// serialization when placed inside a parent Widget Blueprint)
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ButtonBorder;

	UPROPERTY(Transient)
	TObjectPtr<UButton> InnerButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ButtonLabel;

	UPROPERTY(Transient)
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
	virtual void SynchronizeProperties() override;
	virtual void ApplyStyle_Implementation() override;

	UFUNCTION(BlueprintCallable, Category = "Button")
	void SetIcon(UTexture2D* Texture);

	UFUNCTION(BlueprintCallable, Category = "Button")
	void SetText(FText Text);

	UFUNCTION(BlueprintCallable, Category = "Button")
	void SetEnabled(bool bEnabled);

	/** Enable or disable text wrapping */
	UFUNCTION(BlueprintCallable, Category = "Button")
	void SetAutoWrapText(bool bWrap);

	/** Enable or disable custom sizing */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void SetUseCustomSize(bool bUseCustom);

	/** Set the custom size (also enables custom sizing) */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void SetCustomSize(FVector2D NewSize);

	/** Set the color variant at runtime */
	UFUNCTION(BlueprintCallable, Category = "Style")
	void SetVariant(ERammsButtonVariant NewVariant);

	UFUNCTION(BlueprintPure, Category = "Button")
	bool IsButtonEnabled() const { return bButtonEnabled; }

	UFUNCTION(BlueprintPure, Category = "Style")
	ERammsButtonVariant GetVariant() const { return Variant; }

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

	/** Update visual state (border color, text color, opacity) */
	void UpdateVisualState();

	/** Get the base color for the current variant from the style palette */
	FLinearColor GetVariantColor() const;

	/** Build widget tree programmatically */
	virtual void	 ResetCachedWidgets() override;
	virtual void	 BuildWidgetTree() override;
	virtual UWidget* GetRootWidgetForValidation() override { return RootSizeBox ? (UWidget*)RootSizeBox : (UWidget*)ButtonBorder; }

	/** Apply size overrides to the root SizeBox based on bUseCustomSize */
	void ApplySizeOverrides();
};
