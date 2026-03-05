// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsNotificationWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"

void URammsNotificationWidget::BuildWidgetTree()
{
	if (!WidgetTree || NotificationBorder)
		return;

	// Root: rounded outer border that clips children to the rounded shape
	NotificationBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NotificationBorder"));
	NotificationBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(
		FLinearColor(0.12f, 0.12f, 0.15f, 0.95f), 6.0f);
	NotificationBorder->SetPadding(FMargin(0.0f));
	NotificationBorder->SetClipping(EWidgetClipping::ClipToBounds);
	WidgetTree->RootWidget = NotificationBorder;

	// HorizontalBox: accent bar | text content (side by side inside the rounded border)
	UHorizontalBox* ContentRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ContentRow"));
	NotificationBorder->AddChild(ContentRow);

	// Accent color bar on the left — rounded left corners to match outer border
	AccentBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AccentBar"));
	AccentBar->Background = URammsUIStyle::MakeRoundedBoxBrushEx(
		GetAccentColor(), FVector4(6.0f, 0.0f, 0.0f, 6.0f));
	UHorizontalBoxSlot* AccentSlot = ContentRow->AddChildToHorizontalBox(AccentBar);
	if (AccentSlot)
	{
		AccentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		AccentSlot->SetPadding(FMargin(0));
	}

	// Spacer inside accent bar to give it width
	UImage* AccentSpacer = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("AccentSpacer"));
	AccentSpacer->SetColorAndOpacity(FLinearColor::Transparent);
	AccentSpacer->SetDesiredSizeOverride(FVector2D(4.0f, 1.0f));
	AccentBar->AddChild(AccentSpacer);

	// Text content area with padding
	UBorder* TextPadding = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TextPadding"));
	TextPadding->SetBrushColor(FLinearColor::Transparent);
	TextPadding->SetPadding(FMargin(10.0f, 8.0f, 12.0f, 8.0f));
	UHorizontalBoxSlot* TextSlot = ContentRow->AddChildToHorizontalBox(TextPadding);
	if (TextSlot)
	{
		TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// Vertical layout for title + message
	UVerticalBox* TextLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TextLayout"));
	TextPadding->AddChild(TextLayout);

	// Title (optional, hidden by default)
	TitleLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleLabel"));
	TitleLabel->SetText(Title);
	TitleLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	TitleLabel->SetVisibility(Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	UVerticalBoxSlot* TitleSlot = TextLayout->AddChildToVerticalBox(TitleLabel);
	if (TitleSlot) TitleSlot->SetPadding(FMargin(0, 0, 0, 2));

	// Message
	MessageLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MessageLabel"));
	MessageLabel->SetText(Message);
	MessageLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)));
	MessageLabel->SetAutoWrapText(true);
	TextLayout->AddChildToVerticalBox(MessageLabel);
}

void URammsNotificationWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsNotificationWidget::NativeConstruct()
{
	SetIsFocusable(true);
	Super::NativeConstruct();

	TimeRemaining = Duration;

	// Update labels from member properties (may have been set via SetNotificationContent)
	if (MessageLabel)
		MessageLabel->SetText(Message);

	if (TitleLabel)
	{
		TitleLabel->SetText(Title);
		TitleLabel->SetVisibility(Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (AccentBar)
	{
		float Radius = Style ? Style->Border.CornerRadiusMedium : 6.0f;
		FVector4 LeftRadii(Radius, 0.0f, 0.0f, Radius);
		FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrushEx(GetAccentColor(), LeftRadii);
		URammsUIStyle::ApplyRoundedBrushToBorder(AccentBar, Brush);
	}

	// Slide in from right
	SlideIn(FVector2D(300, 0), 0.3f, ERammsUIEasing::EaseOut);
}

void URammsNotificationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bDismissed || Duration <= 0.0f)
		return;

	TimeRemaining -= InDeltaTime;
	if (TimeRemaining <= 0.0f)
	{
		Dismiss();
	}
}

