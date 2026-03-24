// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsLayoutHost.h"
#include "RammsUISubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"

URammsLayoutHost::URammsLayoutHost(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ── Lifecycle ─────────────────────────────────────────────────────

void URammsLayoutHost::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildHostTree();
}

void URammsLayoutHost::NativeConstruct()
{
	Super::NativeConstruct();

	// Subscribe to layout transition requests via the event bus
	if (!bSubscribedToTransitionRequests)
	{
		if (UWorld* World = GetWorld())
		{
			if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
			{
				Subsystem->OnLayoutTransitionRequested.AddUniqueDynamic(this, &URammsLayoutHost::HandleLayoutTransitionRequest);
				bSubscribedToTransitionRequests = true;
			}
		}
	}

	// If layouts were added before construction, inject into the active one
	if (!ActiveLayoutName.IsNone())
	{
		if (URammsLayoutBase* Active = GetActiveLayout())
		{
			InjectPoolWidgets(Active);
		}
	}
}

void URammsLayoutHost::NativeDestruct()
{
	if (bSubscribedToTransitionRequests)
	{
		if (UWorld* World = GetWorld())
		{
			if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
			{
				Subsystem->OnLayoutTransitionRequested.RemoveDynamic(this, &URammsLayoutHost::HandleLayoutTransitionRequest);
			}
		}
		bSubscribedToTransitionRequests = false;
	}

	Super::NativeDestruct();
}

void URammsLayoutHost::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bTransitioning || !RootOverlay)
	{
		return;
	}

	// Advance crossfade
	TransitionAlpha += InDeltaTime / FMath::Max(CrossfadeDuration, 0.01f);

	if (TransitionAlpha >= 1.0f)
	{
		TransitionAlpha = 1.0f;
		FinishTransition();
		return;
	}

	// Smooth crossfade: ease in/out
	float T = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionAlpha, 2.0f);

	// Incoming fades in, outgoing fades out
	if (URammsLayoutBase* Incoming = GetLayout(TransitionToName))
	{
		Incoming->SetRenderOpacity(T);
	}
	if (URammsLayoutBase* Outgoing = GetLayout(TransitionFromName))
	{
		Outgoing->SetRenderOpacity(1.0f - T);
	}
}

void URammsLayoutHost::BuildHostTree()
{
	if (!WidgetTree || RootOverlay)
	{
		return;
	}

	// Overlay as root — layouts are direct children, stacked for crossfade
	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
	RootOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootOverlay;
}

// ── Layout Registration ───────────────────────────────────────────

URammsLayoutBase* URammsLayoutHost::AddLayout(TSubclassOf<URammsLayoutBase> LayoutClass, FName LayoutName)
{
	if (!LayoutClass || LayoutName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("URammsLayoutHost::AddLayout: Invalid class or name"));
		return nullptr;
	}

	if (LayoutMap.Contains(LayoutName))
	{
		UE_LOG(LogTemp, Warning, TEXT("URammsLayoutHost::AddLayout: Layout '%s' already registered"), *LayoutName.ToString());
		return LayoutMap[LayoutName];
	}

	// Create the layout widget
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("URammsLayoutHost::AddLayout: No owning player controller"));
		return nullptr;
	}

	URammsLayoutBase* Layout = CreateWidget<URammsLayoutBase>(PC, LayoutClass);
	if (!Layout)
	{
		UE_LOG(LogTemp, Error, TEXT("URammsLayoutHost::AddLayout: Failed to create widget for '%s'"), *LayoutName.ToString());
		return nullptr;
	}

	AddLayoutInstance(Layout, LayoutName);
	return Layout;
}

