// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsStatusPanel.h"
#include "UI/RammsUIStyle.h"
#include "RammsUISubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBoxSlot.h"
#include "Components/ButtonSlot.h"

// ── Constructor ────────────────────────────────────────────────────────

URammsStatusPanel::URammsStatusPanel()
{
	// Default fields — backward-compatible with original fixed layout
	{
		FRammsStatusField F;
		F.Key = TEXT("Speed");
		F.Label = FText::FromString(TEXT("Speed"));
		F.Source = ERammsStatusFieldSource::RobotState;
		F.Format = ERammsStatusValueFormat::Float;
		F.Units = FText::FromString(TEXT("m/s"));
		F.DecimalPlaces = 1;
		Fields.Add(MoveTemp(F));
	}
	{
		FRammsStatusField F;
		F.Key = TEXT("Battery");
		F.Label = FText::FromString(TEXT("Battery"));
		F.Source = ERammsStatusFieldSource::RobotState;
		F.Format = ERammsStatusValueFormat::Percent;
		F.bUseThresholdColors = true;
		F.WarningThreshold = 0.5f;
		F.CriticalThreshold = 0.2f;
		Fields.Add(MoveTemp(F));
	}
	{
		FRammsStatusField F;
		F.Key = TEXT("Mode");
		F.Label = FText::FromString(TEXT("Mode"));
		F.Source = ERammsStatusFieldSource::RobotState;
		F.Format = ERammsStatusValueFormat::EnumName;
		F.EnumType = StaticEnum<ERammsRobotMode>();
		Fields.Add(MoveTemp(F));
	}
}

// ── Widget Tree ────────────────────────────────────────────────────────

void URammsStatusPanel::ResetCachedWidgets()
{
	PanelBorder = nullptr;
	HeaderBorder = nullptr;
	ToggleButton = nullptr;
	HeaderText = nullptr;
	ToggleIcon = nullptr;
	HeaderRow = nullptr;
	ContentSizeBox = nullptr;
	ContentBox = nullptr;
	LabelWidgets.Empty();
	ValueWidgets.Empty();
}

