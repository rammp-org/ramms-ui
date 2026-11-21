// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsJoystickWidget.h"
#include "RammsControlTypes.h"
#include "RammsSurfaceJoystick.generated.h"

/**
 * A joystick that drives two axes of a control surface (IRammsControlSink):
 * the thumb's Y feeds ControlIdY (forward), its X feeds ControlIdX (turn),
 * and letting go releases both so they spring back. This is the touch path
 * for driving — the Pixel Streaming page, a tablet, or the panel's own
 * joystick for any paired Continuous axes (drive, arm move, camera orbit).
 *
 * The sink is found through URammsUISubsystem's control-surface registry
 * unless TargetSink is set (or SetTarget is called by the panel).
 */
UCLASS(meta = (DisplayName = "Ramms Surface Joystick"))
class RAMMSUI_API URammsSurfaceJoystick : public URammsJoystickWidget
{
	GENERATED_BODY()

public:
	URammsSurfaceJoystick(const FObjectInitializer& ObjectInitializer);

	/** Object implementing IRammsControlSink; auto-found when null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	TObjectPtr<UObject> TargetSink;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	bool bAutoFindSink = true;

	/** Control fed by the thumb's horizontal deflection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	FName ControlIdX = FName("drive.turn");

	/** Control fed by the thumb's vertical deflection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	FName ControlIdY = FName("drive.forward");

	/** Widget space has Y down; on, pushing the thumb up is +Y (forward). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	bool bInvertY = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	ERammsControlSource Source = ERammsControlSource::Touch;

	/** While nobody holds the thumb, show what the two controls are commanded by any other source -- the
	 *  keyboard, a gamepad, a script, an autonomy client -- so the stick moves whenever the robot is driven.
	 *  Always on in the Sim look (ERammsHudLook); set this to have it in the Classic look too. Display only:
	 *  nothing is sent to the sink, and GetValue stays the thumb's own input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	bool bMirrorCommand = false;

	/** What the thumb shows of that command, -1..1 per axis as the controls have it (X = ControlIdX, Y =
	 *  ControlIdY, so forward is +Y whatever bInvertY); zero while held and when not mirroring. */
	UFUNCTION(BlueprintPure, Category = "Control Surface")
	FVector2D GetMirroredValue() const { return MirrorShown; }

	UFUNCTION(BlueprintCallable, Category = "Control Surface")
	void SetTarget(UObject* Sink, FName IdX, FName IdY);

	/** Resize the joystick (the base keeps its radii protected). Re-lays out
	 *  the background and thumb when the widget tree already exists, so it
	 *  works after CreateWidget too. */
	UFUNCTION(BlueprintCallable, Category = "Control Surface")
	void SetRadii(float InJoystickRadius, float InThumbRadius);

	/** Feed a value as if the thumb were there (tests, scripted demos). */
	UFUNCTION(BlueprintCallable, Category = "Control Surface")
	void SimulateInput(FVector2D Value);

	UFUNCTION(BlueprintCallable, Category = "Control Surface")
	void SimulateRelease();

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Real touch (a tablet, the Pixel Streaming page) arrives through the touch
	// overrides; route it to the base joystick's pointer handling so a finger
	// drives and releases the axes exactly like the mouse.
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

private:
	UFUNCTION()
	void HandleValue(FVector2D Value);
	UFUNCTION()
	void HandleReleased();

	UObject* ResolveSink();
	void	 Push(FVector2D Value);
	void	 Release();
	/** Move the thumb toward what the sink reports as commanded (bMirrorCommand), or back to centre. */
	void TickMirror(float DeltaSeconds);

	/** The mirrored command as the thumb shows it now (eased toward the sink's). */
	FVector2D MirrorShown = FVector2D::ZeroVector;
};