void URammsLayoutHost::AddLayoutInstance(URammsLayoutBase* Layout, FName LayoutName)
{
	if (!Layout || LayoutName.IsNone())
	{
		return;
	}

	// Ensure the host tree (Overlay) is built before adding children
	if (!RootOverlay)
	{
		BuildHostTree();
	}

	LayoutMap.Add(LayoutName, Layout);
	LayoutOrder.Add(LayoutName);

	// Set back-reference so the layout can find its host
	Layout->SetOwningHost(this);

	// Add to the overlay (stacked — visibility controls which is shown)
	if (RootOverlay)
	{
		UOverlaySlot* LayoutSlot = RootOverlay->AddChildToOverlay(Layout);
		if (LayoutSlot)
		{
			LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
			LayoutSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// Propagate style
	if (Style)
	{
		Layout->SetStyle(Style);
	}

	// Start collapsed if not the first layout
	if (LayoutOrder.Num() == 1)
	{
		ActiveLayoutName = LayoutName;
		Layout->SetRenderOpacity(1.0f);
		Layout->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		InjectPoolWidgets(Layout);
	}
	else
	{
		Layout->SetRenderOpacity(0.0f);
		Layout->SetVisibility(ESlateVisibility::Collapsed);
	}

	UE_LOG(LogTemp, Log, TEXT("URammsLayoutHost: Registered layout '%s' (index %d, %d slots)"),
		*LayoutName.ToString(), LayoutOrder.Num() - 1, Layout->GetLayoutSlotNames().Num());
}

URammsLayoutBase* URammsLayoutHost::GetLayout(FName LayoutName) const
{
	const TObjectPtr<URammsLayoutBase>* Found = LayoutMap.Find(LayoutName);
	return Found ? Found->Get() : nullptr;
}

URammsLayoutBase* URammsLayoutHost::GetActiveLayout() const
{
	return GetLayout(ActiveLayoutName);
}

TArray<FName> URammsLayoutHost::GetLayoutNames() const
{
	return LayoutOrder;
}

// ── Widget Pool ───────────────────────────────────────────────────

void URammsLayoutHost::AddPoolWidget(FName WidgetTag, URammsBaseWidget* Widget)
{
	if (WidgetTag.IsNone() || !Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("URammsLayoutHost::AddPoolWidget: Invalid tag or null widget"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("URammsLayoutHost::AddPoolWidget('%s', '%s')"),
		*WidgetTag.ToString(), *Widget->GetName());

	// Replace if tag already exists
	for (FRammsPoolEntry& Entry : WidgetPool)
	{
		if (Entry.WidgetTag == WidgetTag)
		{
			Entry.Widget = Widget;

			if (Style)
			{
				Widget->SetStyle(Style);
			}

			// If active, inject immediately
			if (URammsLayoutBase* Active = GetActiveLayout())
			{
				Active->InjectWidget(WidgetTag, Widget);
			}
			return;
		}
	}

	FRammsPoolEntry NewEntry;
	NewEntry.WidgetTag = WidgetTag;
	NewEntry.Widget = Widget;
	WidgetPool.Add(NewEntry);

	// Propagate style
	if (Style)
	{
		Widget->SetStyle(Style);
	}

	// If active layout has a matching slot, inject immediately
	if (URammsLayoutBase* Active = GetActiveLayout())
	{
		UE_LOG(LogTemp, Log, TEXT("  Active layout '%s' — attempting injection"),
			*ActiveLayoutName.ToString());
		Active->InjectWidget(WidgetTag, Widget);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("  No active layout yet — widget pooled for later injection"));
	}
}

void URammsLayoutHost::RemovePoolWidget(FName WidgetTag)
{
	WidgetPool.RemoveAll([&WidgetTag](const FRammsPoolEntry& Entry) {
		if (Entry.WidgetTag == WidgetTag && Entry.Widget)
		{
			Entry.Widget->RemoveFromParent();
			return true;
		}
		return false;
	});
}

URammsBaseWidget* URammsLayoutHost::GetPoolWidget(FName WidgetTag) const
{
	for (const FRammsPoolEntry& Entry : WidgetPool)
	{
		if (Entry.WidgetTag == WidgetTag)
		{
			return Entry.Widget;
		}
	}
	return nullptr;
}

// ── Transitions ───────────────────────────────────────────────────

void URammsLayoutHost::TransitionToLayout(FName LayoutName, bool bAnimated)
{
	if (LayoutName.IsNone() || !LayoutMap.Contains(LayoutName))
	{
		UE_LOG(LogTemp, Warning, TEXT("URammsLayoutHost::TransitionToLayout: Unknown layout '%s'"), *LayoutName.ToString());
		return;
	}

	if (LayoutName == ActiveLayoutName && !bTransitioning)
	{
		return; // already there
	}

	// If currently transitioning, finish immediately
	if (bTransitioning)
	{
		FinishTransition();
	}

	TransitionFromName = ActiveLayoutName;
	TransitionToName = LayoutName;

	// Extract pool widgets from the outgoing layout
	URammsLayoutBase* Outgoing = GetLayout(TransitionFromName);
	if (Outgoing)
	{
		Outgoing->ClearAllSlots();
	}

	// Inject pool widgets into the incoming layout
	URammsLayoutBase* Incoming = GetLayout(LayoutName);
	if (Incoming)
	{
		Incoming->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		InjectPoolWidgets(Incoming);
	}

	if (bAnimated && CrossfadeDuration > 0.0f)
	{
		// Real crossfade: both layouts visible in the overlay,
		// incoming fades in while outgoing fades out.
		bTransitioning = true;
		TransitionAlpha = 0.0f;

		if (Incoming)
		{
			Incoming->SetRenderOpacity(0.0f);
			// Block input on the incoming layout during the fade
			Incoming->SetVisibility(ESlateVisibility::HitTestInvisible);
		}

		// Keep the outgoing layout visible so it can fade out
		if (Outgoing)
		{
			Outgoing->SetRenderOpacity(1.0f);
			Outgoing->SetVisibility(ESlateVisibility::HitTestInvisible);
		}

		OnLayoutTransitionStarted.Broadcast(TransitionToName, TransitionFromName);
	}
	else
	{
		// Instant switch
		ActiveLayoutName = LayoutName;

		if (Incoming)
		{
			Incoming->SetRenderOpacity(1.0f);
			Incoming->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}

		// Collapse the outgoing layout
		if (Outgoing)
		{
			Outgoing->SetVisibility(ESlateVisibility::Collapsed);
			Outgoing->SetRenderOpacity(0.0f);
		}

		OnLayoutTransitionStarted.Broadcast(LayoutName, TransitionFromName);
		OnLayoutTransitionCompleted.Broadcast(LayoutName, TransitionFromName);
	}
}

void URammsLayoutHost::TransitionToLayoutIndex(int32 Index, bool bAnimated)
{
	if (Index >= 0 && Index < LayoutOrder.Num())
	{
		TransitionToLayout(LayoutOrder[Index], bAnimated);
	}
}

// ── Internal ──────────────────────────────────────────────────────

void URammsLayoutHost::InjectPoolWidgets(URammsLayoutBase* Layout)
{
	if (!Layout)
	{
		return;
	}

	TArray<FName> SlotNames = Layout->GetLayoutSlotNames();

	UE_LOG(LogTemp, Log, TEXT("URammsLayoutHost::InjectPoolWidgets: Layout has %d slots, pool has %d widgets"),
		SlotNames.Num(), WidgetPool.Num());

	for (const FRammsPoolEntry& Entry : WidgetPool)
	{
		if (!Entry.Widget)
		{
			continue;
		}

		if (SlotNames.Contains(Entry.WidgetTag))
		{
			// Remove from current parent first (safe even if no parent)
			Entry.Widget->RemoveFromParent();
			Layout->InjectWidget(Entry.WidgetTag, Entry.Widget);
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("  Pool widget '%s' — no matching slot in this layout"),
				*Entry.WidgetTag.ToString());
		}
	}
}

void URammsLayoutHost::ExtractPoolWidgets()
{
	for (const FRammsPoolEntry& Entry : WidgetPool)
	{
		if (Entry.Widget)
		{
			Entry.Widget->RemoveFromParent();
		}
	}
}

void URammsLayoutHost::FinishTransition()
{
	bTransitioning = false;

	ActiveLayoutName = TransitionToName;

	// Incoming layout: fully opaque and interactive
	if (URammsLayoutBase* Incoming = GetLayout(TransitionToName))
	{
		Incoming->SetRenderOpacity(1.0f);
		Incoming->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Incoming->InvalidateLayoutAndVolatility();
	}

	// Outgoing layout: collapsed and non-interactive
	if (URammsLayoutBase* Outgoing = GetLayout(TransitionFromName))
	{
		Outgoing->SetVisibility(ESlateVisibility::Collapsed);
		Outgoing->SetRenderOpacity(0.0f);
	}

	OnLayoutTransitionCompleted.Broadcast(TransitionToName, TransitionFromName);

	UE_LOG(LogTemp, Log, TEXT("URammsLayoutHost: Transition complete → '%s'"), *ActiveLayoutName.ToString());

	TransitionFromName = NAME_None;
	TransitionToName = NAME_None;
}

void URammsLayoutHost::HandleLayoutTransitionRequest(FName LayoutName, bool bAnimated)
{
	TransitionToLayout(LayoutName, bAnimated);
}