void URammsStatusPanel::BuildWidgetTree()
{
	if (!WidgetTree || PanelBorder)
		return;

	// Root: outer border with rounded corners and clipping
	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(
		FLinearColor(0.06f, 0.06f, 0.08f, 0.92f), 4.0f, FLinearColor(0.3f, 0.3f, 0.3f, 1.0f), 1.0f);
	PanelBorder->SetPadding(FMargin(1.0f));
	PanelBorder->SetClipping(EWidgetClipping::ClipToBounds);
	WidgetTree->RootWidget = PanelBorder;

	UVerticalBox* MainBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainBox"));
	PanelBorder->AddChild(MainBox);

	// ── Header: styled border with title + toggle ──
	HeaderBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HeaderBorder"));
	HeaderBorder->Background = URammsUIStyle::MakeRoundedBoxBrushEx(
		FLinearColor(0.1f, 0.1f, 0.12f, 1.0f), FVector4(3.0f, 3.0f, 0.0f, 0.0f));
	HeaderBorder->SetPadding(FMargin(8.0f, 6.0f));
	if (UVerticalBoxSlot* HeaderSlot = MainBox->AddChildToVerticalBox(HeaderBorder))
	{
		HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		HeaderSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderRow"));
	HeaderBorder->AddChild(HeaderRow);

	HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HeaderText"));
	HeaderText->SetText(HeaderTitle);
	HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	if (UHorizontalBoxSlot* LabelSlot = HeaderRow->AddChildToHorizontalBox(HeaderText))
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Toggle button — transparent background, matching CollapsibleContainer style
	ToggleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ToggleButton"));
	ToggleButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	if (UHorizontalBoxSlot* BtnSlot = HeaderRow->AddChildToHorizontalBox(ToggleButton))
	{
		BtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		BtnSlot->SetVerticalAlignment(VAlign_Center);
		BtnSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
	}

	ToggleIcon = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ToggleIcon"));
	ToggleIcon->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ToggleButton->AddChild(ToggleIcon);

	// ── Content area: SizeBox → VBox (for animated collapse) ──
	ContentSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ContentSizeBox"));
	ContentSizeBox->SetClipping(EWidgetClipping::ClipToBounds);
	if (UVerticalBoxSlot* SizeSlot = MainBox->AddChildToVerticalBox(ContentSizeBox))
	{
		SizeSlot->SetHorizontalAlignment(HAlign_Fill);
		SizeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
	ContentSizeBox->AddChild(ContentBox);

	// Initial collapsed state
	if (!bIsExpanded)
	{
		ContentSizeBox->SetHeightOverride(0.0f);
		ContentSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		ContentSizeBox->SetRenderOpacity(0.0f);
		AnimationProgress = 0.0f;
		AnimationTarget = 0.0f;
	}

	BuildFieldRows();
}

void URammsStatusPanel::BuildFieldRows()
{
	if (!ContentBox || !WidgetTree)
		return;

	LabelWidgets.Empty();
	ValueWidgets.Empty();

	float RowPadding = Style ? Style->Spacing.XSmall : 2.0f;
	float RowInternalPad = Style ? Style->Spacing.Small : 4.0f;

	for (int32 i = 0; i < Fields.Num(); ++i)
	{
		// Separator line between rows (not before the first)
		if (i > 0)
		{
			UBorder* Separator = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), *FString::Printf(TEXT("Separator_%d"), i));
			Separator->SetBrushColor(Style ? Style->Colors.Border : FLinearColor(0.3f, 0.3f, 0.3f, 0.3f));
			Separator->SetDesiredSizeScale(FVector2D(1.0f, 1.0f));
			if (UVerticalBoxSlot* SepSlot = ContentBox->AddChildToVerticalBox(Separator))
			{
				SepSlot->SetHorizontalAlignment(HAlign_Fill);
				SepSlot->SetPadding(FMargin(RowInternalPad, 0.0f, RowInternalPad, 0.0f));
			}
			// Use a SizeBox to enforce separator height (2px avoids sub-pixel snapping at non-integer DPI)
			USizeBox* SepSize = WidgetTree->ConstructWidget<USizeBox>(
				USizeBox::StaticClass(), *FString::Printf(TEXT("SepSize_%d"), i));
			SepSize->SetHeightOverride(2.0f);
			// Re-parent: remove separator from ContentBox and put in SizeBox
			ContentBox->RemoveChild(Separator);
			SepSize->AddChild(Separator);
			if (UVerticalBoxSlot* SepSizeSlot = ContentBox->AddChildToVerticalBox(SepSize))
			{
				SepSizeSlot->SetHorizontalAlignment(HAlign_Fill);
				SepSizeSlot->SetPadding(FMargin(RowInternalPad, 0.0f, RowInternalPad, 0.0f));
			}
		}

		// Table row: HBox with fixed-width label + fill value
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), *FString::Printf(TEXT("FieldRow_%d"), i));
		if (UVerticalBoxSlot* RowSlot = ContentBox->AddChildToVerticalBox(Row))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
			RowSlot->SetPadding(FMargin(RowInternalPad, RowPadding, RowInternalPad, RowPadding));
		}

		// Label column
		UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("FieldLabel_%d"), i));
		LabelText->SetText(Fields[i].Label);
		LabelText->SetColorAndOpacity(FSlateColor(
			Style ? Style->Colors.TextSecondary : FLinearColor(0.7f, 0.7f, 0.7f)));
		if (UHorizontalBoxSlot* LblSlot = Row->AddChildToHorizontalBox(LabelText))
		{
			if (LabelColumnWidth > 0.0f)
			{
				LblSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			}
			else
			{
				LblSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			}
			LblSlot->SetVerticalAlignment(VAlign_Center);
			LblSlot->SetPadding(FMargin(0.0f, 0.0f, RowInternalPad, 0.0f));
		}

		// If fixed-width label, wrap in a SizeBox
		if (LabelColumnWidth > 0.0f)
		{
			// We need to re-parent the label into a SizeBox for fixed width
			Row->RemoveChild(LabelText);
			USizeBox* LabelSizeBox = WidgetTree->ConstructWidget<USizeBox>(
				USizeBox::StaticClass(), *FString::Printf(TEXT("LabelSize_%d"), i));
			LabelSizeBox->SetWidthOverride(LabelColumnWidth);
			LabelSizeBox->AddChild(LabelText);
			if (UHorizontalBoxSlot* SzSlot = Row->AddChildToHorizontalBox(LabelSizeBox))
			{
				SzSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				SzSlot->SetVerticalAlignment(VAlign_Center);
				SzSlot->SetPadding(FMargin(0.0f, 0.0f, RowInternalPad, 0.0f));
			}
		}

		// Value column (fills remaining space)
		UTextBlock* ValueText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("FieldValue_%d"), i));
		ValueText->SetText(FText::FromString(TEXT("--")));
		ValueText->SetColorAndOpacity(FSlateColor(
			Style ? Style->Colors.TextPrimary : FLinearColor::White));
		ValueText->SetAutoWrapText(Fields[i].bWrapText);
		if (UHorizontalBoxSlot* ValSlot = Row->AddChildToHorizontalBox(ValueText))
		{
			ValSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ValSlot->SetVerticalAlignment(VAlign_Center);
		}

		LabelWidgets.Add(LabelText);
		ValueWidgets.Add(ValueText);
	}
}

