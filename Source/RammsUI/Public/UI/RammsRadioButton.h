// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "RammsRadioButton.generated.h"

/**
 * Styled radio button widget (part of a group)
 */
UCLASS(meta = (DisplayName = "Ramms Radio Button"))
class RAMMSUI_API URammsRadioButton : public URammsBaseWidget
{
	GENERATED_BODY()

protected:
	/** Radio button label */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radio")
	FText ButtonText = FText::FromString(TEXT("Option"));

	/** Radio group ID (radio buttons with same group ID are mutually exclusive) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radio")
	FName GroupID = NAME_None;

	/** Value represented by this radio button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radio")
	int32 Value = 0;

	/** Whether this radio button is checked */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radio")
	bool bIsChecked = false;

	// Widget references (built programmatically)
	UPROPERTY()
	TObjectPtr<UCheckBox> InnerCheckBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> RadioLabel;

	/** Static map of radio groups (for mutual exclusion) */
	static TMap<FName, TArray<TWeakObjectPtr<URammsRadioButton>>> RadioGroups;

public:
	/** On checked state changed delegate */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRammsRadioChanged, bool, bChecked, int32, SelectedValue);
	UPROPERTY(BlueprintAssignable, Category = "Radio")
	FOnRammsRadioChanged OnCheckedChanged;

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void ApplyStyle_Implementation() override;

	/**
	 * Set checked state
	 */
	UFUNCTION(BlueprintCallable, Category = "Radio")
	void SetChecked(bool bChecked);

	/**
	 * Is this radio button checked?
	 */
	UFUNCTION(BlueprintPure, Category = "Radio")
	bool IsChecked() const { return bIsChecked; }

	/**
	 * Get the value of this radio button
	 */
	UFUNCTION(BlueprintPure, Category = "Radio")
	int32 GetValue() const { return Value; }

	/**
	 * Set button text
	 */
	UFUNCTION(BlueprintCallable, Category = "Radio")
	void SetText(FText Text);

protected:
	UFUNCTION()
	void OnCheckBoxChanged(bool bNewChecked);

	/** Uncheck all other radio buttons in the same group */
	void UncheckGroup();

	/** Build widget tree programmatically */
	virtual void BuildWidgetTree() override;
};
