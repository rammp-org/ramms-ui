// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsTaskSelector.h"
#include "UI/RammsImageButton.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/UniformGridSlot.h"
#include "Components/SizeBox.h"
#include "Components/ButtonSlot.h"

// ── Lifecycle ──────────────────────────────────────────────────────

void URammsTaskSelector::ResetCachedWidgets()
{
	RootBorder = nullptr;
	ButtonContainer = nullptr;
	ButtonMap.Empty();
}

void URammsTaskSelector::BuildWidgetTree()
{
	if (!WidgetTree || RootBorder)
		return;

	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RootBorder"));
	RootBorder->SetPadding(FMargin(ContentPadding));
	RootBorder->SetBrushColor(FLinearColor::Transparent);
	WidgetTree->RootWidget = RootBorder;

	RebuildContainer();
	RebuildButtons();
}

void URammsTaskSelector::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsTaskSelector::NativeConstruct()
{
	Super::NativeConstruct();

	// Sync button active states with SelectedValue
	for (auto& Pair : ButtonMap)
	{
		if (Pair.Value)
		{
			Pair.Value->SetActive(Pair.Key == SelectedValue);
		}
	}
}

void URammsTaskSelector::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (RootBorder)
	{
		RootBorder->SetPadding(FMargin(ContentPadding));
	}

	// Full rebuild so designer reflects property changes
	if (ButtonContainer)
	{
		RebuildContainer();
		RebuildButtons();
	}
}

void URammsTaskSelector::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	for (auto& Pair : ButtonMap)
	{
		if (Pair.Value)
		{
			Pair.Value->SetStyle(Style);
		}
	}
}

// ── Public API ─────────────────────────────────────────────────────

void URammsTaskSelector::SelectTask(uint8 EnumValue)
{
	if (SelectedValue == EnumValue)
		return;

	// Deactivate previous
	if (TObjectPtr<URammsImageButton>* Prev = ButtonMap.Find(SelectedValue))
	{
		if (*Prev)
			(*Prev)->SetActive(false);
	}

	SelectedValue = EnumValue;

	// Activate new
	if (TObjectPtr<URammsImageButton>* Next = ButtonMap.Find(SelectedValue))
	{
		if (*Next)
			(*Next)->SetActive(true);
	}

	OnTaskSelected.Broadcast(SelectedValue);
}

void URammsTaskSelector::ClearSelection()
{
	if (SelectedValue == 255)
		return;

	if (TObjectPtr<URammsImageButton>* Prev = ButtonMap.Find(SelectedValue))
	{
		if (*Prev)
			(*Prev)->SetActive(false);
	}

	SelectedValue = 255;
	OnTaskDeselected.Broadcast();
}

void URammsTaskSelector::SetTaskEnabled(uint8 EnumValue, bool bEnabled)
{
	if (TObjectPtr<URammsImageButton>* Btn = ButtonMap.Find(EnumValue))
	{
		if (*Btn)
			(*Btn)->SetButtonEnabled(bEnabled);
	}

	for (FRammsTaskDefinition& Def : Tasks)
	{
		if (Def.EnumValue == EnumValue)
		{
			Def.bEnabled = bEnabled;
			break;
		}
	}
}

// ── Container Management ───────────────────────────────────────────

void URammsTaskSelector::RebuildContainer()
{
	if (!RootBorder || !WidgetTree)
		return;

	if (ButtonContainer)
	{
		ButtonContainer->RemoveFromParent();
		ButtonContainer = nullptr;
	}

	// UniformGridPanel is preferred when uniform sizing is on — it naturally
	// makes every cell the same size.  When MaxPerRow > 0, it also handles
	// wrapping.  For single-line uniform layouts, set MaxPerRow to a large
	// value to keep everything on one row.
	if (bUniformButtonSize || MaxPerRow > 0)
	{
		UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(
			UUniformGridPanel::StaticClass(), TEXT("ButtonGrid"));
		Grid->SetSlotPadding(FMargin(ButtonSpacing * 0.5f));
		ButtonContainer = Grid;
	}
	else if (Orientation == Orient_Horizontal)
	{
		ButtonContainer = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("ButtonHBox"));
	}
	else
	{
		ButtonContainer = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("ButtonVBox"));
	}

	RootBorder->AddChild(ButtonContainer);
}

// ── Button Building ────────────────────────────────────────────────

TArray<FRammsTaskDefinition> URammsTaskSelector::BuildMergedTaskList() const
{
	TArray<FRammsTaskDefinition> Merged;

	if (bAutoGenerateFromEnum && TaskEnum)
	{
		// Full overrides take priority
		TMap<uint8, const FRammsTaskDefinition*> Overrides;
		for (const FRammsTaskDefinition& Def : Tasks)
		{
			Overrides.Add(Def.EnumValue, &Def);
		}

		// Icon-only overrides for enum values not covered by full overrides
		TMap<uint8, UTexture2D*> IconMap;
		for (const FRammsTaskIconMapping& Mapping : IconOverrides)
		{
			IconMap.Add(Mapping.EnumValue, Mapping.Icon);
		}

		const int32 Count = TaskEnum->NumEnums() - 1; // skip _MAX
		for (int32 i = 0; i < Count; ++i)
		{
			const uint8 Val = static_cast<uint8>(TaskEnum->GetValueByIndex(i));

			if (const FRammsTaskDefinition* const* Override = Overrides.Find(Val))
			{
				FRammsTaskDefinition Def = **Override;
				// If the full override has no image, check icon overrides
				if (!Def.Image)
				{
					if (UTexture2D** IconPtr = IconMap.Find(Val))
					{
						Def.Image = *IconPtr;
					}
				}
				Merged.Add(MoveTemp(Def));
			}
			else
			{
				FRammsTaskDefinition Auto;
				Auto.EnumValue = Val;
				Auto.Label = TaskEnum->GetDisplayNameTextByIndex(i);
				Auto.bEnabled = true;

				// Apply icon override if available
				if (UTexture2D** IconPtr = IconMap.Find(Val))
				{
					Auto.Image = *IconPtr;
				}

				Merged.Add(MoveTemp(Auto));
			}
		}
	}
	else
	{
		Merged = Tasks;
	}

	return Merged;
}

