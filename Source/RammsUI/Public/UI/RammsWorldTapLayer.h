// RammsWorldTapLayer.h
//
// Makes the 3D scene tappable from under a full-screen HUD.
//
// THE PROBLEM. W_RAMMS_UI's root is a full-screen Button (tap the background to go back), and a
// UButton handles OnMouseButtonDown. Slate therefore consumes every press before it reaches the
// game viewport, so APlayerController never sees the mouse button and UPrimitiveComponent's
// OnClicked / OnInputTouchBegin can never fire for ANY actor while the HUD is up. That is in the
// widget asset, not an editor artifact: it is just as true on the robot. The markers' collision,
// trace channel and tap payload were all correct the whole time; nothing was ever pressing them.
//
// THE FIX. This widget sits inside the HUD, directly above that background button and below
// everything else in it. A press reaches it first, it traces the view, and:
//
//   - it hit something implementing IRammsWorldTappable  -> call the interface, return Handled.
//   - it hit nothing tappable                            -> return Unhandled.
//
// Unhandled bubbles UP to the ancestor button, so tapping empty space still goes Back and every
// UMG control above this layer still takes its own presses first. Both behaviours survive; neither
// has to know about the other.
//
// Z-ORDER IS THE WHOLE INSTALLATION. "Above the background button, below the rest of the UI" is
// only expressible INSIDE the HUD's own tree -- a widget added to the player screen is either in
// front of the UI (and steals its presses) or behind the button (and never sees one). Put this as
// the FIRST child of the overlay that holds the HUD.
//
// It deliberately does not go through APlayerController. Besides the button above, this project
// has repeatedly found mouse buttons not reaching PlayerInput at all (PIE in the level viewport,
// pixel streaming) while Slate had the pointer throughout. Slate is the layer that reliably has
// the press, so the hit test is done from here.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "RammsWorldTapLayer.generated.h"

UCLASS(meta = (DisplayName = "Ramms World Tap Layer"))
class RAMMSUI_API URammsWorldTapLayer : public UUserWidget
{
	GENERATED_BODY()

public:
	URammsWorldTapLayer(const FObjectInitializer& ObjectInitializer);

	/** Off makes this layer pass every press straight through, as if it were not here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Interaction")
	bool bTapsEnabled = true;

	/**
	 * The channel the view is traced on. Visibility, to match what a collider blocks to be
	 * clickable at all and what APlayerController's own click trace uses, so a thing set up for
	 * one route is reachable by the other.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Interaction")
	TEnumAsByte<ECollisionChannel> TapChannel = ECC_Visibility;

	/** How far into the scene a tap reaches, in Unreal units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Interaction")
	float TapTraceDistance = 100000.0f;

	/**
	 * Refuse a tap on a tappable that solid geometry hides.
	 *
	 * OFF, because in this project "blocks the Visibility channel" is not a usable stand-in for
	 * "the operator can see it". BP_CameraProjectionActor's procedural mesh has its rendering
	 * visibility toggled off while its collision stays, so it is an invisible wall across the view
	 * that would hide most markers behind it -- measured: with occlusion respected, three of four
	 * staged markers became untappable and nothing on screen said why.
	 *
	 * The honest version of this test needs a notion of visible occluders that the engine's
	 * collision channels do not give us here. Until there is one, a tap reaches the nearest
	 * tappable on the ray whatever is in front of it. Turn this on in a project where the
	 * Visibility channel does mean visibility.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Interaction")
	bool bRejectOccludedTaps = false;

	/**
	 * Trace the view at a VIEWPORT PIXEL position and tap the nearest tappable on that ray.
	 *
	 * Public and callable so a tap can be driven without a pointer at all -- a test, a remote
	 * command, an on-screen control that names a target. Returns whether anything took it.
	 */
	UFUNCTION(BlueprintCallable, Category = "RAMMS|Interaction")
	bool TapAtScreenPosition(FVector2D ScreenPosition);

	/** The thing a tap at this pixel would reach, without tapping it. Null when that is nothing. */
	UFUNCTION(BlueprintCallable, Category = "RAMMS|Interaction")
	UObject* FindTappableAtScreenPosition(FVector2D ScreenPosition, FHitResult& OutHit);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void				NativePreConstruct() override;
	virtual FReply				NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply				NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;

	/** Absolute Slate coordinates -> viewport pixels, which is what deprojection wants. */
	bool ScreenPositionFromPointer(const FPointerEvent& PointerEvent, FVector2D& OutViewportPixels) const;

	/** The interface implementer a hit stands for: the component if it implements it, else the actor. */
	static UObject* ResolveTappable(const FHitResult& Hit);
};
