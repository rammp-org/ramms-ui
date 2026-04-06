// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "RammsTestWidget.generated.h"

/**
 * Simple test widget to verify URammsUIStyle and animation system
 * Builds its UI programmatically - no Widget Blueprint needed.
 */
UCLASS(meta = (DisplayName = "Ramms Test Widget"))
class RAMMSUI_API URammsTestWidget : public URammsBaseWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(Transient)
	TObjectPtr<UBorder> MainBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> FadeInButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> FadeOutButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SlideInButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ScaleInButton;

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void ApplyStyle_Implementation() override;

protected:
	virtual void	 ResetCachedWidgets() override;
	virtual void	 BuildWidgetTree() override;
	virtual UWidget* GetRootWidgetForValidation() override { return MainBorder; }

private:
	UButton* CreateTestButton(const FString& Name, const FString& Label);

	UFUNCTION()
	void OnFadeInClicked();

	UFUNCTION()
	void OnFadeOutClicked();

	UFUNCTION()
	void OnSlideInClicked();

	UFUNCTION()
	void OnScaleInClicked();
};