void URammsNotificationWidget::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	float Radius = Style->Border.CornerRadiusMedium;

	if (NotificationBorder)
	{
		FLinearColor Bg = Style->Colors.Surface;
		Bg.A = 0.95f;
		FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrush(Bg, Radius);
		URammsUIStyle::ApplyRoundedBrushToBorder(NotificationBorder, Brush);
	}

	if (AccentBar)
	{
		FVector4 LeftRadii(Radius, 0.0f, 0.0f, Radius);
		FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrushEx(GetAccentColor(), LeftRadii);
		URammsUIStyle::ApplyRoundedBrushToBorder(AccentBar, Brush);
	}

	if (TitleLabel)
	{
		TitleLabel->SetFont(Style->Typography.HeadingSmall);
		TitleLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}

	if (MessageLabel)
	{
		MessageLabel->SetFont(Style->Typography.Body);
		MessageLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
	}
}

void URammsNotificationWidget::Show(FText InMessage, ERammsNotificationLevel InLevel, float InDuration, FText InTitle)
{
	Message = InMessage;
	Level = InLevel;
	Duration = InDuration;
	Title = InTitle;
	TimeRemaining = Duration;
	bDismissed = false;

	if (MessageLabel)
		MessageLabel->SetText(Message);

	if (TitleLabel)
	{
		TitleLabel->SetText(Title);
		TitleLabel->SetVisibility(Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (AccentBar)
	{
		float Radius = Style ? Style->Border.CornerRadiusMedium : 6.0f;
		FVector4 LeftRadii(Radius, 0.0f, 0.0f, Radius);
		FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrushEx(GetAccentColor(), LeftRadii);
		URammsUIStyle::ApplyRoundedBrushToBorder(AccentBar, Brush);
	}

	SetVisibility(ESlateVisibility::Visible);
	SetRenderOpacity(1.0f);
	SlideIn(FVector2D(300, 0), 0.3f, ERammsUIEasing::EaseOut);
}

void URammsNotificationWidget::SetNotificationContent(FText InMessage, ERammsNotificationLevel InLevel, float InDuration, FText InTitle)
{
	// Set content properties before the widget is added to a parent.
	// NativeConstruct will handle the initial animation when it gets added.
	Message = InMessage;
	Level = InLevel;
	Duration = InDuration;
	Title = InTitle;
}

void URammsNotificationWidget::Dismiss()
{
	if (bDismissed)
		return;

	bDismissed = true;
	FadeOut(0.3f, ERammsUIEasing::EaseIn);
	OnDismissed.Broadcast();

	// Hide after fade completes (don't RemoveFromParent — allows reuse via Show())
	FTimerHandle TimerHandle;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimerHandle, [this]()
		{
			if (IsValid(this))
			{
				SetVisibility(ESlateVisibility::Collapsed);
				SetRenderOpacity(0.0f);
			}
		}, 0.35f, false);
	}
}

FReply URammsNotificationWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDismissOnClick && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		Dismiss();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FLinearColor URammsNotificationWidget::GetAccentColor() const
{
	if (Style)
	{
		switch (Level)
		{
		case ERammsNotificationLevel::Info:    return Style->Colors.Info;
		case ERammsNotificationLevel::Success: return Style->Colors.Success;
		case ERammsNotificationLevel::Warning: return Style->Colors.Warning;
		case ERammsNotificationLevel::Error:   return Style->Colors.Error;
		}
	}

	// Fallback colors without style
	switch (Level)
	{
	case ERammsNotificationLevel::Info:    return FLinearColor(0.2f, 0.5f, 1.0f);
	case ERammsNotificationLevel::Success: return FLinearColor(0.2f, 0.8f, 0.3f);
	case ERammsNotificationLevel::Warning: return FLinearColor(1.0f, 0.7f, 0.1f);
	case ERammsNotificationLevel::Error:   return FLinearColor(1.0f, 0.2f, 0.2f);
	}

	return FLinearColor::White;
}
