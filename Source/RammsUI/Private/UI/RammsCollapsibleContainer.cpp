// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsCollapsibleContainer.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/CanvasPanelSlot.h"

void URammsCollapsibleContainer::ResetCachedWidgets()
{
	ContainerBorder = nullptr;
	HeaderBorder = nullptr;
	HeaderLabel = nullptr;
	ToggleButton = nullptr;
	ToggleIcon = nullptr;
	ContentSizeBox = nullptr;
	ContentScrollBox = nullptr;
	ContentBox = nullptr;
	ContentSlot = nullptr;
}

void URammsCollapsibleContainer::BuildWidgetTree()
{
	if (!WidgetTree)
		return;

	if (ContentBox)
		return;

	// Root: outer border with rounded corners
	ContainerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ContainerBorder"));
	ContainerBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(
		FLinearColor(0.06f, 0.06f, 0.08f, 0.92f), 4.0f, FLinearColor(0.3f, 0.3f, 0.3f, 1.0f), 1.0f);
	ContainerBorder->SetPadding(FMargin(1.0f));
	ContainerBorder->SetClipping(EWidgetClipping::ClipToBounds);
	WidgetTree->RootWidget = ContainerBorder;

	UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainVBox"));
	ContainerBorder->AddChild(MainVBox);

	// Header (always visible) — rounded top corners, inset from outer border
	HeaderBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HeaderBorder"));
	HeaderBorder->Background = URammsUIStyle::MakeRoundedBoxBrushEx(
		FLinearColor(0.1f, 0.1f, 0.12f, 1.0f), FVector4(3.0f, 3.0f, 0.0f, 0.0f));
	HeaderBorder->SetPadding(FMargin(8.0f, 6.0f));
	UVerticalBoxSlot* HeaderSlot = MainVBox->AddChildToVerticalBox(HeaderBorder);
	if (HeaderSlot)
	{
		HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		HeaderSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderRow"));
	HeaderBorder->AddChild(HeaderRow);

	HeaderLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HeaderLabel"));
	HeaderLabel->SetText(HeaderTitle);
	HeaderLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UHorizontalBoxSlot* LabelSlot = HeaderRow->AddChildToHorizontalBox(HeaderLabel);
	if (LabelSlot)
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	ToggleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ToggleButton"));
	ToggleButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	UHorizontalBoxSlot* BtnSlot = HeaderRow->AddChildToHorizontalBox(ToggleButton);
	if (BtnSlot)
	{
		BtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		BtnSlot->SetVerticalAlignment(VAlign_Center);
		BtnSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
	}

	ToggleIcon = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ToggleIcon"));
	ToggleIcon->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ToggleButton->AddChild(ToggleIcon);

	// Content area: SizeBox -> ScrollBox -> VBox -> NamedSlot
	ContentSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ContentSizeBox"));
	ContentSizeBox->SetClipping(EWidgetClipping::ClipToBounds);
	UVerticalBoxSlot* SizeBoxSlot = MainVBox->AddChildToVerticalBox(ContentSizeBox);
	if (SizeBoxSlot)
	{
		SizeBoxSlot->SetHorizontalAlignment(HAlign_Fill);
		if (bIsExpanded)
		{
			// Expanded: Fill slot so ScrollBox gets constrained by parent
			SizeBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		else
		{
			// Collapsed: Auto slot + HeightOverride(0) -- stays in layout to preserve width
			SizeBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	ContentScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ContentScrollBox"));
	ContentScrollBox->SetOrientation(EOrientation::Orient_Vertical);
	ContentScrollBox->SetAlwaysShowScrollbar(true);
	ContentScrollBox->SetAlwaysShowScrollbarTrack(true);
	ContentSizeBox->AddChild(ContentScrollBox);

	ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
	ContentScrollBox->AddChild(ContentBox);

	ContentSlot = WidgetTree->ConstructWidget<UNamedSlot>(UNamedSlot::StaticClass(), TEXT("ContentSlot"));
	ContentBox->AddChildToVerticalBox(ContentSlot);

	// Initial collapsed state: HeightOverride(0) + HitTestInvisible
	// NOT Visibility::Collapsed, because Collapsed removes from layout and loses width
	if (!bIsExpanded)
	{
		ContentSizeBox->SetHeightOverride(0.0f);
		ContentSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		ContentSizeBox->SetRenderOpacity(0.0f);
		AnimationProgress = 0.0f;
		AnimationTarget = 0.0f;
	}
}

void URammsCollapsibleContainer::EnsureScrollableContent()
{
	if (!ContentBox || !WidgetTree)
		return;

	if (!ContentScrollBox)
	{
		UPanelWidget* BoxParent = ContentBox->GetParent();
		if (!BoxParent)
			return;

		ContentScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ContentScrollBox"));
		ContentScrollBox->SetOrientation(EOrientation::Orient_Vertical);

		BoxParent->RemoveChild(ContentBox);
		BoxParent->AddChild(ContentScrollBox);
		ContentScrollBox->AddChild(ContentBox);
	}

	if (!ContentSizeBox)
	{
		UPanelWidget* ScrollParent = ContentScrollBox->GetParent();
		if (!ScrollParent)
			return;

		ContentSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ContentSizeBox"));
		ContentSizeBox->SetClipping(EWidgetClipping::ClipToBounds);

		ScrollParent->RemoveChild(ContentScrollBox);

		if (UVerticalBox* VBoxParent = Cast<UVerticalBox>(ScrollParent))
		{
			UVerticalBoxSlot* FillSlot = VBoxParent->AddChildToVerticalBox(ContentSizeBox);
			if (FillSlot)
			{
				FillSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				FillSlot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
		else
		{
			ScrollParent->AddChild(ContentSizeBox);
		}

		ContentSizeBox->AddChild(ContentScrollBox);
	}

	// Ensure correct slot sizing for current state
	SetContentSlotFill(bIsExpanded && !bIsAnimating);
}

void URammsCollapsibleContainer::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsCollapsibleContainer::NativePreConstruct()
{
	Super::NativePreConstruct();
	// Build widget tree for designer preview (guard in BuildWidgetTree prevents double-build)
	BuildWidgetTree();

	// Only apply designer-specific collapsed/expanded preview state.
	// At runtime, NativeConstruct handles the correct initial state.
	if (!IsDesignTime())
		return;

	// Apply collapsed/expanded visual state for designer preview.
	// In the designer there are no animations. Use HeightOverride(0) + Hidden
	// to maintain content width while hiding the content area.
	if (ContentSizeBox)
	{
		if (bIsExpanded)
		{
			ContentSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			SetContentSlotFill(true);
			if (MaxContentHeight > 0.0f)
				ContentSizeBox->SetHeightOverride(MaxContentHeight);
			else
				ContentSizeBox->ClearHeightOverride();
			ContentSizeBox->SetRenderOpacity(1.0f);
		}
		else
		{
			// Collapsed in designer: HeightOverride(0) hides content while
			// preserving width. Hidden visibility stops rendering but keeps
			// the widget in layout so the container width is maintained.
			SetContentSlotFill(false);
			ContentSizeBox->SetHeightOverride(0.0f);
			ContentSizeBox->SetVisibility(ESlateVisibility::Hidden);
			ContentSizeBox->SetRenderOpacity(0.0f);
		}
	}
	if (ToggleIcon)
	{
		UpdateToggleIcon();
	}
}

void URammsCollapsibleContainer::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (HeaderLabel)
	{
		HeaderLabel->SetText(HeaderTitle);
	}
	if (HeaderBorder)
	{
		// Keep the header border (and toggle button) always visible.
		// Only collapse the label text when the title is empty.
		HeaderBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (HeaderLabel)
	{
		HeaderLabel->SetVisibility(HeaderTitle.IsEmptyOrWhitespace()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::SelfHitTestInvisible);
	}
	if (ToggleIcon)
	{
		UpdateToggleIcon();
	}

	if (ContentScrollBox)
	{
		ContentScrollBox->SetScrollBarVisibility(
			bEnableScrolling ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		ContentScrollBox->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	}

	if (ContentSizeBox && !bIsAnimating)
	{
		if (bIsExpanded)
		{
			ContentSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			ContentSizeBox->ClearMinDesiredWidth();
			SetContentSlotFill(true);
			if (MaxContentHeight > 0.0f)
			{
				ContentSizeBox->SetHeightOverride(MaxContentHeight);
			}
			else
			{
				ContentSizeBox->ClearHeightOverride();
			}
			ContentSizeBox->SetRenderOpacity(1.0f);
		}
		else if (IsDesignTime())
		{
			// Designer: HeightOverride(0) + Hidden to collapse content while
			// keeping width in layout (so the container doesn't shrink in X)
			SetContentSlotFill(false);
			ContentSizeBox->SetHeightOverride(0.0f);
			ContentSizeBox->SetVisibility(ESlateVisibility::Hidden);
			ContentSizeBox->SetRenderOpacity(0.0f);
		}
		else
		{
			// Runtime collapsed: HeightOverride(0) preserves width for animations
			SetContentSlotFill(false);
			ContentSizeBox->SetHeightOverride(0.0f);
			ContentSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
			ContentSizeBox->SetRenderOpacity(0.0f);
		}
	}
}

void URammsCollapsibleContainer::NativeConstruct()
{
	Super::NativeConstruct();

	EnsureScrollableContent();

	if (ToggleButton)
	{
		ToggleButton->OnClicked.AddUniqueDynamic(this, &URammsCollapsibleContainer::OnToggleClicked);
	}

	UpdateToggleIcon();

	if (HeaderLabel)
	{
		HeaderLabel->SetText(HeaderTitle);
	}

	if (ContainerBorder)
	{
		ContainerBorder->SetClipping(EWidgetClipping::ClipToBounds);
	}

	if (ContentSizeBox)
	{
		ContentSizeBox->SetClipping(EWidgetClipping::ClipToBounds);
	}

	if (ContentScrollBox)
	{
		ContentScrollBox->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
		ContentScrollBox->SetScrollBarVisibility(
			bEnableScrolling ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	// Cache expanded slot size for Canvas Panel parents
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CachedExpandedSlotSize = CanvasSlot->GetSize();

		// If AutoSize is off but the slot has no usable dimensions, the slot was
		// likely never manually sized (the designer auto-enabled AutoSize for the
		// near-zero initial slot, masking the zero size). Enable AutoSize at
		// runtime so the widget has valid dimensions.
		if (!CanvasSlot->GetAutoSize() && CachedExpandedSlotSize.X < 1.0f && CachedExpandedSlotSize.Y < 1.0f)
		{
			CanvasSlot->SetAutoSize(true);
		}
	}

	// Apply initial state
	if (ContentSizeBox)
	{
		if (bIsExpanded)
		{
			ContentSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			ContentSizeBox->ClearMinDesiredWidth();
			SetContentSlotFill(true);
			if (MaxContentHeight > 0.0f)
			{
				ContentSizeBox->SetHeightOverride(MaxContentHeight);
			}
			else
			{
				ContentSizeBox->ClearHeightOverride();
			}
			ContentSizeBox->SetRenderOpacity(1.0f);
		}
		else
		{
			// Collapsed: Auto slot + HeightOverride(0) to preserve width
			SetContentSlotFill(false);
			ContentSizeBox->SetHeightOverride(0.0f);
			ContentSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
			ContentSizeBox->SetRenderOpacity(0.0f);
			// Defer the canvas-slot resize to the first tick so Slate has
			// completed at least one layout pass and slot sizes are reliable.
			bPendingInitialCollapse = true;
		}
	}
}

void URammsCollapsibleContainer::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Deferred initial collapse: now that the first layout pass has happened,
	// the canvas slot size is reliable. Re-cache and resize the slot.
	if (bPendingInitialCollapse)
	{
		bPendingInitialCollapse = false;
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			FVector2D SlotSize = CanvasSlot->GetSize();
			if (SlotSize.Y > 0.0f)
			{
				CachedExpandedSlotSize = SlotSize;
			}
			else if (CanvasSlot->GetAutoSize())
			{
				// AutoSize slot with no stored size — use actual rendered geometry
				FVector2D Actual = MyGeometry.GetLocalSize();
				if (Actual.Y > 0.0f)
				{
					CachedExpandedSlotSize = Actual;
				}
			}
		}
		UpdateParentSlotSize(0.0f);
	}

	// Cache content height while expanded and stable
	if (bNeedsCacheHeight && bIsExpanded && !bIsAnimating && ContentSizeBox)
	{
		float ActualH = ContentSizeBox->GetCachedGeometry().GetLocalSize().Y;
		if (ActualH > 0.0f)
		{
			ExpandedContentHeight = ActualH;
			bNeedsCacheHeight = false;
		}
		else if (ContentBox)
		{
			FVector2D DesiredSize = ContentBox->GetDesiredSize();
			if (DesiredSize.Y > 0.0f)
			{
				ExpandedContentHeight = DesiredSize.Y + 16.0f;
				bNeedsCacheHeight = false;
			}
		}

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			FVector2D SlotSize = CanvasSlot->GetSize();
			if (SlotSize.Y > 0.0f)
			{
				CachedExpandedSlotSize = SlotSize;
			}
		}
	}

	if (!bIsAnimating)
		return;

	// Advance animation
	float Direction = (AnimationTarget > AnimationProgress) ? 1.0f : -1.0f;
	float Speed = (AnimationDuration > 0.0f) ? (1.0f / AnimationDuration) : 100.0f;
	AnimationProgress += Direction * Speed * InDeltaTime;
	AnimationProgress = FMath::Clamp(AnimationProgress, 0.0f, 1.0f);

	float EasedAlpha = EvaluateEasing(AnimationProgress, ERammsUIEasing::EaseInOut);
	ApplyAnimationState(EasedAlpha);
	UpdateParentSlotSize(EasedAlpha);

	if (FMath::IsNearlyEqual(AnimationProgress, AnimationTarget, 0.001f))
	{
		AnimationProgress = AnimationTarget;
		bIsAnimating = false;

		if (ContentSizeBox)
		{
			if (AnimationTarget >= 1.0f)
			{
				// Fully expanded: Fill slot for scrolling, clear height override
				SetContentSlotFill(true);
				if (MaxContentHeight > 0.0f)
				{
					ContentSizeBox->SetHeightOverride(MaxContentHeight);
				}
				else
				{
					ContentSizeBox->ClearHeightOverride();
				}
				ContentSizeBox->ClearMinDesiredWidth();
				ContentSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				ContentSizeBox->SetRenderOpacity(1.0f);
			}
			else
			{
				// Fully collapsed: Auto slot + HeightOverride(0) -- stays in layout for width
				SetContentSlotFill(false);
				ContentSizeBox->SetHeightOverride(0.0f);
				ContentSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
				ContentSizeBox->SetRenderOpacity(0.0f);
			}
		}

		UpdateParentSlotSize(AnimationTarget);
	}
}

void URammsCollapsibleContainer::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	float Radius = Style->Border.CornerRadiusMedium;
	float BorderW = Style->Border.BorderWidth;

	if (ContainerBorder)
	{
		FLinearColor Bg = Style->Colors.Background;
		Bg.A = 0.92f;
		if (bShowBorder)
		{
			FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrush(Bg, Radius, Style->Colors.Border, BorderW);
			URammsUIStyle::ApplyRoundedBrushToBorder(ContainerBorder, Brush);
		}
		else
		{
			FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrush(FLinearColor::Transparent, Radius);
			URammsUIStyle::ApplyRoundedBrushToBorder(ContainerBorder, Brush);
		}
		// Inset children by the border width so content doesn't overlap the rounded outline
		ContainerBorder->SetPadding(FMargin(BorderW));
	}

	if (HeaderBorder)
	{
		UpdateHeaderCornerRadii();
	}

	if (HeaderLabel)
	{
		HeaderLabel->SetFont(Style->Typography.HeadingSmall);
		HeaderLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}

	if (ToggleIcon)
	{
		ToggleIcon->SetFont(Style->Typography.Body);
		ToggleIcon->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
	}

	if (Style->ExpandCollapseCurve.Duration > 0.0f)
	{
		AnimationDuration = Style->ExpandCollapseCurve.Duration;
	}

	// Style the scrollbar using the shared style definition
	if (ContentScrollBox)
	{
		URammsUIStyle::ApplyScrollBarStyle(ContentScrollBox, Style->ScrollBar);
	}
}

void URammsCollapsibleContainer::AddContentChild(UWidget* Child)
{
	if (ContentBox && Child)
	{
		ContentBox->AddChildToVerticalBox(Child);
		bNeedsCacheHeight = true;
	}
}

void URammsCollapsibleContainer::ToggleExpand()
{
	SetExpanded(!bIsExpanded, true);
}

void URammsCollapsibleContainer::SetExpanded(bool bExpanded, bool bAnimate)
{
	if (bIsExpanded == bExpanded && !bIsAnimating)
		return;

	bIsExpanded = bExpanded;
	UpdateToggleIcon();
	UpdateHeaderCornerRadii();
	OnExpandStateChanged.Broadcast(bIsExpanded);

	if (!ContentSizeBox)
	{
		if (ContentBox)
		{
			ContentBox->SetVisibility(bIsExpanded ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		UpdateParentSlotSize(bIsExpanded ? 1.0f : 0.0f);
		return;
	}

	AnimationTarget = bIsExpanded ? 1.0f : 0.0f;

	// Cache sizes before collapsing
	if (!bIsExpanded)
	{
		if (ContentSizeBox)
		{
			float ActualH = ContentSizeBox->GetCachedGeometry().GetLocalSize().Y;
			if (ActualH > 0.0f)
			{
				ExpandedContentHeight = ActualH;
			}
			// Cache width so collapsed state maintains the same width (scrollbar included)
			float ActualW = ContentSizeBox->GetCachedGeometry().GetLocalSize().X;
			if (ActualW > 0.0f)
			{
				ContentSizeBox->SetMinDesiredWidth(ActualW);
			}
		}
		if (ExpandedContentHeight <= 0.0f && ContentBox)
		{
			FVector2D DesiredSize = ContentBox->GetDesiredSize();
			if (DesiredSize.Y > 0.0f)
			{
				ExpandedContentHeight = DesiredSize.Y + 16.0f;
			}
		}
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			FVector2D SlotSize = CanvasSlot->GetSize();
			if (SlotSize.Y > 0.0f)
			{
				CachedExpandedSlotSize = SlotSize;
			}
		}
	}

	if (ExpandedContentHeight <= 0.0f)
	{
		ExpandedContentHeight = 200.0f;
	}

	if (bAnimate && CollapseAnimation != ERammsCollapseAnimation::None && AnimationDuration > 0.0f)
	{
		bIsAnimating = true;

		// During animation: Auto slot so HeightOverride controls actual space taken
		SetContentSlotFill(false);
		ContentSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		if (bIsExpanded)
		{
			ContentSizeBox->SetHeightOverride(0.0f);
			ContentSizeBox->SetRenderOpacity(0.0f);
			AnimationProgress = 0.0f;
		}
		else
		{
			ContentSizeBox->SetHeightOverride(GetEffectiveMaxHeight());
			AnimationProgress = 1.0f;
		}
	}
	else
	{
		// Instant (no animation)
		bIsAnimating = false;
		AnimationProgress = AnimationTarget;

		if (bIsExpanded)
		{
			SetContentSlotFill(true);
			if (MaxContentHeight > 0.0f)
			{
				ContentSizeBox->SetHeightOverride(MaxContentHeight);
			}
			else
			{
				ContentSizeBox->ClearHeightOverride();
			}
			ContentSizeBox->ClearMinDesiredWidth();
			ContentSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			ContentSizeBox->SetRenderOpacity(1.0f);
		}
		else
		{
			SetContentSlotFill(false);
			ContentSizeBox->SetHeightOverride(0.0f);
			ContentSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
			ContentSizeBox->SetRenderOpacity(0.0f);
		}

		UpdateParentSlotSize(AnimationTarget);
	}
}

void URammsCollapsibleContainer::SetHeaderTitle(FText Title)
{
	HeaderTitle = Title;
	if (HeaderLabel)
	{
		HeaderLabel->SetText(HeaderTitle);
		HeaderLabel->SetVisibility(HeaderTitle.IsEmptyOrWhitespace()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::SelfHitTestInvisible);
	}
}

void URammsCollapsibleContainer::UpdateToggleIcon()
{
	if (!ToggleIcon)
		return;

	FString IconStr;
	switch (CollapseAnimation)
	{
		case ERammsCollapseAnimation::SlideLeft:
			IconStr = bIsExpanded ? TEXT("\u25C0") : TEXT("\u25B6");
			break;
		case ERammsCollapseAnimation::SlideRight:
			IconStr = bIsExpanded ? TEXT("\u25B6") : TEXT("\u25C0");
			break;
		default:
			IconStr = bIsExpanded ? TEXT("\u25B2") : TEXT("\u25BC");
			break;
	}
	ToggleIcon->SetText(FText::FromString(IconStr));
}

void URammsCollapsibleContainer::UpdateHeaderCornerRadii()
{
	if (!HeaderBorder)
		return;

	float OuterRadius = Style ? Style->Border.CornerRadiusMedium : 4.0f;
	float BorderW = Style ? Style->Border.BorderWidth : 1.0f;
	float R = FMath::Max(OuterRadius - BorderW, 0.0f);

	// When collapsed, header IS the whole widget — needs all corners
	// When expanded, header is on top — only top corners
	FVector4 Radii;
	if (bIsExpanded)
		Radii = FVector4(R, R, 0.0f, 0.0f);
	else
		Radii = FVector4(R, R, R, R);

	FLinearColor HeaderBg = Style ? Style->Colors.Surface : FLinearColor(0.1f, 0.1f, 0.12f, 1.0f);
	HeaderBg.A = 1.0f;
	FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrushEx(HeaderBg, Radii);
	URammsUIStyle::ApplyRoundedBrushToBorder(HeaderBorder, Brush);
}

void URammsCollapsibleContainer::ApplyAnimationState(float Alpha)
{
	if (!ContentSizeBox)
		return;

	float TargetHeight = GetEffectiveMaxHeight();
	ContentSizeBox->SetHeightOverride(TargetHeight * Alpha);
	ContentSizeBox->SetRenderOpacity(Alpha);
}

void URammsCollapsibleContainer::UpdateParentSlotSize(float Alpha)
{
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
	if (!CanvasSlot || CachedExpandedSlotSize.Y <= 0.0f)
		return;

	float HeaderH = 30.0f;
	if (HeaderBorder)
	{
		FVector2D HeaderSize = HeaderBorder->GetDesiredSize();
		if (HeaderSize.Y > 0.0f)
		{
			HeaderH = HeaderSize.Y;
		}
	}

	// Account for container border padding (top + bottom)
	float BorderPad = ContainerBorder ? ContainerBorder->GetPadding().GetTotalSpaceAlong<Orient_Vertical>() : 2.0f;
	float MinHeight = HeaderH + BorderPad;

	float ContentH = CachedExpandedSlotSize.Y - MinHeight;
	float NewHeight = FMath::Max(MinHeight + ContentH * Alpha, MinHeight);
	CanvasSlot->SetSize(FVector2D(CachedExpandedSlotSize.X, NewHeight));
}

void URammsCollapsibleContainer::SetContentSlotFill(bool bFill)
{
	if (!ContentSizeBox)
		return;

	if (UVerticalBoxSlot* VBSlot = Cast<UVerticalBoxSlot>(ContentSizeBox->Slot))
	{
		VBSlot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
	}
}

float URammsCollapsibleContainer::GetEffectiveMaxHeight() const
{
	if (MaxContentHeight > 0.0f)
	{
		return MaxContentHeight;
	}
	return (ExpandedContentHeight > 0.0f) ? ExpandedContentHeight : 200.0f;
}

void URammsCollapsibleContainer::SetMaxContentHeight(float Height)
{
	MaxContentHeight = FMath::Max(0.0f, Height);

	if (bIsExpanded && !bIsAnimating && ContentSizeBox)
	{
		if (MaxContentHeight > 0.0f)
		{
			ContentSizeBox->SetHeightOverride(MaxContentHeight);
		}
		else
		{
			ContentSizeBox->ClearHeightOverride();
		}
	}
}

void URammsCollapsibleContainer::OnToggleClicked()
{
	ToggleExpand();
}