// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsBaseWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Animation/UMGSequencePlayer.h"
#include "Interfaces/IRammsRobotController.h"
#include "RammsUISubsystem.h"

bool URammsBaseWidget::Initialize()
{
	bool bResult = Super::Initialize();
	if (bResult)
	{
		// Build the widget tree immediately after WidgetTree is created by Super.
		// This ensures the tree is populated BEFORE RebuildWidget()/TakeWidget()
		// creates the Slate representation — critical for designer preview.
		// At runtime, NativeOnInitialized (called by Super) may have already built
		// it; the guard in each derived BuildWidgetTree prevents double-building.
		BuildWidgetTree();

		// Re-apply NamedSlot content from serialized NamedSlotBindings.
		// UUserWidget::InitializeNamedSlots() (called by Super before BuildWidgetTree)
		// uses UPROPERTY lookup to find UNamedSlot pointers, which fails because our
		// pointers are marked Transient (null at that point). After BuildWidgetTree
		// creates the actual UNamedSlot widgets, re-apply via our known-good pointers.
		TArray<FName> SlotNames;
		GetSlotNames(SlotNames);
		for (const FName& SlotName : SlotNames)
		{
			if (UWidget* Content = GetContentForSlot(SlotName))
			{
				if (UNamedSlot* NamedSlot = GetNamedSlotWidget(SlotName))
				{
					NamedSlot->ClearChildren();
					NamedSlot->AddChild(Content);
				}
			}
		}
	}
	return bResult;
}

void URammsBaseWidget::SetContentForSlot(FName SlotName, UWidget* Content)
{
	// Let Super handle NamedSlotBindings persistence and WidgetTree->FindWidget.
	Super::SetContentForSlot(SlotName, Content);

	// Also directly populate our UNamedSlot via known-good pointer.
	// Super may have found a stale deserialized widget with the same name
	// instead of the one created by BuildWidgetTree.
	if (UNamedSlot* NamedSlot = GetNamedSlotWidget(SlotName))
	{
		NamedSlot->ClearChildren();
		if (Content)
		{
			NamedSlot->AddChild(Content);
		}
	}
}

void URammsBaseWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Tree should already be built by Initialize(), but apply style here
	// since this is the first point where designer flags are set.
	if (bAutoApplyStyle && Style)
	{
		ApplyStyle();
		PropagateStyleToChildren();
	}
}

void URammsBaseWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Auto-pick up subsystem theme if no explicit style was set
	if (!Style)
	{
		if (UWorld* World = GetWorld())
		{
			if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
			{
				if (URammsUIStyle* Theme = Subsystem->GetTheme())
				{
					Style = Theme;
				}
			}
		}
	}

	// Style is already applied by NativePreConstruct, but re-apply if
	// style was set between PreConstruct and Construct (e.g., parent propagation)
	if (bAutoApplyStyle && Style)
	{
		ApplyStyle();
		PropagateStyleToChildren();
	}

	if (bAutoFindRobotController)
	{
		SubscribeToRegistryChanges();
		ResolveController();
	}
}

void URammsBaseWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ActiveAnimations.Num() > 0)
	{
		UpdateAnimations(InDeltaTime);
	}
}

void URammsBaseWidget::SetStyle(URammsUIStyle* NewStyle)
{
	if (NewStyle != Style)
	{
		Style = NewStyle;
		ApplyStyle();
		PropagateStyleToChildren();
	}
}

void URammsBaseWidget::PropagateStyleToChildren()
{
	if (!bPropagateStyleToChildren || !Style || !WidgetTree)
		return;

	// Walk all widgets in our WidgetTree and propagate style to child RammsBaseWidgets
	WidgetTree->ForEachWidget([this](UWidget* Widget) {
		if (Widget == this)
			return;

		if (URammsBaseWidget* ChildRamms = Cast<URammsBaseWidget>(Widget))
		{
			// Only set if child doesn't already have a style (respect explicit overrides)
			if (!ChildRamms->GetStyle())
			{
				ChildRamms->SetStyle(Style);
			}
		}
	});
}

void URammsBaseWidget::ApplyStyle_Implementation()
{
	// Base implementation does nothing - override in derived classes
}

void URammsBaseWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// After Blueprint recompilation or deserialization the WidgetTree root may
	// be cleared while our cached widget pointers are stale.  Also detect stale
	// cached pointers that reference widgets not owned by the current WidgetTree
	// (happens when a C++ UUserWidget is serialized inside a parent WBP and the
	// cached TObjectPtr was not marked Transient).  In either case, wipe the
	// cached pointers so BuildWidgetTree can recreate the tree.
	if (WidgetTree)
	{
		if (!WidgetTree->RootWidget)
		{
			ResetCachedWidgets();
		}
		else
		{
			// Verify the root widget is actually one of ours.  If a stale pointer
			// slipped through serialization, the WidgetTree root will differ from
			// what BuildWidgetTree would have set.
			UWidget* ExpectedRoot = GetRootWidgetForValidation();
			if (ExpectedRoot && WidgetTree->RootWidget != ExpectedRoot)
			{
				ResetCachedWidgets();
				WidgetTree->RootWidget = nullptr;
			}
		}
	}

	// Ensure the widget tree is built (covers the case where NativePreConstruct
	// ran before WidgetTree was ready, or the tree was just reset above)
	BuildWidgetTree();

	// Designer-only: when placed in a Canvas Panel with default (0,0) slot size
	// (common when the widget had no content at placement time), auto-enable
	// "Size to Content" so the widget is visible in the editor. Gated to
	// design-time so runtime layout (e.g., widgets sized later or animated
	// from 0) is never silently mutated.
	if (IsDesignTime())
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			FAnchors Anchors = CanvasSlot->GetAnchors();
			bool	 bPointAnchors = FMath::IsNearlyEqual(Anchors.Minimum.X, Anchors.Maximum.X)
				&& FMath::IsNearlyEqual(Anchors.Minimum.Y, Anchors.Maximum.Y);

			if (bPointAnchors && !CanvasSlot->GetAutoSize())
			{
				FVector2D SlotSize = CanvasSlot->GetSize();
				if (SlotSize.X < 1.0f && SlotSize.Y < 1.0f)
				{
					CanvasSlot->SetAutoSize(true);
				}
			}
		}
	}

	// Re-apply style when properties change in the designer
	if (bAutoApplyStyle && Style)
	{
		ApplyStyle();
		PropagateStyleToChildren();
	}

	// Force Slate to recalculate layout — prevents 0-height after Blueprint recompilation
	InvalidateLayoutAndVolatility();
}

// ==================== Animation Helpers ====================

void URammsBaseWidget::FadeIn(float Duration, ERammsUIEasing Easing)
{
	FAnimationState Anim;
	Anim.Type = FAnimationState::EType::Fade;
	Anim.Duration = (Duration < 0.0f && Style) ? Style->FadeInCurve.Duration : FMath::Max(0.0f, Duration);
	Anim.Easing = (Easing == ERammsUIEasing::EaseInOut && Style) ? Style->FadeInCurve.Easing : Easing;
	Anim.StartValue = FVector2D(CurrentOpacity, 0.0f);
	Anim.TargetValue = FVector2D(1.0f, 0.0f);
	Anim.bActive = true;

	StartAnimation(Anim);
}

void URammsBaseWidget::FadeOut(float Duration, ERammsUIEasing Easing)
{
	FAnimationState Anim;
	Anim.Type = FAnimationState::EType::Fade;
	Anim.Duration = (Duration < 0.0f && Style) ? Style->FadeOutCurve.Duration : FMath::Max(0.0f, Duration);
	Anim.Easing = (Easing == ERammsUIEasing::EaseInOut && Style) ? Style->FadeOutCurve.Easing : Easing;
	Anim.StartValue = FVector2D(CurrentOpacity, 0.0f);
	Anim.TargetValue = FVector2D(0.0f, 0.0f);
	Anim.bActive = true;

	StartAnimation(Anim);
}

void URammsBaseWidget::SlideIn(FVector2D FromOffset, float Duration, ERammsUIEasing Easing)
{
	FAnimationState Anim;
	Anim.Type = FAnimationState::EType::Slide;
	Anim.Duration = (Duration < 0.0f && Style) ? Style->SlideCurve.Duration : FMath::Max(0.0f, Duration);
	Anim.Easing = (Easing == ERammsUIEasing::EaseInOut && Style) ? Style->SlideCurve.Easing : Easing;
	Anim.StartValue = FromOffset;
	Anim.TargetValue = FVector2D::ZeroVector;
	Anim.bActive = true;

	StartAnimation(Anim);
}

