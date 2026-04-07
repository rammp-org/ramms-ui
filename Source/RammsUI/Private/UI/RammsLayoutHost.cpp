// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsLayoutHost.h"
#include "RammsUISubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/GameViewportClient.h"
#include "Slate/WidgetTransform.h"

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

	// Check for orientation changes
	UpdateOrientationCheck();

	if (!bTransitioning || !RootOverlay)
	{
		return;
	}

	// Advance crossfade
	TransitionAlpha += InDeltaTime / FMath::Max(ActiveTransitionDuration, 0.01f);

	if (TransitionAlpha >= 1.0f)
	{
		TransitionAlpha = 1.0f;
		FinishTransition();
		return;
	}

	// Smooth easing
	const float T = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionAlpha, 2.0f);

	URammsLayoutBase* Incoming = GetLayout(TransitionToName);
	URammsLayoutBase* Outgoing = GetLayout(TransitionFromName);

	const bool bUseScale = (ActiveTransitionStyle == ERammsTransitionStyle::Scale
		|| ActiveTransitionStyle == ERammsTransitionStyle::SlideAndScale);
	const bool bUseSlide = (ActiveTransitionStyle == ERammsTransitionStyle::Slide
		|| ActiveTransitionStyle == ERammsTransitionStyle::SlideAndScale);

	const float SlideDistance = bUseSlide ? (TransitionViewportWidth * ActiveSlideDistanceFraction) : 0.0f;
	const float ScaleMin = ActiveTransitionScaleAmount;

	// Compute and apply full transform for incoming layout
	if (Incoming)
	{
		Incoming->SetRenderOpacity(T);

		FVector2D Trans(0.0f, 0.0f);
		FVector2D Scl(1.0f, 1.0f);
		if (bUseSlide)
		{
			Trans.X = FMath::Lerp(SlideDistance * TransitionSlideSign, 0.0f, T);
		}
		if (bUseScale)
		{
			const float S = FMath::Lerp(ScaleMin, 1.0f, T);
			Scl = FVector2D(S, S);
		}
		Incoming->SetRenderTransform(FWidgetTransform(Trans, Scl, FVector2D::ZeroVector, 0.0f));
	}

	// Compute and apply full transform for outgoing layout
	if (Outgoing)
	{
		Outgoing->SetRenderOpacity(1.0f - T);

		FVector2D Trans(0.0f, 0.0f);
		FVector2D Scl(1.0f, 1.0f);
		if (bUseSlide)
		{
			Trans.X = FMath::Lerp(0.0f, -SlideDistance * TransitionSlideSign, T);
		}
		if (bUseScale)
		{
			const float S = FMath::Lerp(1.0f, ScaleMin, T);
			Scl = FVector2D(S, S);
		}
		Outgoing->SetRenderTransform(FWidgetTransform(Trans, Scl, FVector2D::ZeroVector, 0.0f));
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

		// Correct for orientation if already initialized
		// (handles case where host ticked before any layouts were registered)
		ApplyInitialOrientationCorrection();
	}
	else
	{
		Layout->SetRenderOpacity(0.0f);
		Layout->SetVisibility(ESlateVisibility::Collapsed);
	}

	UE_LOG(LogTemp, Log, TEXT("URammsLayoutHost: Registered layout '%s' (index %d, %d slots)"),
		*LayoutName.ToString(), LayoutOrder.Num() - 1, Layout->GetLayoutSlotNames().Num());
}

