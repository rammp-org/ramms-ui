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
	// AbsoluteToViewport silently writes (0,0) when there is no game viewport to convert against,
	// and (0,0) is a perfectly valid pixel: a press anywhere would then trace through the top-left
	// corner and could report a tap on whatever happens to be there. Check its own precondition
	// first and say no instead.
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !World->GetGameViewport())
	{
		return false;
	}

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

	// bIgnoreBlocks: collect TOUCHES along the whole ray and never stop at a blocker.
	//
	// This is the difference between the feature working and not. A detection marker overlaps the
	// tap channel rather than blocking it (ARammsDetectionWorldMarker::TapChannel says why it must
	// not block), so it is a touch -- and an ordinary multi-trace ends at the first blocking hit,
	// which discards every touch beyond it. The markers sit AT the objects they stand for, behind
	// those objects' meshes and, in this project, behind the camera projection actor's hidden
	// procedural mesh. Measured without this flag: one marker of four was reachable.
	//
	// Ignoring blocks is also why nothing here has to "look past" an occluder a few at a time: a
	// rule that drops blockers to keep searching has to decide how many to drop, and any answer to
	// that is arbitrary. Occlusion, if it is wanted at all, is one explicit test below.
	Params.bIgnoreBlocks = true;

	TArray<FHitResult> Hits;
	World->LineTraceMultiByChannel(Hits, Origin, End, TapChannel, Params);

	const FHitResult* Nearest = nullptr;
	UObject*		  Tappable = nullptr;
	for (const FHitResult& Hit : Hits) // sorted near to far
	{
		if (UObject* Candidate = ResolveTappable(Hit))
		{
			Nearest = &Hit;
			Tappable = Candidate;
			break;
		}
	}
	if (!Tappable)
	{
		return nullptr;
	}

	if (bRejectOccludedTaps)
	{
		FCollisionQueryParams OcclusionParams(FName(TEXT("RammsWorldTapOcclusion")), /*bTraceComplex=*/false);
		if (const APawn* Pawn = Controller->GetPawn())
		{
			OcclusionParams.AddIgnoredActor(Pawn);
		}
		FHitResult Blocker;
		if (World->LineTraceSingleByChannel(Blocker, Origin, End, TapChannel, OcclusionParams)
			&& Blocker.Distance < Nearest->Distance)
		{
			return nullptr;
		}
	}

	OutHit = *Nearest;
	return Tappable;
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

	// The one way in. A marker's OnClicked and OnInputTouchBegin call this same event where they
	// fire at all, so a Blueprint override of it covers every route rather than just this one.
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