void URammsBaseWidget::SlideOut(FVector2D ToOffset, float Duration, ERammsUIEasing Easing)
{
	FAnimationState Anim;
	Anim.Type = FAnimationState::EType::Slide;
	Anim.Duration = (Duration < 0.0f && Style) ? Style->SlideCurve.Duration : FMath::Max(0.0f, Duration);
	Anim.Easing = (Easing == ERammsUIEasing::EaseInOut && Style) ? Style->SlideCurve.Easing : Easing;
	Anim.StartValue = FVector2D::ZeroVector;
	Anim.TargetValue = ToOffset;
	Anim.bActive = true;

	StartAnimation(Anim);
}

void URammsBaseWidget::ScaleIn(float Duration, ERammsUIEasing Easing)
{
	FAnimationState Anim;
	Anim.Type = FAnimationState::EType::Scale;
	Anim.Duration = (Duration < 0.0f && Style) ? Style->ScaleCurve.Duration : FMath::Max(0.0f, Duration);
	Anim.Easing = (Easing == ERammsUIEasing::EaseInOut && Style) ? Style->ScaleCurve.Easing : Easing;
	Anim.StartValue = FVector2D(0.0f, 0.0f);
	Anim.TargetValue = FVector2D(1.0f, 1.0f);
	Anim.bActive = true;

	StartAnimation(Anim);
}

void URammsBaseWidget::ScaleOut(float Duration, ERammsUIEasing Easing)
{
	FAnimationState Anim;
	Anim.Type = FAnimationState::EType::Scale;
	Anim.Duration = (Duration < 0.0f && Style) ? Style->ScaleCurve.Duration : FMath::Max(0.0f, Duration);
	Anim.Easing = (Easing == ERammsUIEasing::EaseInOut && Style) ? Style->ScaleCurve.Easing : Easing;
	Anim.StartValue = CurrentScale;
	Anim.TargetValue = FVector2D(0.0f, 0.0f);
	Anim.bActive = true;

	StartAnimation(Anim);
}

void URammsBaseWidget::StopRammsAnimations()
{
	ActiveAnimations.Empty();
}

// ==================== Layout Helpers ====================

void URammsBaseWidget::SetAnchors(FAnchors Anchors)
{
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CanvasSlot->SetAnchors(Anchors);
	}
}

void URammsBaseWidget::SetAlignment(FVector2D Alignment)
{
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CanvasSlot->SetAlignment(Alignment);
	}
}

void URammsBaseWidget::SetPosition(FVector2D Position)
{
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CanvasSlot->SetPosition(Position);
	}
}

void URammsBaseWidget::SetSize(FVector2D Size)
{
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CanvasSlot->SetSize(Size);
	}
}

// ==================== Animation System ====================

float URammsBaseWidget::EvaluateEasing(float Alpha, ERammsUIEasing Easing)
{
	switch (Easing)
	{
		case ERammsUIEasing::Linear:
			return Alpha;

		case ERammsUIEasing::EaseIn:
			return Alpha * Alpha;

		case ERammsUIEasing::EaseOut:
			return 1.0f - (1.0f - Alpha) * (1.0f - Alpha);

		case ERammsUIEasing::EaseInOut:
			if (Alpha < 0.5f)
				return 2.0f * Alpha * Alpha;
			else
				return 1.0f - FMath::Pow(-2.0f * Alpha + 2.0f, 2.0f) / 2.0f;

		case ERammsUIEasing::Bounce:
		{
			const float n1 = 7.5625f;
			const float d1 = 2.75f;

			if (Alpha < 1.0f / d1)
			{
				return n1 * Alpha * Alpha;
			}
			else if (Alpha < 2.0f / d1)
			{
				float t = Alpha - 1.5f / d1;
				return n1 * t * t + 0.75f;
			}
			else if (Alpha < 2.5f / d1)
			{
				float t = Alpha - 2.25f / d1;
				return n1 * t * t + 0.9375f;
			}
			else
			{
				float t = Alpha - 2.625f / d1;
				return n1 * t * t + 0.984375f;
			}
		}

		case ERammsUIEasing::Elastic:
		{
			const float c4 = (2.0f * PI) / 3.0f;
			if (Alpha == 0.0f)
				return 0.0f;
			if (Alpha == 1.0f)
				return 1.0f;
			return FMath::Pow(2.0f, -10.0f * Alpha) * FMath::Sin((Alpha * 10.0f - 0.75f) * c4) + 1.0f;
		}

		default:
			return Alpha;
	}
}

