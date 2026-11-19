#include "UI/RammsWorldTapLayer.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "RammsWorldTappable.h"

URammsWorldTapLayer::URammsWorldTapLayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Visible, not the UUserWidget default of SelfHitTestInvisible: this layer's whole job is to
	// be the thing a press lands on. It has no children of its own to hit-test instead.
	SetVisibility(ESlateVisibility::Visible);
}

void URammsWorldTapLayer::NativePreConstruct()
{
	Super::NativePreConstruct();

	// An archetype placed in a widget tree carries its own serialized Visibility, and a layer that
	// came through as SelfHitTestInvisible is silently inert -- it paints nothing, so there is
	// nothing to notice. Collapsed and Hidden are left alone: those are someone turning it off.
	const ESlateVisibility Current = GetVisibility();
	if (Current == ESlateVisibility::SelfHitTestInvisible || Current == ESlateVisibility::HitTestInvisible)
	{
		SetVisibility(ESlateVisibility::Visible);
	}
}

TSharedRef<SWidget> URammsWorldTapLayer::RebuildWidget()
{
	// A root of its own, so the layer has real geometry to be hit-tested over. SelfHitTestInvisible
	// on the canvas and Visible on the layer: this widget answers for its whole rect, and anything
	// a designer later puts inside it keeps its own hit testing.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TapCanvas"));
		if (Root)
		{
			Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			WidgetTree->RootWidget = Root;
		}
	}
	return Super::RebuildWidget();
}

bool URammsWorldTapLayer::ScreenPositionFromPointer(const FPointerEvent& PointerEvent, FVector2D& OutViewportPixels) const
{
	// Slate hands out absolute desktop coordinates; deprojection wants viewport pixels. The two
	// differ by the viewport's position on the desktop AND by DPI scale, so this conversion is not
	// optional -- skipping it puts the ray through a different part of the scene on every monitor.
	FVector2D PixelPosition = FVector2D::ZeroVector;
	FVector2D ViewportPosition = FVector2D::ZeroVector;
	USlateBlueprintLibrary::AbsoluteToViewport(this, PointerEvent.GetScreenSpacePosition(), PixelPosition, ViewportPosition);
	OutViewportPixels = PixelPosition;
	return true;
}

UObject* URammsWorldTapLayer::ResolveTappable(const FHitResult& Hit)
{
	// The component first: one actor may carry several separately tappable parts, and the part that
	// was actually hit is the more specific answer whenever it has one.
	if (UPrimitiveComponent* Component = Hit.GetComponent())
	{
		if (Component->Implements<URammsWorldTappable>())
		{
			return Component;
		}
	}
	if (AActor* Actor = Hit.GetActor())
	{
		if (Actor->Implements<URammsWorldTappable>())
		{
			return Actor;
		}
	}
	return nullptr;
}

UObject* URammsWorldTapLayer::FindTappableAtScreenPosition(FVector2D ScreenPosition, FHitResult& OutHit)
{
	OutHit = FHitResult();

	APlayerController* Controller = GetOwningPlayer();
	UWorld*			   World = GetWorld();
	if (!Controller || !World)
	{
		return nullptr;
	}

	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ZeroVector;
	if (!Controller->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, Origin, Direction))
	{
		return nullptr;
	}

	const FVector End = Origin + Direction * TapTraceDistance;

	FCollisionQueryParams Params(FName(TEXT("RammsWorldTap")), /*bTraceComplex=*/false);
	if (const APawn* Pawn = Controller->GetPawn())
	{
		Params.AddIgnoredActor(Pawn);
	}

	const int32 MaxPasses = bTapThroughOccluders ? FMath::Max(1, MaxOccludersToSearch) : 1;
	for (int32 Pass = 0; Pass < MaxPasses; ++Pass)
	{
		TArray<FHitResult> Hits;
		World->LineTraceMultiByChannel(Hits, Origin, End, TapChannel, Params);
		if (Hits.Num() == 0)
		{
			return nullptr;
		}

		// Sorted near to far, so the first tappable on the ray is the one the operator aimed at.
		for (const FHitResult& Hit : Hits)
		{
			if (UObject* Tappable = ResolveTappable(Hit))
			{
				OutHit = Hit;
				return Tappable;
			}
		}

		// Nothing tappable this side of whatever stopped the trace. A multi-trace ends at its first
		// blocker, so looking further means asking again without that actor: a marker drawn at an
		// object sits INSIDE that object's mesh, and stopping at the mesh would make the markers
		// that matter most -- the ones on real furniture -- the ones that cannot be tapped.
		const FHitResult& Blocker = Hits.Last();
		AActor*			  BlockingActor = Blocker.GetActor();
		if (!Blocker.bBlockingHit || !BlockingActor)
		{
			// The ray ran out rather than hitting anything; there is nothing further to look past.
			return nullptr;
		}
		Params.AddIgnoredActor(BlockingActor);
	}
	return nullptr;
}

bool URammsWorldTapLayer::TapAtScreenPosition(FVector2D ScreenPosition)
{
	if (!bTapsEnabled)
	{
		return false;
	}

	FHitResult Hit;
	UObject*   Tappable = FindTappableAtScreenPosition(ScreenPosition, Hit);
	if (!Tappable)
	{
		return false;
	}

	// The one way in. A marker's OnClicked and OnInputTouchBegin call the same event, and a
	// Blueprint that overrides it changes every route at once.
	IRammsWorldTappable::Execute_OnRammsWorldTapped(Tappable, Hit);
	return true;
}

FReply URammsWorldTapLayer::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Only the primary button taps. A right-click belongs to whatever is behind this layer.
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		FVector2D ScreenPosition = FVector2D::ZeroVector;
		if (ScreenPositionFromPointer(InMouseEvent, ScreenPosition) && TapAtScreenPosition(ScreenPosition))
		{
			return FReply::Handled();
		}
	}

	// Unhandled on purpose: it bubbles up to the HUD's background button, so tapping empty space
	// still does what it did before this layer existed.
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URammsWorldTapLayer::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent)
{
	FVector2D ScreenPosition = FVector2D::ZeroVector;
	if (ScreenPositionFromPointer(InTouchEvent, ScreenPosition) && TapAtScreenPosition(ScreenPosition))
	{
		return FReply::Handled();
	}
	return Super::NativeOnTouchStarted(InGeometry, InTouchEvent);
}