// ── Lifecycle ──────────────────────────────────────────────────────────

void URammsStatusPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsStatusPanel::NativePreConstruct()
{
	Super::NativePreConstruct();
	BuildWidgetTree();

	if (!IsDesignTime())
		return;

	// Designer preview state
	if (ContentSizeBox)
	{
		if (bIsExpanded)
		{
			ContentSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			ContentSizeBox->ClearHeightOverride();
			ContentSizeBox->SetRenderOpacity(1.0f);
		}
		else
		{
			ContentSizeBox->SetHeightOverride(0.0f);
			ContentSizeBox->SetVisibility(ESlateVisibility::Hidden);
			ContentSizeBox->SetRenderOpacity(0.0f);
		}
	}
	UpdateToggleIcon();
}

void URammsStatusPanel::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (HeaderText)
	{
		HeaderText->SetText(HeaderTitle);
		HeaderText->SetVisibility(HeaderTitle.IsEmptyOrWhitespace()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::SelfHitTestInvisible);
	}

	// Rebuild rows if field count changed (designer editing)
	if (ContentBox && WidgetTree && LabelWidgets.Num() != Fields.Num())
	{
		ContentBox->ClearChildren();
		BuildFieldRows();
		ApplyStyle_Implementation();
	}

	UpdateToggleIcon();
	RefreshAllFields();
}

void URammsStatusPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (ToggleButton)
	{
		ToggleButton->OnClicked.AddUniqueDynamic(this, &URammsStatusPanel::OnToggleClicked);
	}

	// State provider subscription
	if (StateProvider.GetInterface())
	{
		StateUpdateHandle = StateProvider.GetInterface()->OnRobotStateUpdate().AddUObject(
			this, &URammsStatusPanel::OnRobotStateUpdate);
	}

	// Subsystem subscriptions
	if (UWorld* World = GetWorld())
	{
		if (URammsUISubsystem* Sub = World->GetSubsystem<URammsUISubsystem>())
		{
			Sub->OnRobotStateChanged.AddUniqueDynamic(this, &URammsStatusPanel::OnSubsystemStateChanged);
			Sub->OnPropertyChanged.AddUniqueDynamic(this, &URammsStatusPanel::OnSubsystemPropertyChanged);

			if (Sub->HasRobotState())
			{
				ApplyRemoteState(Sub->GetCachedRobotState());
			}
		}
	}

	UpdateToggleIcon();

	// Apply initial state
	if (ContentSizeBox)
	{
		if (bIsExpanded)
		{
			ContentSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			ContentSizeBox->ClearHeightOverride();
			ContentSizeBox->SetRenderOpacity(1.0f);
		}
		else
		{
			ContentSizeBox->SetHeightOverride(0.0f);
			ContentSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
			ContentSizeBox->SetRenderOpacity(0.0f);
		}
	}
}

void URammsStatusPanel::NativeDestruct()
{
	if (StateProvider.GetInterface() && StateUpdateHandle.IsValid())
	{
		StateProvider.GetInterface()->OnRobotStateUpdate().Remove(StateUpdateHandle);
		StateUpdateHandle.Reset();
	}

	Super::NativeDestruct();
}

void URammsStatusPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Periodic data refresh
	TimeSinceUpdate += InDeltaTime;
	if (TimeSinceUpdate >= UpdateFrequency)
	{
		TimeSinceUpdate = 0.0f;
		UpdateDisplay();
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

	if (FMath::IsNearlyEqual(AnimationProgress, AnimationTarget, 0.001f))
	{
		AnimationProgress = AnimationTarget;
		bIsAnimating = false;

		if (ContentSizeBox)
		{
			if (AnimationTarget >= 1.0f)
			{
				ContentSizeBox->ClearHeightOverride();
				ContentSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				ContentSizeBox->SetRenderOpacity(1.0f);
			}
			else
			{
				ContentSizeBox->SetHeightOverride(0.0f);
				ContentSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
				ContentSizeBox->SetRenderOpacity(0.0f);
			}
		}
	}
}

// ── Styling ────────────────────────────────────────────────────────────

void URammsStatusPanel::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	float Radius = Style->Border.CornerRadiusMedium;
	float BorderW = Style->Border.BorderWidth;

	// Outer panel border
	if (PanelBorder)
	{
		FLinearColor Bg = Style->Colors.Background;
		Bg.A = 0.92f;
		if (bShowBorder)
		{
			FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrush(Bg, Radius, Style->Colors.Border, BorderW);
			URammsUIStyle::ApplyRoundedBrushToBorder(PanelBorder, Brush);
		}
		else
		{
			FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrush(FLinearColor::Transparent, Radius);
			URammsUIStyle::ApplyRoundedBrushToBorder(PanelBorder, Brush);
		}
		PanelBorder->SetPadding(FMargin(BorderW));
	}

	// Header border
	if (HeaderBorder)
	{
		UpdateHeaderCornerRadii();
	}

	// Header text
	if (HeaderText)
	{
		HeaderText->SetFont(Style->Typography.HeadingSmall);
		HeaderText->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}

	// Toggle icon styling to match CollapsibleContainer
	if (ToggleIcon)
	{
		ToggleIcon->SetFont(Style->Typography.Body);
		ToggleIcon->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
	}

	// Animation duration from style
	if (Style->ExpandCollapseCurve.Duration > 0.0f)
	{
		AnimationDuration = Style->ExpandCollapseCurve.Duration;
	}

	// Field label + value typography
	for (UTextBlock* W : LabelWidgets)
	{
		if (W)
		{
			W->SetFont(Style->Typography.Body);
			W->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
		}
	}

	for (UTextBlock* W : ValueWidgets)
	{
		if (W)
		{
			W->SetFont(Style->Typography.Monospace);
		}
	}
}

// ── State Provider ───────────────────────────────────────────────────────

void URammsStatusPanel::SetStateProvider(TScriptInterface<IRammsStateProvider> Provider)
{
	if (StateProvider.GetInterface() && StateUpdateHandle.IsValid())
	{
		StateProvider.GetInterface()->OnRobotStateUpdate().Remove(StateUpdateHandle);
		StateUpdateHandle.Reset();
	}

	StateProvider = Provider;

	if (StateProvider.GetInterface())
	{
		StateUpdateHandle = StateProvider.GetInterface()->OnRobotStateUpdate().AddUObject(
			this, &URammsStatusPanel::OnRobotStateUpdate);
	}

	UpdateDisplay();
}

// ── Expand / Collapse ────────────────────────────────────────────────────

void URammsStatusPanel::ToggleExpand()
{
	SetExpanded(!bIsExpanded, true);
}

