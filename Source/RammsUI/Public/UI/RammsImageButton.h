// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Overlay.h"
#include "Components/VerticalBox.h"
#include "Components/SizeBox.h"
#include "RammsImageButton.generated.h"

/**
 * A styled button with a prominent image and optional label.
 * Supports hover/press feedback, toggle/active state, and custom sizing.
 */
UCLASS(meta = (DisplayName = "Ramms Image Button"))
class RAMMSUI_API URammsImageButton : public URammsBaseWidget
{
	GENERATED_BODY()

protected:
	/** Button label text (shown below or over the image) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageButton")
	FText LabelText;

	/** Image texture displayed on the button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageButton")
	TObjectPtr<UTexture2D> ButtonImage;

	/** Image size in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageButton")
	FVector2D ImageSize = FVector2D(64.0f, 64.0f);

	/** Whether this button can toggle on/off */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageButton")
	bool bIsToggle = false;

	/** Current toggle state (only used if bIsToggle) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageButton")
	bool bIsActive = false;

	/** Whether button is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageButton")
	bool bButtonEnabled = true;

	/** Show label below the image */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageButton")
	bool bShowLabel = true;

	/** Allow label text to wrap to multiple lines */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageButton")
	bool bAutoWrapLabel = false;

	/** When true, the button uses CustomSize instead of auto-sizing to content */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bUseCustomSize = false;

	/** Fixed button size in pixels (only used when bUseCustomSize is true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (EditCondition = "bUseCustomSize"))
	FVector2D CustomSize = FVector2D(80.0f, 100.0f);

	/** Padding around image content */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	float ContentPadding = 8.0f;

	/** Border thickness for active highlight */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	float ActiveBorderWidth = 3.0f;

	/** When true, the style's IconTint color is applied to the image.
	 *  Disable for full-color / photographic images that should not be tinted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	bool bApplyIconTint = true;

	// Widget references (Transient — rebuilt programmatically)
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ButtonBorder;

	UPROPERTY(Transient)
	TObjectPtr<UButton> InnerButton;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ContentImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ActiveBorder;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> ImageSizeBox;

	bool bIsHovered = false;
	bool bIsPressed = false;

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnImageButtonClicked);
	UPROPERTY(BlueprintAssignable, Category = "ImageButton")
	FOnImageButtonClicked OnClicked;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnImageButtonToggled, bool, bActive);
	UPROPERTY(BlueprintAssignable, Category = "ImageButton")
	FOnImageButtonToggled OnToggled;

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void SynchronizeProperties() override;
	virtual void ApplyStyle_Implementation() override;

	/** Set the button image */
	UFUNCTION(BlueprintCallable, Category = "ImageButton")
	void SetButtonImage(UTexture2D* Texture);

	/** Set the label text */
	UFUNCTION(BlueprintCallable, Category = "ImageButton")
	void SetLabelText(FText Text);

	/** Set the active/toggle state */
	UFUNCTION(BlueprintCallable, Category = "ImageButton")
	void SetActive(bool bNewActive);

	/** Get the active state */
	UFUNCTION(BlueprintPure, Category = "ImageButton")
	bool IsActive() const { return bIsActive; }

	/** Set enabled state */
	UFUNCTION(BlueprintCallable, Category = "ImageButton")
	void SetButtonEnabled(bool bEnabled);

	/** Set image size */
	UFUNCTION(BlueprintCallable, Category = "ImageButton")
	void SetImageSize(FVector2D NewSize);

	/** Show or hide the label below the image */
	UFUNCTION(BlueprintCallable, Category = "ImageButton")
	void SetShowLabel(bool bShow);

	/** Enable or disable auto-wrapping on the label text */
	UFUNCTION(BlueprintCallable, Category = "ImageButton")
	void SetLabelAutoWrap(bool bAutoWrap);

	/** Enable or disable custom sizing */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void SetUseCustomSize(bool bUseCustom);

	/** Set the custom size (also enables custom sizing) */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void SetCustomSize(FVector2D NewSize);

	/** Enable or disable style-driven icon tinting.
	 *  Disable for full-color / photographic images that should not be tinted. */
	UFUNCTION(BlueprintCallable, Category = "Style")
	void SetApplyIconTint(bool bApply);

protected:
	virtual void	 ResetCachedWidgets() override;
	virtual void	 BuildWidgetTree() override;
	virtual UWidget* GetRootWidgetForValidation() override { return RootSizeBox ? (UWidget*)RootSizeBox : (UWidget*)ButtonBorder; }
	void			 UpdateVisualState();

	/** Apply size overrides to the root SizeBox based on bUseCustomSize */
	void ApplySizeOverrides();

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

	UFUNCTION()
	void HandlePressed();

	UFUNCTION()
	void HandleReleased();
};
