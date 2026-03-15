// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsPanel.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

void URammsPanel::ResetCachedWidgets()
{
	PanelBorder = nullptr;
	HeaderBorder = nullptr;
	HeaderLabel = nullptr;
	ContentVBox = nullptr;
	ContentBorder = nullptr;
	ContentSlot = nullptr;
}

void URammsPanel::BuildWidgetTree()
{
	if (!WidgetTree || PanelBorder)
		return;

	// Root: outer border with rounded corners + background
	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(
		FLinearColor(0.06f, 0.06f, 0.08f, 0.92f), 4.0f,
		FLinearColor(0.3f, 0.3f, 0.3f, 1.0f), 1.0f);
	PanelBorder->SetPadding(FMargin(1.0f));
	PanelBorder->SetClipping(EWidgetClipping::ClipToBounds);
	WidgetTree->RootWidget = PanelBorder;

	UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainVBox"));
	PanelBorder->AddChild(MainVBox);

	// Header (optional) — rounded top corners
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

	HeaderLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HeaderLabel"));
	HeaderLabel->SetText(HeaderText);
	HeaderLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	HeaderBorder->AddChild(HeaderLabel);

	// Content area: border (for padding) → VBox → NamedSlot
	ContentBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ContentBorder"));
	ContentBorder->SetBrushColor(FLinearColor::Transparent);
	ContentBorder->SetPadding(ContentPadding);
	UVerticalBoxSlot* ContentSlotVB = MainVBox->AddChildToVerticalBox(ContentBorder);
	if (ContentSlotVB)
	{
		ContentSlotVB->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ContentSlotVB->SetHorizontalAlignment(HAlign_Fill);
	}

	ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentVBox"));
	ContentBorder->AddChild(ContentVBox);

	ContentSlot = WidgetTree->ConstructWidget<UNamedSlot>(UNamedSlot::StaticClass(), TEXT("ContentSlot"));
	ContentVBox->AddChildToVerticalBox(ContentSlot);

	UpdateHeaderVisibility();
}

void URammsPanel::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (HeaderLabel)
	{
		HeaderLabel->SetText(HeaderText);
	}

	if (ContentBorder)
	{
		ContentBorder->SetPadding(ContentPadding);
	}

	UpdateHeaderVisibility();
}

void URammsPanel::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	float Radius = (CornerRadiusOverride >= 0.0f) ? CornerRadiusOverride : Style->Border.CornerRadiusMedium;
	float BorderW = Style->Border.BorderWidth;

	if (PanelBorder)
	{
		FLinearColor Bg = Style->Colors.Background;
		Bg.A = (BackgroundOpacity >= 0.0f) ? BackgroundOpacity : 0.92f;

		if (bShowBorder)
		{
			FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrush(Bg, Radius, Style->Colors.Border, BorderW);
			URammsUIStyle::ApplyRoundedBrushToBorder(PanelBorder, Brush);
		}
		else
		{
			FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrush(Bg, Radius);
			URammsUIStyle::ApplyRoundedBrushToBorder(PanelBorder, Brush);
		}
		PanelBorder->SetPadding(FMargin(BorderW));
	}

	if (HeaderBorder)
	{
		float R = FMath::Max(Radius - BorderW, 0.0f);
		FLinearColor HeaderBg = Style->Colors.Surface;
		HeaderBg.A = 1.0f;
		FSlateBrush Brush = URammsUIStyle::MakeRoundedBoxBrushEx(HeaderBg, FVector4(R, R, 0.0f, 0.0f));
		URammsUIStyle::ApplyRoundedBrushToBorder(HeaderBorder, Brush);
	}

	if (HeaderLabel)
	{
		HeaderLabel->SetFont(Style->Typography.HeadingSmall);
		HeaderLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}
}

void URammsPanel::AddContentChild(UWidget* Child)
{
	if (ContentVBox && Child)
	{
		ContentVBox->AddChildToVerticalBox(Child);
	}
}

void URammsPanel::SetHeaderText(FText Text)
{
	HeaderText = Text;
	if (HeaderLabel)
	{
		HeaderLabel->SetText(HeaderText);
	}
	UpdateHeaderVisibility();
}

void URammsPanel::SetShowBorder(bool bShow)
{
	bShowBorder = bShow;
	if (Style)
	{
		ApplyStyle();
	}
}

void URammsPanel::UpdateHeaderVisibility()
{
	if (!HeaderBorder)
		return;

	bool bVisible = bShowHeader && !HeaderText.IsEmpty();
	HeaderBorder->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}
