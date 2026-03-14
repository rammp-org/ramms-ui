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
	UPROPERTY()
	TObjectPtr<UBorder> MainBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UButton> FadeInButton;

	UPROPERTY()
	TObjectPtr<UButton> FadeOutButton;

	UPROPERTY()
	TObjectPtr<UButton> SlideInButton;

	UPROPERTY()
	TObjectPtr<UButton> ScaleInButton;

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void ApplyStyle_Implementation() override;

protected:
	virtual void BuildWidgetTree() override;

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