void URammsLayoutHost::AddLayouts(const TMap<FName, TSubclassOf<URammsLayoutBase>>& Layouts, FName InitialLayout)
{
	for (const auto& Pair : Layouts)
	{
		AddLayout(Pair.Value, Pair.Key);
	}

	// Switch to the requested initial layout (if different from the first registered)
	if (!InitialLayout.IsNone() && InitialLayout != ActiveLayoutName && LayoutMap.Contains(InitialLayout))
	{
		TransitionToLayout(InitialLayout, false);
	}

	// Run orientation correction after all layouts are available
	ApplyInitialOrientationCorrection();
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

void URammsLayoutHost::TransitionToLayout(FName LayoutName, bool bAnimated,
	ERammsSlideDirection SlideDirection)
{
	// Capture and reset orientation flag (set by UpdateOrientationCheck before calling)
	const bool bIsOrientationTriggered = bOrientationTransition;
	bOrientationTransition = false;

	// Resolve to the orientation-appropriate variant if available
	// (e.g., "Arm" → "Arm_Portrait" when in portrait mode)
	if (!bIsOrientationTriggered && bOrientationInitialized)
	{
		FName BaseName = GetBaseLayoutName(LayoutName);
		FName OrientedName = ResolveLayoutForOrientation(BaseName, CurrentOrientation);
		if (OrientedName != LayoutName && LayoutMap.Contains(OrientedName))
		{
			LayoutName = OrientedName;
		}
	}

	if (LayoutName.IsNone() || !LayoutMap.Contains(LayoutName))
	{
		UE_LOG(LogTemp, Warning, TEXT("URammsLayoutHost::TransitionToLayout: Unknown layout '%s'"), *LayoutName.ToString());
		return;
	}

	// Already on the requested layout and not mid-transition — no-op
	if (LayoutName == ActiveLayoutName && !bTransitioning)
	{
		return;
	}

	// Mid-transition to the same target — just let it finish naturally
	if (bTransitioning && LayoutName == TransitionToName)
	{
		return;
	}

	// If currently transitioning to a *different* layout, finish immediately
	if (bTransitioning)
	{
		FinishTransition();
	}

	// After finishing a prior transition, ActiveLayoutName may now match — recheck
	if (LayoutName == ActiveLayoutName)
	{
		return;
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

	if (bAnimated)
	{
		// Resolve effective transition parameters
		if (bIsOrientationTriggered && bOverrideOrientationTransitionStyle)
		{
			ActiveTransitionStyle = OrientationTransitionStyle;
			ActiveTransitionDuration = OrientationTransitionDuration;
			ActiveTransitionScaleAmount = OrientationTransitionScaleAmount;
			ActiveSlideDistanceFraction = OrientationSlideDistanceFraction;
		}
		else
		{
			ActiveTransitionStyle = TransitionStyle;
			ActiveTransitionDuration = TransitionDuration;
			ActiveTransitionScaleAmount = TransitionScaleAmount;
			ActiveSlideDistanceFraction = SlideDistanceFraction;
		}

		if (ActiveTransitionDuration <= 0.0f)
		{
			// Duration zero means instant even if animated was requested
			bAnimated = false;
		}
	}

	if (bAnimated)
	{
		// Real crossfade: both layouts visible in the overlay,
		// incoming fades in while outgoing fades out.
		bTransitioning = true;
		TransitionAlpha = 0.0f;

		// Cache viewport width for slide distance
		TransitionViewportWidth = 1920.0f;
		if (GEngine && GEngine->GameViewport)
		{
			FVector2D VP;
			GEngine->GameViewport->GetViewportSize(VP);
			if (VP.X > 0.0f)
				TransitionViewportWidth = VP.X;
		}

		// Resolve slide direction
		if (SlideDirection == ERammsSlideDirection::Auto)
		{
			// Higher index = slide left (new content comes from right)
			const int32 FromIdx = LayoutOrder.IndexOfByKey(TransitionFromName);
			const int32 ToIdx = LayoutOrder.IndexOfByKey(TransitionToName);
			TransitionSlideSign = (ToIdx >= FromIdx) ? 1.0f : -1.0f;
		}
		else
		{
			TransitionSlideSign = (SlideDirection == ERammsSlideDirection::Left) ? 1.0f : -1.0f;
		}

		if (Incoming)
		{
			Incoming->SetRenderOpacity(0.0f);
			Incoming->SetRenderTransform(FWidgetTransform());
			CachedIncomingPivot = Incoming->GetRenderTransformPivot();
			Incoming->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			// Block input on the incoming layout during the fade
			Incoming->SetVisibility(ESlateVisibility::HitTestInvisible);
		}

		// Keep the outgoing layout visible so it can fade out
		if (Outgoing)
		{
			Outgoing->SetRenderOpacity(1.0f);
			Outgoing->SetRenderTransform(FWidgetTransform());
			CachedOutgoingPivot = Outgoing->GetRenderTransformPivot();
			Outgoing->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
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

	// Incoming layout: fully opaque, interactive, reset transform and pivot
	if (URammsLayoutBase* Incoming = GetLayout(TransitionToName))
	{
		Incoming->SetRenderOpacity(1.0f);
		Incoming->SetRenderTransform(FWidgetTransform());
		Incoming->SetRenderTransformPivot(CachedIncomingPivot);
		Incoming->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Incoming->InvalidateLayoutAndVolatility();
	}

	// Outgoing layout: collapsed, reset transform and pivot
	if (URammsLayoutBase* Outgoing = GetLayout(TransitionFromName))
	{
		Outgoing->SetVisibility(ESlateVisibility::Collapsed);
		Outgoing->SetRenderOpacity(0.0f);
		Outgoing->SetRenderTransform(FWidgetTransform());
		Outgoing->SetRenderTransformPivot(CachedOutgoingPivot);
	}

	OnLayoutTransitionCompleted.Broadcast(TransitionToName, TransitionFromName);

	UE_LOG(LogTemp, Log, TEXT("URammsLayoutHost: Transition complete -> '%s'"), *ActiveLayoutName.ToString());

	TransitionFromName = NAME_None;
	TransitionToName = NAME_None;
}

void URammsLayoutHost::HandleLayoutTransitionRequest(FName LayoutName, bool bAnimated, ERammsSlideDirection SlideDirection)
{
	TransitionToLayout(LayoutName, bAnimated, SlideDirection);
}

// ── Orientation ───────────────────────────────────────────────────

ERammsOrientation URammsLayoutHost::ComputeEffectiveOrientation() const
{
	if (OrientationOverride == ERammsOrientationOverride::ForceLandscape)
	{
		return ERammsOrientation::Landscape;
	}
	if (OrientationOverride == ERammsOrientationOverride::ForcePortrait)
	{
		return ERammsOrientation::Portrait;
	}

	// Auto-detect from viewport
	FVector2D ViewportSize(1920, 1080);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	// Hysteresis: require >5% beyond 1:1 to flip, prevents oscillation near square
	const float Ratio = (ViewportSize.Y > 0.0f) ? (ViewportSize.X / ViewportSize.Y) : 1.0f;
	if (CurrentOrientation == ERammsOrientation::Landscape)
	{
		return (Ratio < 0.95f) ? ERammsOrientation::Portrait : ERammsOrientation::Landscape;
	}
	else
	{
		return (Ratio > 1.05f) ? ERammsOrientation::Landscape : ERammsOrientation::Portrait;
	}
}

void URammsLayoutHost::UpdateOrientationCheck()
{
	// Early-out if viewport size hasn't changed AND override hasn't changed
	if (bOrientationInitialized)
	{
		const bool bOverrideChanged = (OrientationOverride != LastAppliedOrientationOverride);
		FVector2D  CurrentVP(1920, 1080);
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(CurrentVP);
		}
		if (!bOverrideChanged && CurrentVP.Equals(CachedViewportSize, 0.5f))
		{
			return;
		}
		CachedViewportSize = CurrentVP;
		LastAppliedOrientationOverride = OrientationOverride;
	}

	ERammsOrientation NewOrientation = ComputeEffectiveOrientation();

	if (!bOrientationInitialized)
	{
		CurrentOrientation = NewOrientation;
		CachedViewportSize = FVector2D(1920, 1080);
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(CachedViewportSize);
		}
		LastAppliedOrientationOverride = OrientationOverride;
		bOrientationInitialized = true;

		// If starting in non-default orientation, immediately transition
		if (!ActiveLayoutName.IsNone() && !bTransitioning)
		{
			FName BaseName = GetBaseLayoutName(ActiveLayoutName);
			FName TargetName = ResolveLayoutForOrientation(BaseName, NewOrientation);

			if (TargetName != ActiveLayoutName && LayoutMap.Contains(TargetName))
			{
				UE_LOG(LogTemp, Log, TEXT("URammsLayoutHost: Initial orientation '%s' - switching to '%s'"),
					NewOrientation == ERammsOrientation::Landscape ? TEXT("Landscape") : TEXT("Portrait"),
					*TargetName.ToString());
				TransitionToLayout(TargetName, false); // instant on first frame
			}
		}
		return;
	}

	if (NewOrientation == CurrentOrientation)
	{
		return;
	}

	ERammsOrientation OldOrientation = CurrentOrientation;
	CurrentOrientation = NewOrientation;

	UE_LOG(LogTemp, Log, TEXT("URammsLayoutHost: Orientation changed %s -> %s"),
		OldOrientation == ERammsOrientation::Landscape ? TEXT("Landscape") : TEXT("Portrait"),
		NewOrientation == ERammsOrientation::Landscape ? TEXT("Landscape") : TEXT("Portrait"));

	OnOrientationChanged.Broadcast(NewOrientation);

	// Auto-transition to the orientation-appropriate layout variant
	if (!ActiveLayoutName.IsNone() && !bTransitioning)
	{
		FName BaseName = GetBaseLayoutName(ActiveLayoutName);
		FName TargetName = ResolveLayoutForOrientation(BaseName, NewOrientation);

		if (TargetName != ActiveLayoutName && LayoutMap.Contains(TargetName))
		{
			UE_LOG(LogTemp, Log, TEXT("URammsLayoutHost: Orientation auto-transition '%s' -> '%s'"),
				*ActiveLayoutName.ToString(), *TargetName.ToString());
			bOrientationTransition = true;
			TransitionToLayout(TargetName, bAnimateOrientationTransition);
		}
	}
}

void URammsLayoutHost::ApplyInitialOrientationCorrection()
{
	if (ActiveLayoutName.IsNone() || bTransitioning)
	{
		return;
	}

	// Compute current orientation (initializes if needed)
	if (!bOrientationInitialized)
	{
		CurrentOrientation = ComputeEffectiveOrientation();
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(CachedViewportSize);
		}
		LastAppliedOrientationOverride = OrientationOverride;
		bOrientationInitialized = true;
	}

	FName BaseName = GetBaseLayoutName(ActiveLayoutName);
	FName TargetName = ResolveLayoutForOrientation(BaseName, CurrentOrientation);

	if (TargetName != ActiveLayoutName && LayoutMap.Contains(TargetName))
	{
		UE_LOG(LogTemp, Log, TEXT("URammsLayoutHost: Initial orientation correction '%s' -> '%s'"),
			*ActiveLayoutName.ToString(), *TargetName.ToString());
		TransitionToLayout(TargetName, false); // instant
	}
}

void URammsLayoutHost::SetOrientationOverride(ERammsOrientationOverride Override)
{
	OrientationOverride = Override;
	// Invalidate viewport cache so UpdateOrientationCheck re-evaluates
	CachedViewportSize = FVector2D(-1.0f, -1.0f);
	UpdateOrientationCheck();
}

FName URammsLayoutHost::ResolveLayoutForOrientation(FName BaseName, ERammsOrientation Orientation) const
{
	if (Orientation == ERammsOrientation::Portrait)
	{
		// 1. Check explicit pair mapping
		if (const FName* PortraitName = PortraitLayoutMap.Find(BaseName))
		{
			if (LayoutMap.Contains(*PortraitName))
			{
				return *PortraitName;
			}
		}

		// 2. Suffix fallback: try BaseName_Portrait
		FName SuffixedName = FName(*(BaseName.ToString() + TEXT("_Portrait")));
		if (LayoutMap.Contains(SuffixedName))
		{
			return SuffixedName;
		}

		// 3. No portrait variant found — stay on base
		return BaseName;
	}
	else
	{
		// Landscape: the base name IS the landscape variant.
		// But if we're currently on a portrait layout, resolve back.
		if (LayoutMap.Contains(BaseName))
		{
			return BaseName;
		}

		// Try BaseName_Landscape suffix
		FName SuffixedName = FName(*(BaseName.ToString() + TEXT("_Landscape")));
		if (LayoutMap.Contains(SuffixedName))
		{
			return SuffixedName;
		}

		return BaseName;
	}
}

FName URammsLayoutHost::GetBaseLayoutName(FName LayoutName) const
{
	FString NameStr = LayoutName.ToString();

	// Strip known suffixes
	if (NameStr.EndsWith(TEXT("_Portrait")))
	{
		FName Stripped = FName(*NameStr.LeftChop(9)); // len("_Portrait") = 9
		if (LayoutMap.Contains(Stripped) || PortraitLayoutMap.Contains(Stripped))
		{
			return Stripped;
		}
	}
	else if (NameStr.EndsWith(TEXT("_Landscape")))
	{
		FName Stripped = FName(*NameStr.LeftChop(10)); // len("_Landscape") = 10
		return Stripped;
	}

	// Reverse lookup in PortraitLayoutMap: if LayoutName is a portrait value, return its key
	for (const auto& Pair : PortraitLayoutMap)
	{
		if (Pair.Value == LayoutName)
		{
			return Pair.Key;
		}
	}

	return LayoutName;
}
