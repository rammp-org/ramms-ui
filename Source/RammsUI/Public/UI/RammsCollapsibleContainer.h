// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Components/NamedSlot.h"
#include "Components/ScrollBox.h"
#include "RammsCollapsibleContainer.generated.h"

/**
 * Animation style for expand/collapse transitions
 */
UENUM(BlueprintType)
enum class ERammsCollapseAnimation : uint8
{
	None	   UMETA(DisplayName = "None (Instant)"),
	ScaleY	   UMETA(DisplayName = "Scale Vertical"),
	SlideDown  UMETA(DisplayName = "Slide Down"),
	SlideUp	   UMETA(DisplayName = "Slide Up"),
	SlideLeft  UMETA(DisplayName = "Slide Left"),
	SlideRight UMETA(DisplayName = "Slide Right"),
	Fade	   UMETA(DisplayName = "Fade")
};

/**
 * A container widget with a header bar that can expand/collapse its content.
 * The header always stays visible with a toggle button.
 * Content can be any child widgets added to the ContentSlot.
 */
UCLASS(meta = (DisplayName = "Ramms Collapsible Container"))
class RAMMSUI_API URammsCollapsibleContainer : public URammsBaseWidget
{
	GENERATED_BODY()

protected:
	/** Header title text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collapsible", meta = (ExposeOnSpawn = true))
	FText HeaderTitle = FText::FromString(TEXT("Panel"));

	/** Whether content is expanded */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collapsible")
	bool bIsExpanded = true;

	/** Animation to use for expand/collapse */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collapsible")
	ERammsCollapseAnimation CollapseAnimation = ERammsCollapseAnimation::ScaleY;

	/** Animation duration in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collapsible", meta = (ClampMin = "0.0"))
	float AnimationDuration = 0.25f;

	/** Slide/scale offset in pixels (for slide animations) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collapsible")
	float SlideOffset = 150.0f;

	/** Show border around the container */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	bool bShowBorder = true;

	/** Maximum height for the content area in pixels. 0 = auto (use content's natural height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collapsible", meta = (ClampMin = "0.0"))
	float MaxContentHeight = 0.0f;

	/** Enable scrollbar when content overflows */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collapsible")
	bool bEnableScrolling = true;

	// Widget references — use BindWidgetOptional so a Widget Blueprint can provide these.
	// If not provided (pure C++), BuildWidgetTree creates them programmatically.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ContainerBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> HeaderBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HeaderLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ToggleButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ToggleIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> ContentSizeBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ContentScrollBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> ContentBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UNamedSlot> ContentSlot;

	/** Animation state tracking */
	bool  bIsAnimating = false;
	float AnimationProgress = 1.0f; // 1 = fully expanded, 0 = fully collapsed
	float AnimationTarget = 1.0f;
	float ExpandedContentHeight = 0.0f; // Cached height of content when expanded
	bool  bNeedsCacheHeight = true;

	/** Cached expanded size for Canvas Panel slot resizing */
	FVector2D CachedExpandedSlotSize = FVector2D::ZeroVector;

	/** Deferred initial collapse — resize the canvas slot after the first layout pass */
	bool bPendingInitialCollapse = false;

public:
	/** Fired when expand state changes */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExpandStateChanged, bool, bExpanded);
	UPROPERTY(BlueprintAssignable, Category = "Collapsible")
	FOnExpandStateChanged OnExpandStateChanged;

	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void ApplyStyle_Implementation() override;
	virtual void SynchronizeProperties() override;

	/** Get the content box to add child widgets to */
	UFUNCTION(BlueprintPure, Category = "Collapsible")
	UVerticalBox* GetContentBox() const { return ContentBox; }

	/** Add a widget to the content area */
	UFUNCTION(BlueprintCallable, Category = "Collapsible")
	void AddContentChild(UWidget* Child);

	/** Toggle expand/collapse */
	UFUNCTION(BlueprintCallable, Category = "Collapsible")
	void ToggleExpand();

	/** Set expand state */
	UFUNCTION(BlueprintCallable, Category = "Collapsible")
	void SetExpanded(bool bExpanded, bool bAnimate = true);

	/** Is currently expanded */
	UFUNCTION(BlueprintPure, Category = "Collapsible")
	bool IsExpanded() const { return bIsExpanded; }

	/** Set the header title */
	UFUNCTION(BlueprintCallable, Category = "Collapsible")
	void SetHeaderTitle(FText Title);

	/** Set the collapse animation type */
	UFUNCTION(BlueprintCallable, Category = "Collapsible")
	void SetCollapseAnimation(ERammsCollapseAnimation Animation) { CollapseAnimation = Animation; }

	/** Set the maximum content height (0 = auto) */
	UFUNCTION(BlueprintCallable, Category = "Collapsible")
	void SetMaxContentHeight(float Height);

protected:
	virtual void ResetCachedWidgets() override;
	virtual void BuildWidgetTree() override;
	void		 EnsureScrollableContent();
	void		 UpdateToggleIcon();
	void		 UpdateHeaderCornerRadii();
	void		 ApplyAnimationState(float Alpha);
	void		 UpdateParentSlotSize(float Alpha);
	void		 SetContentSlotFill(bool bFill);
	float		 GetEffectiveMaxHeight() const;

	UFUNCTION()
	void OnToggleClicked();
};
