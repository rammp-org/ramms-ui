// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsToolbar.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "RammsUISubsystem.h"

void URammsToolbar::ResetCachedWidgets()
{
	ToolbarBorder = nullptr;
	ItemContainer = nullptr;
	ToolbarContentSlot = nullptr;
	ItemButtons.Empty();
}

void URammsToolbar::BuildWidgetTree()
{
	if (!WidgetTree)
		return;

	// If WBP provided both border and container, just build the items into it
	if (ToolbarBorder && ItemContainer)
	{
		RebuildItems();
		return;
	}

	// If WBP provided only the container (no border), build items into it
	if (ItemContainer)
	{
		RebuildItems();
		return;
	}

	// Root: Border with rounded background
	ToolbarBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ToolbarBorder"));
	ToolbarBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(
		FLinearColor(0.08f, 0.08f, 0.1f, 0.9f), 4.0f, FLinearColor(0.3f, 0.3f, 0.3f, 1.0f), 1.0f);
	ToolbarBorder->SetPadding(FMargin(4.0f));
	ToolbarBorder->SetClipping(EWidgetClipping::ClipToBounds);
	WidgetTree->RootWidget = ToolbarBorder;

	// Item container based on orientation
	if (Orientation == ERammsToolbarOrientation::Horizontal)
	{
		UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ItemContainer"));
		ItemContainer = HBox;
	}
	else
	{
		UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ItemContainer"));
		ItemContainer = VBox;
	}
	ToolbarBorder->AddChild(ItemContainer);

	// Named slot for custom content added in WBP designer
	ToolbarContentSlot = WidgetTree->ConstructWidget<UNamedSlot>(UNamedSlot::StaticClass(), TEXT("ToolbarContentSlot"));
	if (Orientation == ERammsToolbarOrientation::Horizontal)
	{
		UHorizontalBox* HBox = Cast<UHorizontalBox>(ItemContainer);
		if (HBox)
		{
			UHorizontalBoxSlot* SlotEntry = HBox->AddChildToHorizontalBox(ToolbarContentSlot);
			if (SlotEntry)
				SlotEntry->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
	else
	{
		UVerticalBox* VBox = Cast<UVerticalBox>(ItemContainer);
		if (VBox)
		{
			UVerticalBoxSlot* SlotEntry = VBox->AddChildToVerticalBox(ToolbarContentSlot);
			if (SlotEntry)
				SlotEntry->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}

	// Build buttons for initial items
	RebuildItems();
}

void URammsToolbar::RebuildItems()
{
	if (!ItemContainer || !WidgetTree)
		return;

	// Remove only item buttons (preserve ToolbarContentSlot and other children)
	for (const auto& Pair : ItemButtons)
	{
		if (Pair.Value)
		{
			Pair.Value->RemoveFromParent();
		}
	}
	ItemButtons.Empty();

	// Create buttons for each item
	for (const FRammsToolbarItem& Item : Items)
	{
		UButton* Btn = CreateItemButton(Item);
		ItemButtons.Add(Item.ItemID, Btn);

		if (Orientation == ERammsToolbarOrientation::Horizontal)
		{
			UHorizontalBox* HBox = Cast<UHorizontalBox>(ItemContainer);
			if (HBox)
			{
				UHorizontalBoxSlot* ItemSlot = HBox->AddChildToHorizontalBox(Btn);
				if (ItemSlot)
					ItemSlot->SetPadding(FMargin(ItemSpacing * 0.5f));
			}
		}
		else
		{
			UVerticalBox* VBox = Cast<UVerticalBox>(ItemContainer);
			if (VBox)
			{
				UVerticalBoxSlot* ItemSlot = VBox->AddChildToVerticalBox(Btn);
				if (ItemSlot)
					ItemSlot->SetPadding(FMargin(ItemSpacing * 0.5f));
			}
		}
	}
}

UButton* URammsToolbar::CreateItemButton(const FRammsToolbarItem& Item)
{
	FString	 BtnName = FString::Printf(TEXT("Btn_%s"), *Item.ItemID.ToString());
	UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *BtnName);

	// Add icon or label
	if (Item.Icon)
	{
		FString ImgName = FString::Printf(TEXT("Icon_%s"), *Item.ItemID.ToString());
		UImage* IconImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *ImgName);
		IconImg->SetBrushFromTexture(Item.Icon);
		IconImg->SetDesiredSizeOverride(FVector2D(24.0f, 24.0f));
		Btn->AddChild(IconImg);
	}
	else
	{
		FString		LblName = FString::Printf(TEXT("Label_%s"), *Item.ItemID.ToString());
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *LblName);
		Label->SetText(Item.Label);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Btn->AddChild(Label);
	}

	Btn->SetIsEnabled(Item.bIsEnabled);

	// Store ItemID in button's tag for lookup
	Btn->OnClicked.AddUniqueDynamic(this, &URammsToolbar::OnButtonClicked);

	return Btn;
}

void URammsToolbar::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsToolbar::NativeConstruct()
{
	Super::NativeConstruct();
}

void URammsToolbar::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// Rebuild items if the Items array changed in the designer
	if (ItemContainer && WidgetTree)
	{
		RebuildItems();
	}
}

