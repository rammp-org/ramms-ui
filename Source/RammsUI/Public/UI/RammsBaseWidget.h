// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/RammsUIStyle.h"
#include "RammsBaseWidget.generated.h"

/**
 * Base class for all RammsUI widgets
 * Provides style application, animation helpers, and common utilities
 */
UCLASS(Abstract, Blueprintable)
class RAMMSUI_API URammsBaseWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** Current UI style */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	TObjectPtr<URammsUIStyle> Style;

	/** Whether to apply style automatically in NativeConstruct */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	bool bAutoApplyStyle = true;

	/** Whether to propagate style to child RammsBaseWidget instances */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	bool bPropagateStyleToChildren = true;

	/** Current opacity (for fade animations) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float CurrentOpacity = 1.0f;

	/** Current scale (for scale animations) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	FVector2D CurrentScale = FVector2D(1.0f, 1.0f);

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/**
	 * Set the UI style and apply it
	 */
	UFUNCTION(BlueprintCallable, Category = "Style")
	virtual void SetStyle(URammsUIStyle* NewStyle);

	/**
	 * Get the current UI style
	 */
	UFUNCTION(BlueprintPure, Category = "Style")
	URammsUIStyle* GetStyle() const { return Style; }

	/**
	 * Apply the current style to this widget
	 * Override this in derived classes to customize style application
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Style")
	void ApplyStyle();
	virtual void ApplyStyle_Implementation();

	// ==================== Animation Helpers ====================

	/**
	 * Fade in with animation
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void FadeIn(float Duration = -1.0f, ERammsUIEasing Easing = ERammsUIEasing::EaseInOut);

	/**
	 * Fade out with animation
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void FadeOut(float Duration = -1.0f, ERammsUIEasing Easing = ERammsUIEasing::EaseInOut);

	/**
	 * Slide in from direction (in screen space)
	 * @param FromOffset - Offset to slide from (e.g., FVector2D(0, -100) for top)
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void SlideIn(FVector2D FromOffset, float Duration = -1.0f, ERammsUIEasing Easing = ERammsUIEasing::EaseInOut);

	/**
	 * Slide out to direction (in screen space)
	 * @param ToOffset - Offset to slide to (e.g., FVector2D(0, 100) for bottom)
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void SlideOut(FVector2D ToOffset, float Duration = -1.0f, ERammsUIEasing Easing = ERammsUIEasing::EaseInOut);

	/**
	 * Scale in with animation
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void ScaleIn(float Duration = -1.0f, ERammsUIEasing Easing = ERammsUIEasing::EaseInOut);

	/**
	 * Scale out with animation
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void ScaleOut(float Duration = -1.0f, ERammsUIEasing Easing = ERammsUIEasing::EaseInOut);

	/**
	 * Stop all running Ramms animations (custom animation system)
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void StopRammsAnimations();

	// ==================== Layout Helpers ====================

	/**
	 * Set widget anchors (for responsive layout)
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void SetAnchors(FAnchors Anchors);

	/**
	 * Set widget alignment
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void SetAlignment(FVector2D Alignment);

	/**
	 * Set widget position (in slot if available)
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void SetPosition(FVector2D Position);

	/**
	 * Set widget size (in slot if available)
	 */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	void SetSize(FVector2D Size);

protected:
	/**
	 * Evaluate easing function
	 */
	static float EvaluateEasing(float Alpha, ERammsUIEasing Easing);

private:
	struct FAnimationState
	{
		bool bActive = false;
		float ElapsedTime = 0.0f;
		float Duration = 0.3f;
		ERammsUIEasing Easing = ERammsUIEasing::Linear;

		// Animation type-specific data
		enum class EType { Fade, Slide, Scale } Type;
		FVector2D StartValue = FVector2D::ZeroVector;
		FVector2D TargetValue = FVector2D::ZeroVector;
	};

	TArray<FAnimationState> ActiveAnimations;

	void UpdateAnimations(float DeltaTime);
	void StartAnimation(FAnimationState Animation);
	void PropagateStyleToChildren();
};
