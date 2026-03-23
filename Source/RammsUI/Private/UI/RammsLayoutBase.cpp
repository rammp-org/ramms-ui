// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsLayoutBase.h"
#include "UI/RammsLayoutHost.h"
#include "RammsUISubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/NamedSlot.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/PanelWidget.h"
#include "Components/ContentWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogRammsLayout, Log, All);

void URammsLayoutBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	DiscoverSlots();
}

TArray<FName> URammsLayoutBase::GetLayoutSlotNames_Implementation() const
{
	TArray<FName> Names;
	SlotMap.GetKeys(Names);
	return Names;
}

UPanelWidget* URammsLayoutBase::GetSlot(FName SlotName) const
{
	const TObjectPtr<UPanelWidget>* Found = SlotMap.Find(SlotName);
	return Found ? Found->Get() : nullptr;
}

bool URammsLayoutBase::InjectWidget(FName SlotName, UWidget* Widget)
{
	UPanelWidget* Container = GetSlot(SlotName);
	if (!Container || !Widget)
	{
		UE_LOG(LogRammsLayout, Warning,
			TEXT("InjectWidget('%s'): %s"),
			*SlotName.ToString(),
			!Container ? TEXT("slot not found") : TEXT("null widget"));
		return false;
	}

	// Remove widget from any current parent
	Widget->RemoveFromParent();

	// For single-child containers (NamedSlot, SizeBox), use SetContent
	if (UContentWidget* ContentContainer = Cast<UContentWidget>(Container))
	{
		ContentContainer->SetContent(Widget);
	}
	else
	{
		// For multi-child containers (Overlay, etc.), clear then add
		Container->ClearChildren();
		Container->AddChild(Widget);
	}

	UE_LOG(LogRammsLayout, Log,
		TEXT("InjectWidget('%s'): Injected '%s' into %s container"),
		*SlotName.ToString(),
		*Widget->GetName(),
		*Container->GetClass()->GetName());

	return true;
}

void URammsLayoutBase::ClearSlot(FName SlotName)
{
	UPanelWidget* Container = GetSlot(SlotName);
	if (!Container)
	{
		return;
	}

	// Remove children without destroying them
	while (Container->GetChildrenCount() > 0)
	{
		UWidget* Child = Container->GetChildAt(0);
		if (Child)
		{
			Child->RemoveFromParent();
		}
	}
}

void URammsLayoutBase::ClearAllSlots()
{
	for (auto& Pair : SlotMap)
	{
		if (UPanelWidget* Container = Pair.Value)
		{
			while (Container->GetChildrenCount() > 0)
			{
				UWidget* Child = Container->GetChildAt(0);
				if (Child)
				{
					Child->RemoveFromParent();
				}
			}
		}
	}
}

void URammsLayoutBase::RegisterSlot(FName SlotName, UPanelWidget* Container)
{
	if (Container && !SlotName.IsNone())
	{
		SlotMap.Add(SlotName, Container);
		UE_LOG(LogRammsLayout, Log,
			TEXT("RegisterSlot('%s'): %s [%s]"),
			*SlotName.ToString(),
			*Container->GetName(),
			*Container->GetClass()->GetName());
	}
}

void URammsLayoutBase::DiscoverSlots()
{
	if (!WidgetTree)
	{
		UE_LOG(LogRammsLayout, Warning,
			TEXT("DiscoverSlots '%s': No WidgetTree — is this an abstract base class?"),
			*GetName());
		return;
	}

	// Walk all widgets in the tree and register injection-compatible containers
	WidgetTree->ForEachWidget([this](UWidget* Widget) {
		// Accept: NamedSlot, Overlay, SizeBox
		UPanelWidget* Panel = nullptr;

		if (UNamedSlot* Slot = Cast<UNamedSlot>(Widget))
		{
			Panel = Slot;
		}
		else if (UOverlay* Ovl = Cast<UOverlay>(Widget))
		{
			// Skip the root overlay (if it's the root widget, it's structural)
			if (Widget != WidgetTree->RootWidget)
			{
				Panel = Ovl;
			}
		}
		else if (USizeBox* SB = Cast<USizeBox>(Widget))
		{
			Panel = SB;
		}

		if (Panel)
		{
			FName SlotName = Panel->GetFName();
			if (!SlotName.IsNone())
			{
				SlotMap.Add(SlotName, Panel);
			}
		}
	});

	UE_LOG(LogRammsLayout, Log,
		TEXT("DiscoverSlots '%s': Found %d slots:"),
		*GetName(), SlotMap.Num());

	for (auto& Pair : SlotMap)
	{
		UE_LOG(LogRammsLayout, Log,
			TEXT("  - '%s' (%s)"),
			*Pair.Key.ToString(),
			*Pair.Value->GetClass()->GetName());
	}
}

// ── Host Reference & Transition Requests ──────────────────────────

URammsLayoutHost* URammsLayoutBase::GetOwningHost() const
{
	return OwningHost.Get();
}

void URammsLayoutBase::RequestLayoutTransition(FName LayoutName, bool bAnimated)
{
	if (UWorld* World = GetWorld())
	{
		if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
		{
			Subsystem->BroadcastLayoutTransitionRequest(LayoutName, bAnimated);
		}
	}
}

void URammsLayoutBase::SetOwningHost(URammsLayoutHost* Host)
{
	OwningHost = Host;
}