void URammsStatusPanel::SetExpanded(bool bExpanded, bool bAnimated)
{
	if (bIsExpanded == bExpanded && !bIsAnimating)
		return;

	bIsExpanded = bExpanded;
	UpdateToggleIcon();
	UpdateHeaderCornerRadii();

	if (!ContentSizeBox)
	{
		if (ContentBox)
		{
			ContentBox->SetVisibility(bIsExpanded
					? ESlateVisibility::SelfHitTestInvisible
					: ESlateVisibility::Collapsed);
		}
		return;
	}

	AnimationTarget = bIsExpanded ? 1.0f : 0.0f;

	// Cache sizes before collapsing
	if (!bIsExpanded)
	{
		float ActualH = ContentSizeBox->GetCachedGeometry().GetLocalSize().Y;
		if (ActualH > 0.0f)
		{
			ExpandedContentHeight = ActualH;
		}
		else if (ContentBox)
		{
			FVector2D DesiredSize = ContentBox->GetDesiredSize();
			if (DesiredSize.Y > 0.0f)
			{
				ExpandedContentHeight = DesiredSize.Y + 16.0f;
			}
		}
	}
	else
	{
		bNeedsCacheHeight = true;
	}

	if (bAnimated && AnimationDuration > 0.0f && ExpandedContentHeight > 0.0f)
	{
		bIsAnimating = true;

		// Start from current position for smooth direction reversal
		if (bIsExpanded)
		{
			ContentSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}
	else
	{
		// Instant
		AnimationProgress = AnimationTarget;
		bIsAnimating = false;
		if (bIsExpanded)
		{
			ContentSizeBox->ClearHeightOverride();
			ContentSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			ContentSizeBox->SetRenderOpacity(1.0f);
		}
		else
		{
			ContentSizeBox->SetHeightOverride(0.0f);
			ContentSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
			ContentSizeBox->SetRenderOpacity(0.0f);
		}
	}
}

void URammsStatusPanel::UpdateToggleIcon()
{
	if (!ToggleIcon)
		return;

	// ▼ when expanded, ▶ when collapsed (matching CollapsibleContainer)
	ToggleIcon->SetText(FText::FromString(bIsExpanded ? TEXT("\u25BC") : TEXT("\u25B6")));
}

void URammsStatusPanel::UpdateHeaderCornerRadii()
{
	if (!HeaderBorder || !Style)
		return;

	float Radius = FMath::Max(Style->Border.CornerRadiusMedium - Style->Border.BorderWidth, 0.0f);

	FVector4 HeaderRadii;
	if (bIsExpanded)
	{
		// Expanded: round top corners only
		HeaderRadii = FVector4(Radius, Radius, 0.0f, 0.0f);
	}
	else
	{
		// Collapsed: round all corners
		HeaderRadii = FVector4(Radius, Radius, Radius, Radius);
	}

	FLinearColor HeaderBg = Style->Colors.Surface;
	HeaderBg.A = 1.0f;
	FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrushEx(HeaderBg, HeaderRadii);
	URammsUIStyle::ApplyRoundedBrushToBorder(HeaderBorder, Brush);
	HeaderBorder->SetPadding(FMargin(Style->Spacing.Medium, Style->Spacing.Small));
}

void URammsStatusPanel::ApplyAnimationState(float Alpha)
{
	if (!ContentSizeBox || ExpandedContentHeight <= 0.0f)
		return;

	float Height = ExpandedContentHeight * Alpha;
	ContentSizeBox->SetHeightOverride(Height);
	ContentSizeBox->SetRenderOpacity(Alpha);
}

// ── Display Update ───────────────────────────────────────────────────────

void URammsStatusPanel::UpdateDisplay()
{
	IRammsStateProvider* Provider = StateProvider.GetInterface();
	if (Provider)
	{
		FRammsRobotState State;
		if (Provider->GetRobotState(State))
		{
			CachedState = State;
			bHasRemoteState = true;
		}
		else
		{
			// Provider exists but returned no valid data — mark stale so
			// RefreshAllFields can show placeholder / disabled values.
			bHasRemoteState = false;
		}
	}

	RefreshAllFields();
}

void URammsStatusPanel::ApplyRemoteState(const FRammsRobotState& State)
{
	bHasRemoteState = true;
	CachedState = State;
	RefreshAllFields();
}

void URammsStatusPanel::SetCustomFieldValue(FName Key, const FString& Value)
{
	CustomFieldValues.Add(Key, Value);
	RefreshAllFields();
}

void URammsStatusPanel::OnRobotStateUpdate(const FRammsRobotState& State)
{
	CachedState = State;
	bHasRemoteState = true;
	RefreshAllFields();
}

void URammsStatusPanel::OnSubsystemStateChanged(const FRammsRobotState& State)
{
	bHasRemoteState = true;
	CachedState = State;
	RefreshAllFields();
}

void URammsStatusPanel::OnSubsystemPropertyChanged(FName Key, const FString& Value)
{
	for (const FRammsStatusField& Field : Fields)
	{
		if (Field.Source == ERammsStatusFieldSource::PropertyStore && Field.Key == Key)
		{
			RefreshAllFields();
			return;
		}
	}
}

void URammsStatusPanel::OnToggleClicked()
{
	ToggleExpand();
}

// ── Field Resolution & Formatting ────────────────────────────────────────────

void URammsStatusPanel::RefreshAllFields()
{
	if (LabelWidgets.Num() != Fields.Num() || ValueWidgets.Num() != Fields.Num())
		return;

	URammsUISubsystem* Sub = nullptr;
	if (UWorld* World = GetWorld())
	{
		Sub = World->GetSubsystem<URammsUISubsystem>();
	}

	static const FName SpeedKey(TEXT("Speed"));
	static const FName BatteryKey(TEXT("Battery"));
	static const FName ModeKey(TEXT("Mode"));
	static const FName EmergencyStopKey(TEXT("EmergencyStop"));
	static const FName LinearVelocityKey(TEXT("LinearVelocity"));
	static const FName AngularVelocityKey(TEXT("AngularVelocity"));

	const FLinearColor DefaultColor = Style ? Style->Colors.TextPrimary : FLinearColor::White;
	const FLinearColor DisabledColor = Style ? Style->Colors.TextDisabled : FLinearColor::Gray;

	for (int32 i = 0; i < Fields.Num(); ++i)
	{
		const FRammsStatusField& Field = Fields[i];
		UTextBlock*				 ValueWidget = ValueWidgets[i];
		if (!ValueWidget)
			continue;

		// ── Resolve raw value ────────────────────────────────────────────
		float	NumericValue = 0.0f;
		FString StringValue;
		bool	bIsNumeric = false;
		bool	bIsValid = false;

		switch (Field.Source)
		{
			case ERammsStatusFieldSource::RobotState:
			{
				if (!bHasRemoteState)
					break;

				bIsValid = true;
				if (Field.Key == SpeedKey)
				{
					NumericValue = CachedState.LinearVelocity.Size();
					bIsNumeric = true;
				}
				else if (Field.Key == BatteryKey)
				{
					NumericValue = CachedState.BatteryLevel;
					bIsNumeric = true;
				}
				else if (Field.Key == ModeKey)
				{
					NumericValue = static_cast<float>(static_cast<uint8>(CachedState.Mode));
					bIsNumeric = true;
				}
				else if (Field.Key == EmergencyStopKey)
				{
					NumericValue = CachedState.bEmergencyStop ? 1.0f : 0.0f;
					StringValue = CachedState.bEmergencyStop ? TEXT("ACTIVE") : TEXT("Inactive");
					bIsNumeric = true;
				}
				else if (Field.Key == LinearVelocityKey)
				{
					StringValue = CachedState.LinearVelocity.ToString();
				}
				else if (Field.Key == AngularVelocityKey)
				{
					StringValue = CachedState.AngularVelocity.ToString();
				}
				else
				{
					bIsValid = false;
				}
				break;
			}
			case ERammsStatusFieldSource::PropertyStore:
			{
				if (Sub && Sub->HasProperty(Field.Key))
				{
					StringValue = Sub->GetProperty(Field.Key);
					if (StringValue.IsNumeric())
					{
						NumericValue = FCString::Atof(*StringValue);
						bIsNumeric = true;
					}
					bIsValid = true;
				}
				break;
			}
			case ERammsStatusFieldSource::Custom:
			{
				if (const FString* Val = CustomFieldValues.Find(Field.Key))
				{
					StringValue = *Val;
					if (StringValue.IsNumeric())
					{
						NumericValue = FCString::Atof(*StringValue);
						bIsNumeric = true;
					}
					bIsValid = true;
				}
				break;
			}
		}

		// ── Format value ───────────────────────────────────────────────
		FText FormattedValue;
		if (!bIsValid)
		{
			FormattedValue = FText::FromString(TEXT("--"));
		}
		else
		{
			switch (Field.Format)
			{
				case ERammsStatusValueFormat::Float:
				{
					FNumberFormattingOptions Opts = FNumberFormattingOptions::DefaultNoGrouping();
					Opts.MinimumFractionalDigits = Field.DecimalPlaces;
					Opts.MaximumFractionalDigits = Field.DecimalPlaces;
					FText Num = FText::AsNumber(bIsNumeric ? NumericValue : 0.0f, &Opts);
					FormattedValue = Field.Units.IsEmpty()
						? Num
						: FText::Format(FText::FromString(TEXT("{0} {1}")), Num, Field.Units);
					break;
				}
				case ERammsStatusValueFormat::Integer:
				{
					FText Num = FText::AsNumber(FMath::RoundToInt(bIsNumeric ? NumericValue : 0.0f));
					FormattedValue = Field.Units.IsEmpty()
						? Num
						: FText::Format(FText::FromString(TEXT("{0} {1}")), Num, Field.Units);
					break;
				}
				case ERammsStatusValueFormat::Percent:
				{
					int32 Pct = FMath::RoundToInt((bIsNumeric ? NumericValue : 0.0f) * 100.0f);
					FormattedValue = FText::Format(
						FText::FromString(TEXT("{0}%")),
						FText::AsNumber(Pct));
					break;
				}
				case ERammsStatusValueFormat::Boolean:
				{
					if (bIsNumeric)
					{
						StringValue = (NumericValue > 0.5f) ? TEXT("Yes") : TEXT("No");
					}
					else
					{
						StringValue.TrimStartAndEndInline();
						StringValue.ToLowerInline();
						bool bVal = StringValue == TEXT("true") || StringValue == TEXT("yes") || StringValue == TEXT("on") || StringValue == TEXT("1");
						StringValue = bVal ? TEXT("Yes") : TEXT("No");
					}
					break;
				}
				case ERammsStatusValueFormat::EnumName:
				{
					if (Field.EnumType && bIsNumeric)
					{
						int64 IntVal = static_cast<int64>(FMath::RoundToInt(NumericValue));
						FormattedValue = Field.EnumType->GetDisplayNameTextByValue(IntVal);
					}
					else
					{
						FormattedValue = FText::FromString(StringValue.IsEmpty() ? TEXT("--") : StringValue);
					}
					break;
				}
				case ERammsStatusValueFormat::RawText:
					FormattedValue = FText::FromString(StringValue);
					break;
				case ERammsStatusValueFormat::Auto:
				default:
				{
					if (bIsNumeric)
					{
						FNumberFormattingOptions Opts = FNumberFormattingOptions::DefaultNoGrouping();
						Opts.MinimumFractionalDigits = Field.DecimalPlaces;
						Opts.MaximumFractionalDigits = Field.DecimalPlaces;
						FText Num = FText::AsNumber(NumericValue, &Opts);
						FormattedValue = Field.Units.IsEmpty()
							? Num
							: FText::Format(FText::FromString(TEXT("{0} {1}")), Num, Field.Units);
					}
					else
					{
						FormattedValue = FText::FromString(StringValue.IsEmpty() ? TEXT("--") : StringValue);
					}
					break;
				}
			}
		}

		// Set value text (label is set once in BuildFieldRows and doesn't change)
		ValueWidget->SetText(FormattedValue);

		// ── Determine color ──────────────────────────────────────────────
		FLinearColor Color = DefaultColor;

		if (!bIsValid)
		{
			Color = DisabledColor;
		}
		else if (Field.Key == ModeKey && Field.Source == ERammsStatusFieldSource::RobotState)
		{
			Color = GetModeColor(CachedState.Mode);
		}
		else if (Field.bUseThresholdColors && bIsNumeric)
		{
			if (NumericValue <= Field.CriticalThreshold)
				Color = Style ? Style->Colors.Error : FLinearColor::Red;
			else if (NumericValue <= Field.WarningThreshold)
				Color = Style ? Style->Colors.Warning : FLinearColor::Yellow;
			else
				Color = Style ? Style->Colors.Success : FLinearColor::Green;
		}

		ValueWidget->SetColorAndOpacity(FSlateColor(Color));
	}
}

// ── Color Helpers ──────────────────────────────────────────────────────────

FLinearColor URammsStatusPanel::GetBatteryColor(float BatteryLevel) const
{
	if (!Style)
		return FLinearColor::White;

	if (BatteryLevel > 0.5f)
		return Style->Colors.Success;
	else if (BatteryLevel > 0.2f)
		return Style->Colors.Warning;
	else
		return Style->Colors.Error;
}

FLinearColor URammsStatusPanel::GetModeColor(ERammsRobotMode Mode) const
{
	if (!Style)
		return FLinearColor::White;

	switch (Mode)
	{
		case ERammsRobotMode::Standby:
			return Style->Colors.TextSecondary;
		case ERammsRobotMode::Manual:
			return Style->Colors.Info;
		case ERammsRobotMode::Autonomous:
			return Style->Colors.Success;
		case ERammsRobotMode::Emergency:
			return Style->Colors.Error;
		default:
			return Style->Colors.TextDisabled;
	}
}