void URammsBaseWidget::UpdateAnimations(float DeltaTime)
{
	for (int32 i = ActiveAnimations.Num() - 1; i >= 0; --i)
	{
		FAnimationState& Anim = ActiveAnimations[i];

		if (!Anim.bActive)
			continue;

		Anim.ElapsedTime += DeltaTime;
		float Alpha = FMath::Clamp(Anim.ElapsedTime / Anim.Duration, 0.0f, 1.0f);
		float EasedAlpha = EvaluateEasing(Alpha, Anim.Easing);

		switch (Anim.Type)
		{
			case FAnimationState::EType::Fade:
			{
				CurrentOpacity = FMath::Lerp(Anim.StartValue.X, Anim.TargetValue.X, EasedAlpha);
				SetRenderOpacity(CurrentOpacity);
				break;
			}

			case FAnimationState::EType::Slide:
			{
				FVector2D CurrentOffset = FMath::Lerp(Anim.StartValue, Anim.TargetValue, EasedAlpha);
				SetRenderTranslation(CurrentOffset);
				break;
			}

			case FAnimationState::EType::Scale:
			{
				CurrentScale = FMath::Lerp(Anim.StartValue, Anim.TargetValue, EasedAlpha);
				SetRenderScale(CurrentScale);
				break;
			}
		}

		// Remove completed animations
		if (Alpha >= 1.0f)
		{
			ActiveAnimations.RemoveAt(i);
		}
	}
}

void URammsBaseWidget::StartAnimation(FAnimationState Animation)
{
	// Remove existing animations of the same type
	ActiveAnimations.RemoveAll([&Animation](const FAnimationState& Existing) {
		return Existing.Type == Animation.Type;
	});

	ActiveAnimations.Add(Animation);
}

// ==================== Robot Controller Discovery ====================

void URammsBaseWidget::SubscribeToRegistryChanges()
{
	if (bSubscribedToRegistry)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
		{
			Subsystem->OnControllerRegistryChanged.AddUniqueDynamic(this, &URammsBaseWidget::HandleControllerRegistryChanged);
			bSubscribedToRegistry = true;
		}
	}
}

void URammsBaseWidget::UnsubscribeFromRegistryChanges()
{
	if (!bSubscribedToRegistry)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
		{
			Subsystem->OnControllerRegistryChanged.RemoveDynamic(this, &URammsBaseWidget::HandleControllerRegistryChanged);
		}
	}
	bSubscribedToRegistry = false;
}

void URammsBaseWidget::HandleControllerRegistryChanged(AActor* Actor, bool bRegistered)
{
	if (!bAutoFindRobotController)
	{
		return;
	}

	if (bRegistered)
	{
		// A new controller was registered — try to resolve if we don't have one
		if (!ResolvedControllerActor.IsValid())
		{
			ResolveController();
		}
	}
	else
	{
		// A controller was unregistered — if it's ours, clear and try to find another
		if (ResolvedControllerActor.Get() == Actor)
		{
			ResolvedControllerActor.Reset();
			OnRobotControllerLost();

			// Try to find a replacement
			ResolveController();
		}
	}
}

void URammsBaseWidget::BeginDestroy()
{
	UnsubscribeFromRegistryChanges();
	Super::BeginDestroy();
}

void URammsBaseWidget::ResolveController()
{
	// Check if existing controller is still valid
	if (ResolvedControllerActor.IsValid())
	{
		return;
	}

	// Clear stale references
	ResolvedControllerActor.Reset();

	AActor* FoundActor = nullptr;

	// 1. Check explicit override first
	if (TargetRobotOverride)
	{
		if (TargetRobotOverride->GetClass()->ImplementsInterface(URammsRobotController::StaticClass()))
		{
			FoundActor = TargetRobotOverride;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: TargetRobotOverride '%s' does not implement IRammsRobotController"),
				*GetName(), *TargetRobotOverride->GetName());
		}
	}

	// 2. Fall back to subsystem auto-discovery
	if (!FoundActor)
	{
		if (UWorld* World = GetWorld())
		{
			if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
			{
				FoundActor = Subsystem->FindRobotController();
			}
		}
	}

	if (FoundActor)
	{
		ResolvedControllerActor = FoundActor;
		OnRobotControllerResolved(FoundActor);
	}
}

AActor* URammsBaseWidget::GetResolvedControllerActor() const
{
	return ResolvedControllerActor.Get();
}

bool URammsBaseWidget::HasResolvedController() const
{
	return ResolvedControllerActor.IsValid();
}

void URammsBaseWidget::OnRobotControllerResolved(AActor* ControllerActor)
{
	// Base implementation does nothing — override in derived classes
}

void URammsBaseWidget::OnRobotControllerLost()
{
	// Base implementation does nothing — override in derived classes
}
