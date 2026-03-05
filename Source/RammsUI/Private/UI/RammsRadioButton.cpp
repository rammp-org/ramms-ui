// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsRadioButton.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBoxSlot.h"

// Initialize static radio groups map
TMap<FName, TArray<TWeakObjectPtr<URammsRadioButton>>> URammsRadioButton::RadioGroups;

void URammsRadioButton::BuildWidgetTree()
{
	if (!WidgetTree || InnerCheckBox)
		return; // Already built or no tree

	// Root: HorizontalBox
	UHorizontalBox* RootBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RootHBox"));
	WidgetTree->RootWidget = RootBox;

	// Checkbox
	InnerCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("InnerCheckBox"));
	UHorizontalBoxSlot* CheckSlot = Cast<UHorizontalBoxSlot>(RootBox->AddChildToHorizontalBox(InnerCheckBox));
	if (CheckSlot)
	{
		CheckSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		CheckSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Label
	RadioLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RadioLabel"));
	RadioLabel->SetText(ButtonText);
	UHorizontalBoxSlot* LabelSlot = Cast<UHorizontalBoxSlot>(RootBox->AddChildToHorizontalBox(RadioLabel));
	if (LabelSlot)
	{
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void URammsRadioButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Build widget tree early so it exists before Slate representation is created
	// (required for widgets added as children of other panels)
	BuildWidgetTree();
}

void URammsRadioButton::NativeConstruct()
{
	Super::NativeConstruct();

	if (InnerCheckBox)
	{
		InnerCheckBox->SetIsChecked(bIsChecked);
		InnerCheckBox->OnCheckStateChanged.AddDynamic(this, &URammsRadioButton::OnCheckBoxChanged);
	}

	if (RadioLabel)
	{
		RadioLabel->SetText(ButtonText);
	}

	// Register with radio group
	if (GroupID != NAME_None)
	{
		if (!RadioGroups.Contains(GroupID))
		{
			RadioGroups.Add(GroupID, TArray<TWeakObjectPtr<URammsRadioButton>>());
		}
		RadioGroups[GroupID].Add(this);
	}
}

void URammsRadioButton::NativeDestruct()
{
	// Unregister from radio group
	if (GroupID != NAME_None && RadioGroups.Contains(GroupID))
	{
		RadioGroups[GroupID].Remove(this);
		
		// Clean up empty groups
		if (RadioGroups[GroupID].Num() == 0)
		{
			RadioGroups.Remove(GroupID);
		}
	}

	Super::NativeDestruct();
}

void URammsRadioButton::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	// Apply label styling
	if (RadioLabel)
	{
		RadioLabel->SetFont(Style->Typography.Body);
		RadioLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}

	// CheckBox styling would require UCheckBoxStyle asset
	// In production, create and reference a styled checkbox asset
}

void URammsRadioButton::SetChecked(bool bChecked)
{
	if (bIsChecked == bChecked)
		return;

	bIsChecked = bChecked;

	if (InnerCheckBox)
	{
		InnerCheckBox->SetIsChecked(bIsChecked);
	}

	// If checking this button, uncheck others in group
	if (bIsChecked)
	{
		UncheckGroup();
	}

	OnCheckedChanged.Broadcast(bIsChecked, Value);
}

void URammsRadioButton::SetText(FText Text)
{
	ButtonText = Text;
	if (RadioLabel)
	{
		RadioLabel->SetText(ButtonText);
	}
}

void URammsRadioButton::OnCheckBoxChanged(bool bNewChecked)
{
	// Radio buttons can only be checked, not unchecked by user
	// (They can only be unchecked when another in the group is checked)
	if (!bNewChecked && bIsChecked)
	{
		// User tried to uncheck - restore checked state
		if (InnerCheckBox)
		{
			InnerCheckBox->SetIsChecked(true);
		}
		return;
	}

	if (bNewChecked && !bIsChecked)
	{
		SetChecked(true);
	}
}

void URammsRadioButton::UncheckGroup()
{
	if (GroupID == NAME_None || !RadioGroups.Contains(GroupID))
		return;

	TArray<TWeakObjectPtr<URammsRadioButton>>& Group = RadioGroups[GroupID];

	// Uncheck all other buttons in the group
	for (int32 i = Group.Num() - 1; i >= 0; --i)
	{
		if (!Group[i].IsValid())
		{
			// Clean up invalid references
			Group.RemoveAt(i);
			continue;
		}

		URammsRadioButton* OtherButton = Group[i].Get();
		if (OtherButton && OtherButton != this && OtherButton->bIsChecked)
		{
			OtherButton->bIsChecked = false;
			if (OtherButton->InnerCheckBox)
			{
				OtherButton->InnerCheckBox->SetIsChecked(false);
			}
			OtherButton->OnCheckedChanged.Broadcast(false, OtherButton->Value);
		}
	}
}
