// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsNotificationContainer.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"

void URammsNotificationContainer::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
		return;

	// Root: full-screen overlay (fills the entire viewport)
	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), TEXT("RootOverlay"));
	WidgetTree->RootWidget = RootOverlay;

	// Padding border — provides edge margins around the notification stack
	UBorder* PaddingBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("PaddingBorder"));
	PaddingBorder->SetBrushColor(FLinearColor::Transparent);
	PaddingBorder->SetPadding(FMargin(EdgePadding));

	UOverlaySlot* PaddingSlot = RootOverlay->AddChildToOverlay(PaddingBorder);
	if (PaddingSlot)
	{
		PaddingSlot->SetHorizontalAlignment(HAlign_Right);
		PaddingSlot->SetVerticalAlignment(VAlign_Top);
	}

	// SizeBox to constrain notification width
	USizeBox* WidthBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("WidthBox"));
	WidthBox->SetWidthOverride(NotificationWidth);
	PaddingBorder->AddChild(WidthBox);

	// Vertical box inside — notifications stack top-to-bottom
	NotificationStack = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("NotificationStack"));
	WidthBox->AddChild(NotificationStack);
}

void URammsNotificationContainer::NativeConstruct()
{
	Super::NativeConstruct();

	// Make the container non-interactive so clicks pass through to the game
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void URammsNotificationContainer::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Check for dismissed notifications and remove them
	for (int32 i = ActiveNotifications.Num() - 1; i >= 0; --i)
	{
		if (!ActiveNotifications[i].IsValid())
		{
			ActiveNotifications.RemoveAt(i);
			continue;
		}

		URammsNotificationWidget* Notif = ActiveNotifications[i].Get();
		if (Notif->GetVisibility() == ESlateVisibility::Collapsed)
		{
			Notif->RemoveFromParent();
			ActiveNotifications.RemoveAt(i);
		}
	}
}

void URammsNotificationContainer::AddNotification(URammsNotificationWidget* Notification)
{
	if (!Notification || !NotificationStack)
		return;

	// Add to the vertical box
	UVerticalBoxSlot* VBSlot = NotificationStack->AddChildToVerticalBox(Notification);
	if (VBSlot)
	{
		VBSlot->SetHorizontalAlignment(HAlign_Fill);
		VBSlot->SetVerticalAlignment(VAlign_Top);
		VBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		VBSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, ItemSpacing));
	}

	ActiveNotifications.Add(Notification);
}

int32 URammsNotificationContainer::DismissAll()
{
	int32											 Count = 0;
	TArray<TWeakObjectPtr<URammsNotificationWidget>> Copy = ActiveNotifications;
	for (auto& Ptr : Copy)
	{
		if (Ptr.IsValid())
		{
			Ptr->Dismiss();
			Count++;
		}
	}
	return Count;
}

int32 URammsNotificationContainer::GetActiveCount() const
{
	int32 Count = 0;
	for (const auto& Ptr : ActiveNotifications)
	{
		if (Ptr.IsValid() && Ptr->GetVisibility() != ESlateVisibility::Collapsed)
			Count++;
	}
	return Count;
}
