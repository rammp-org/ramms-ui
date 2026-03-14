// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "RammsJoystickWidget.generated.h"

/**
 * Virtual joystick widget for manual robot control.
 * Outputs a normalized FVector2D (-1 to 1 on each axis).
 * Supports mouse and touch input.
 * Auto-discovers actors implementing IRammsRobotController.
 */
UCLASS(meta = (DisplayName = "Ramms Joystick"))
class RAMMSUI_API URammsJoystickWidget : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	URammsJoystickWidget(const FObjectInitializer& ObjectInitializer);

protected:
	/** Joystick radius in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Joystick", meta = (ClampMin = "30.0"))
	float JoystickRadius = 80.0f;

	/** Thumb (knob) radius in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Joystick", meta = (ClampMin = "10.0"))
	float ThumbRadius = 25.0f;

	/** Dead zone (0-1, fraction of radius where input is zero) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Joystick", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float DeadZone = 0.1f;

	/** Auto-center when released */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Joystick")
	bool bAutoCenter = true;

	/** Lock to single axis (X or Y only, whichever is dominant) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Joystick")
	bool bLockToAxis = false;

	/** Background color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor BackgroundColor = FLinearColor(0.1f, 0.1f, 0.1f, 0.5f);

	/** Thumb color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor ThumbColor = FLinearColor(0.3f, 0.6f, 1.0f, 0.9f);

	/** Active thumb color (while dragging) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor ThumbActiveColor = FLinearColor(0.5f, 0.8f, 1.0f, 1.0f);

	// Widget references (built programmatically)
	UPROPERTY()
	TObjectPtr<UCanvasPanel> JoystickCanvas;

	UPROPERTY()
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY()
	TObjectPtr<UImage> ThumbImage;

	/** Current input value (-1 to 1 per axis) */
	FVector2D CurrentValue = FVector2D::ZeroVector;

	/** Is the joystick being interacted with */
	bool bIsActive = false;

	/** Center position of the joystick in local space */
	FVector2D JoystickCenter = FVector2D::ZeroVector;

public:
	/** Fired when joystick value changes */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJoystickValueChanged, FVector2D, Value);
	UPROPERTY(BlueprintAssignable, Category = "Joystick")
	FOnJoystickValueChanged OnValueChanged;

	/** Fired when joystick is released */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJoystickReleased);
	UPROPERTY(BlueprintAssignable, Category = "Joystick")
	FOnJoystickReleased OnReleased;

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void ApplyStyle_Implementation() override;

	/** Get current joystick value (-1 to 1 per axis) */
	UFUNCTION(BlueprintPure, Category = "Joystick")
	FVector2D GetValue() const { return CurrentValue; }

	/** Is the joystick currently being used */
	UFUNCTION(BlueprintPure, Category = "Joystick")
	bool IsActive() const { return bIsActive; }

	/** Reset joystick to center */
	UFUNCTION(BlueprintCallable, Category = "Joystick")
	void ResetToCenter();

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual void BuildWidgetTree() override;
	void UpdateThumbPosition(const FGeometry& InGeometry, FVector2D LocalPos);
	void SetThumbOffset(FVector2D Offset);

	virtual void OnRobotControllerResolved(AActor* ControllerActor) override;
};
