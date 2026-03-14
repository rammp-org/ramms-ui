// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsTaskWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "RammsUISubsystem.h"

void URammsTaskWidget::BuildWidgetTree()
{
	if (!WidgetTree || PanelBorder)
		return;

	// Root border — transparent background; container provides the visual frame
	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetBrushColor(FLinearColor::Transparent);
	PanelBorder->SetPadding(FMargin(12.0f));
	WidgetTree->RootWidget = PanelBorder;

	// Vertical layout: header + status + buttons
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainVBox"));
	PanelBorder->AddChild(VBox);

	// Header
	HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HeaderText"));
	HeaderText->SetText(HeaderTitle);
	HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UVerticalBoxSlot* HeaderSlot = VBox->AddChildToVerticalBox(HeaderText);
	if (HeaderSlot)
	{
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		HeaderSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// Status label
	StatusLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusLabel"));
	StatusLabel->SetText(StatusText);
	StatusLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
	StatusLabel->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* StatusSlot = VBox->AddChildToVerticalBox(StatusLabel);
	if (StatusSlot)
	{
		StatusSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		StatusSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	if (StatusText.IsEmpty())
	{
		StatusLabel->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Buttons row
	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonRow"));
	UVerticalBoxSlot* RowSlot = VBox->AddChildToVerticalBox(ButtonRow);
	if (RowSlot)
	{
		RowSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// Cancel button (less destructive, on the left)
	CancelButton = WidgetTree->ConstructWidget<URammsImageButton>(URammsImageButton::StaticClass(), TEXT("CancelBtn"));
	CancelButton->SetLabelText(FText::FromString(TEXT("Cancel")));
	CancelButton->SetImageSize(ButtonImageSize);
	if (CancelIcon) CancelButton->SetButtonImage(CancelIcon);
	if (Style) CancelButton->SetStyle(Style);

	UHorizontalBoxSlot* CancelSlot = ButtonRow->AddChildToHorizontalBox(CancelButton);
	if (CancelSlot)
	{
		CancelSlot->SetPadding(FMargin(4.0f));
		CancelSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// Spacer between buttons
	USpacer* BtnSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("BtnSpacer"));
	BtnSpacer->SetSize(FVector2D(16.0f, 1.0f));
	ButtonRow->AddChildToHorizontalBox(BtnSpacer);

	// Exit button (primary action, on the right)
	ExitButton = WidgetTree->ConstructWidget<URammsImageButton>(URammsImageButton::StaticClass(), TEXT("ExitBtn"));
	ExitButton->SetLabelText(FText::FromString(TEXT("Exit")));
	ExitButton->SetImageSize(ButtonImageSize);
	if (ExitIcon) ExitButton->SetButtonImage(ExitIcon);
	if (Style) ExitButton->SetStyle(Style);

	UHorizontalBoxSlot* ExitSlot = ButtonRow->AddChildToHorizontalBox(ExitButton);
	if (ExitSlot)
	{
		ExitSlot->SetPadding(FMargin(4.0f));
		ExitSlot->SetHorizontalAlignment(HAlign_Center);
	}
}

void URammsTaskWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsTaskWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ExitButton)
	{
		ExitButton->OnClicked.AddDynamic(this, &URammsTaskWidget::OnExitClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &URammsTaskWidget::OnCancelClicked);
	}
}

void URammsTaskWidget::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	// PanelBorder stays transparent — parent container provides background

	if (PanelBorder)
	{
		PanelBorder->SetPadding(FMargin(Style->Spacing.Medium));
	}

	if (HeaderText)
	{
		HeaderText->SetFont(Style->Typography.HeadingSmall);
		HeaderText->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}

	if (StatusLabel)
	{
		StatusLabel->SetFont(Style->Typography.Body);
		StatusLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
	}

	if (ExitButton) ExitButton->SetStyle(Style);
	if (CancelButton) CancelButton->SetStyle(Style);
}

void URammsTaskWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (ExitButton)
	{
		if (ExitIcon) ExitButton->SetButtonImage(ExitIcon);
		ExitButton->SetImageSize(ButtonImageSize);
	}
	if (CancelButton)
	{
		if (CancelIcon) CancelButton->SetButtonImage(CancelIcon);
		CancelButton->SetImageSize(ButtonImageSize);
	}
	if (HeaderText)
	{
		HeaderText->SetText(HeaderTitle);
	}
}

void URammsTaskWidget::SetHeaderTitle(FText Title)
{
	HeaderTitle = Title;
	if (HeaderText)
	{
		HeaderText->SetText(HeaderTitle);
	}
}

void URammsTaskWidget::SetStatusText(FText Text)
{
	StatusText = Text;
	if (StatusLabel)
	{
		StatusLabel->SetText(StatusText);
		StatusLabel->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void URammsTaskWidget::SetActionIcon(ERammsTaskAction Action, UTexture2D* Icon)
{
	switch (Action)
	{
	case ERammsTaskAction::Exit:
		ExitIcon = Icon;
		if (ExitButton) ExitButton->SetButtonImage(Icon);
		break;
	case ERammsTaskAction::Cancel:
		CancelIcon = Icon;
		if (CancelButton) CancelButton->SetButtonImage(Icon);
		break;
	}
}

void URammsTaskWidget::SetActionEnabled(ERammsTaskAction Action, bool bEnabled)
{
	switch (Action)
	{
	case ERammsTaskAction::Exit:
		if (ExitButton) ExitButton->SetButtonEnabled(bEnabled);
		break;
	case ERammsTaskAction::Cancel:
		if (CancelButton) CancelButton->SetButtonEnabled(bEnabled);
		break;
	}
}

void URammsTaskWidget::OnExitClicked()
{
	OnTaskAction.Broadcast(ERammsTaskAction::Exit);

	if (UWorld* World = GetWorld())
	{
		if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
		{
			Subsystem->BroadcastTaskAction(ERammsTaskAction::Exit);
		}
	}
}

void URammsTaskWidget::OnCancelClicked()
{
	OnTaskAction.Broadcast(ERammsTaskAction::Cancel);

	if (UWorld* World = GetWorld())
	{
		if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
		{
			Subsystem->BroadcastTaskAction(ERammsTaskAction::Cancel);
		}
	}
}