void URammsTaskSelector::RebuildButtons()
{
	if (!ButtonContainer)
		return;

	// Remove existing buttons
	for (auto& Pair : ButtonMap)
	{
		if (Pair.Value)
		{
			Pair.Value->RemoveFromParent();
		}
	}
	ButtonMap.Empty();

	const TArray<FRammsTaskDefinition> MergedTasks = BuildMergedTaskList();

	// Determine effective MaxPerRow for grid layout.  When using
	// uniform sizing without an explicit MaxPerRow, put all items in
	// a single row (horizontal) or single column (vertical).
	const int32 EffectiveMax = (MaxPerRow > 0) ? MaxPerRow : MergedTasks.Num();

	for (int32 i = 0; i < MergedTasks.Num(); ++i)
	{
		const FRammsTaskDefinition& Def = MergedTasks[i];

		APlayerController* PC = GetOwningPlayer();
		URammsImageButton* Btn = PC
			? CreateWidget<URammsImageButton>(PC)
			: CreateWidget<URammsImageButton>(this);

		if (!Btn)
			continue;

		Btn->SetImageSize(ButtonImageSize);
		Btn->SetLabelText(Def.Label);
		Btn->SetShowLabel(bShowLabels);
		if (Def.Image)
		{
			Btn->SetButtonImage(Def.Image);
		}
		Btn->SetButtonEnabled(Def.bEnabled);
		Btn->SetActive(Def.EnumValue == SelectedValue);

		if (bWrapLabelText)
		{
			Btn->SetLabelAutoWrap(true);
		}

		if (Style)
		{
			Btn->SetStyle(Style);
		}

		// Bind click — we manage mutual exclusion ourselves
		Btn->OnClicked.AddUniqueDynamic(this, &URammsTaskSelector::OnButtonClicked);

		// Determine the widget to add to the container.  When using
		// ButtonFixedSize, wrap in a SizeBox to enforce the fixed dimensions.
		UWidget* ChildToAdd = Btn;

		if (bUniformButtonSize && (ButtonFixedSize.X > 0.0 || ButtonFixedSize.Y > 0.0))
		{
			USizeBox* SizeWrapper = WidgetTree
				? WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
					*FString::Printf(TEXT("BtnSize_%d"), i))
				: nullptr;

			if (SizeWrapper)
			{
				if (ButtonFixedSize.X > 0.0)
					SizeWrapper->SetWidthOverride(ButtonFixedSize.X);
				if (ButtonFixedSize.Y > 0.0)
					SizeWrapper->SetHeightOverride(ButtonFixedSize.Y);
				SizeWrapper->AddChild(Btn);
				ChildToAdd = SizeWrapper;
			}
		}

		// Add to layout container
		if (UUniformGridPanel* Grid = Cast<UUniformGridPanel>(ButtonContainer))
		{
			int32 Row, Col;
			if (Orientation == Orient_Horizontal)
			{
				Row = i / EffectiveMax;
				Col = i % EffectiveMax;
			}
			else
			{
				Col = i / EffectiveMax;
				Row = i % EffectiveMax;
			}
			Grid->AddChildToUniformGrid(ChildToAdd, Row, Col);
			if (UUniformGridSlot* GridSlot = Cast<UUniformGridSlot>(ChildToAdd->Slot))
			{
				GridSlot->SetHorizontalAlignment(HAlign_Fill);
				GridSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}
		else if (UHorizontalBox* HBox = Cast<UHorizontalBox>(ButtonContainer))
		{
			if (UHorizontalBoxSlot* HSlot = HBox->AddChildToHorizontalBox(ChildToAdd))
			{
				HSlot->SetPadding(FMargin(ButtonSpacing * 0.5f));
			}
		}
		else if (UVerticalBox* VBox = Cast<UVerticalBox>(ButtonContainer))
		{
			if (UVerticalBoxSlot* VSlot = VBox->AddChildToVerticalBox(ChildToAdd))
			{
				VSlot->SetPadding(FMargin(ButtonSpacing * 0.5f));
			}
		}

		ButtonMap.Add(Def.EnumValue, Btn);
	}
}

// ── Click Handling ─────────────────────────────────────────────────

void URammsTaskSelector::OnButtonClicked()
{
	// Identify the clicked button: the one currently hovered (mouse/touch)
	// or keyboard-focused (gamepad/keyboard navigation).
	uint8 ClickedValue = 255;

	for (auto& Pair : ButtonMap)
	{
		if (Pair.Value && (Pair.Value->IsHovered() || Pair.Value->HasAnyUserFocus()))
		{
			ClickedValue = Pair.Key;
			break;
		}
	}

	if (ClickedValue == 255)
		return;

	if (ClickedValue == SelectedValue)
	{
		if (bAllowDeselect)
		{
			ClearSelection();
		}
	}
	else
	{
		SelectTask(ClickedValue);
	}
}

uint8 URammsTaskSelector::GetEnumValueForButton(URammsImageButton* Button) const
{
	for (const auto& Pair : ButtonMap)
	{
		if (Pair.Value == Button)
			return Pair.Key;
	}
	return 255;
}
