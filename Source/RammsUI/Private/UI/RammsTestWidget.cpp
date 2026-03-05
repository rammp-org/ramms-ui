// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsTestWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"

void URammsTestWidget::BuildWidgetTree()
{
	if (!WidgetTree || MainBorder)
		return;

	// Root: Border
	MainBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MainBorder"));
	MainBorder->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.15f, 0.95f));
	MainBorder->SetPadding(FMargin(16.0f));
	WidgetTree->RootWidget = MainBorder;

	// Vertical layout inside border
	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Layout"));
	MainBorder->AddChild(Layout);

	// Title
	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("RammsUI Test Panel")));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UVerticalBoxSlot* TitleSlot = Layout->AddChildToVerticalBox(TitleText);
	if (TitleSlot) TitleSlot->SetPadding(FMargin(0, 0, 0, 8));

	// Status
	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetText(FText::FromString(TEXT("Ready")));
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
	UVerticalBoxSlot* StatusSlot = Layout->AddChildToVerticalBox(StatusText);
	if (StatusSlot) StatusSlot->SetPadding(FMargin(0, 0, 0, 12));

	// Buttons
	FadeInButton = CreateTestButton(TEXT("FadeInButton"), TEXT("Fade In"));
	Layout->AddChildToVerticalBox(FadeInButton)->SetPadding(FMargin(0, 2));

	FadeOutButton = CreateTestButton(TEXT("FadeOutButton"), TEXT("Fade Out"));
	Layout->AddChildToVerticalBox(FadeOutButton)->SetPadding(FMargin(0, 2));

	SlideInButton = CreateTestButton(TEXT("SlideInButton"), TEXT("Slide In"));
	Layout->AddChildToVerticalBox(SlideInButton)->SetPadding(FMargin(0, 2));

	ScaleInButton = CreateTestButton(TEXT("ScaleInButton"), TEXT("Scale In"));
	Layout->AddChildToVerticalBox(ScaleInButton)->SetPadding(FMargin(0, 2));
}

UButton* URammsTestWidget::CreateTestButton(const FString& Name, const FString& Label)
{
	UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *Name);
	UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(Name + TEXT("Label")));
	BtnLabel->SetText(FText::FromString(Label));
	BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Btn->AddChild(BtnLabel);
	return Btn;
}

void URammsTestWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsTestWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (FadeInButton)
		FadeInButton->OnClicked.AddDynamic(this, &URammsTestWidget::OnFadeInClicked);
	if (FadeOutButton)
		FadeOutButton->OnClicked.AddDynamic(this, &URammsTestWidget::OnFadeOutClicked);
	if (SlideInButton)
		SlideInButton->OnClicked.AddDynamic(this, &URammsTestWidget::OnSlideInClicked);
	if (ScaleInButton)
		ScaleInButton->OnClicked.AddDynamic(this, &URammsTestWidget::OnScaleInClicked);
}

void URammsTestWidget::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	if (MainBorder)
	{
		MainBorder->SetBrushColor(Style->Colors.Surface);
		MainBorder->SetPadding(FMargin(Style->Spacing.Large));
	}

	if (TitleText)
	{
		TitleText->SetFont(Style->Typography.HeadingMedium);
		TitleText->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}

	if (StatusText)
	{
		StatusText->SetFont(Style->Typography.Body);
		StatusText->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
	}
}

void URammsTestWidget::OnFadeInClicked()
{
	if (StatusText)
		StatusText->SetText(FText::FromString(TEXT("Playing: Fade In")));
	FadeIn();
}

void URammsTestWidget::OnFadeOutClicked()
{
	if (StatusText)
		StatusText->SetText(FText::FromString(TEXT("Playing: Fade Out")));
	FadeOut();
}

void URammsTestWidget::OnSlideInClicked()
{
	if (StatusText)
		StatusText->SetText(FText::FromString(TEXT("Playing: Slide In from Top")));
	SlideIn(FVector2D(0, -200));
}

void URammsTestWidget::OnScaleInClicked()
{
	if (StatusText)
		StatusText->SetText(FText::FromString(TEXT("Playing: Scale In")));
	ScaleIn();
}