void URammsToolbar::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	if (ToolbarBorder)
	{
		FLinearColor Bg = Style->Colors.Surface;
		Bg.A = 0.9f;
		float		Radius = Style->Border.CornerRadiusMedium;
		float		BorderW = Style->Border.BorderWidth;
		FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrush(Bg, Radius, Style->Colors.Border, BorderW);
		URammsUIStyle::ApplyRoundedBrushToBorder(ToolbarBorder, Brush);
		ToolbarBorder->SetPadding(FMargin(BorderW + 2.0f));
	}

	// Update button visuals
	for (const FRammsToolbarItem& Item : Items)
	{
		UpdateButtonVisual(Item.ItemID);
	}
}

void URammsToolbar::AddItem(FRammsToolbarItem Item)
{
	Items.Add(Item);

	if (ItemContainer && WidgetTree)
	{
		UButton* Btn = CreateItemButton(Item);
		ItemButtons.Add(Item.ItemID, Btn);

		if (Orientation == ERammsToolbarOrientation::Horizontal)
		{
			UHorizontalBox* HBox = Cast<UHorizontalBox>(ItemContainer);
			if (HBox)
			{
				UHorizontalBoxSlot* ItemSlot = HBox->AddChildToHorizontalBox(Btn);
				if (ItemSlot)
					ItemSlot->SetPadding(FMargin(ItemSpacing * 0.5f));
			}
		}
		else
		{
			UVerticalBox* VBox = Cast<UVerticalBox>(ItemContainer);
			if (VBox)
			{
				UVerticalBoxSlot* ItemSlot = VBox->AddChildToVerticalBox(Btn);
				if (ItemSlot)
					ItemSlot->SetPadding(FMargin(ItemSpacing * 0.5f));
			}
		}
	}
}

void URammsToolbar::RemoveItem(FName ItemID)
{
	Items.RemoveAll([ItemID](const FRammsToolbarItem& Item) { return Item.ItemID == ItemID; });

	TObjectPtr<UButton>* BtnPtr = ItemButtons.Find(ItemID);
	if (BtnPtr && *BtnPtr)
	{
		(*BtnPtr)->RemoveFromParent();
		ItemButtons.Remove(ItemID);
	}
}

void URammsToolbar::SetItemEnabled(FName ItemID, bool bEnabled)
{
	for (FRammsToolbarItem& Item : Items)
	{
		if (Item.ItemID == ItemID)
		{
			Item.bIsEnabled = bEnabled;
			break;
		}
	}

	TObjectPtr<UButton>* BtnPtr = ItemButtons.Find(ItemID);
	if (BtnPtr && *BtnPtr)
	{
		(*BtnPtr)->SetIsEnabled(bEnabled);
	}
}

void URammsToolbar::SetItemActive(FName ItemID, bool bActive)
{
	for (FRammsToolbarItem& Item : Items)
	{
		if (Item.ItemID == ItemID)
		{
			Item.bIsActive = bActive;
			break;
		}
	}
	UpdateButtonVisual(ItemID);
}

bool URammsToolbar::IsItemActive(FName ItemID) const
{
	for (const FRammsToolbarItem& Item : Items)
	{
		if (Item.ItemID == ItemID)
			return Item.bIsActive;
	}
	return false;
}

void URammsToolbar::OnButtonClicked()
{
	// Find which button was clicked by checking which one is hovered
	// (IsHovered is reliable during OnClicked; IsPressed may already be false)
	for (const auto& Pair : ItemButtons)
	{
		if (Pair.Value && Pair.Value->IsHovered())
		{
			FName ItemID = Pair.Key;
			OnItemClicked.Broadcast(ItemID);

			// Also broadcast through subsystem for cross-widget communication
			if (UWorld* World = GetWorld())
			{
				if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
				{
					Subsystem->BroadcastToolbarItemClicked(ItemID);
				}
			}

			// Handle toggle
			for (FRammsToolbarItem& Item : Items)
			{
				if (Item.ItemID == ItemID && Item.bIsToggle)
				{
					Item.bIsActive = !Item.bIsActive;
					UpdateButtonVisual(ItemID);
					OnItemToggled.Broadcast(ItemID, Item.bIsActive);

					if (UWorld* World = GetWorld())
					{
						if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
						{
							Subsystem->BroadcastToolbarItemToggled(ItemID, Item.bIsActive);
						}
					}
					break;
				}
			}
			return;
		}
	}
}

void URammsToolbar::UpdateButtonVisual(FName ItemID)
{
	TObjectPtr<UButton>* BtnPtr = ItemButtons.Find(ItemID);
	if (!BtnPtr || !*BtnPtr)
		return;

	UButton* Btn = *BtnPtr;

	// Find item data
	const FRammsToolbarItem* ItemData = nullptr;
	for (const FRammsToolbarItem& Item : Items)
	{
		if (Item.ItemID == ItemID)
		{
			ItemData = &Item;
			break;
		}
	}

	if (!ItemData)
		return;

	// Apply active/inactive visual
	if (Style)
	{
		FLinearColor BgColor = ItemData->bIsActive ? Style->Colors.Primary : Style->Colors.Secondary;
		Btn->SetBackgroundColor(BgColor);
	}
	else
	{
		FLinearColor BgColor = ItemData->bIsActive ? FLinearColor(0.2f, 0.4f, 0.8f) : FLinearColor(0.2f, 0.2f, 0.25f);
		Btn->SetBackgroundColor(BgColor);
	}

	Btn->SetRenderOpacity(ItemData->bIsEnabled ? 1.0f : 0.5f);
}
