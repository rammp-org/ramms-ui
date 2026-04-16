// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsTaskWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "RammsUISubsystem.h"

void URammsTaskWidget::ResetCachedWidgets()
{
	PanelBorder = nullptr;
	HeaderText = nullptr;
	StatusLabel = nullptr;
	ExitButton = nullptr;
	CancelButton = nullptr;
	ConfirmButton = nullptr;
}

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

	// Buttons row — Fill slots give every visible button the same width
	UHorizontalBox*	  ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonRow"));
	UVerticalBoxSlot* RowSlot = VBox->AddChildToVerticalBox(ButtonRow);
	if (RowSlot)
	{
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	FSlateChildSize EqualFill(ESlateSizeRule::Fill);

	// Cancel button (less destructive, on the left)
	CancelButton = WidgetTree->ConstructWidget<URammsImageButton>(URammsImageButton::StaticClass(), TEXT("CancelBtn"));
	CancelButton->SetLabelText(FText::FromString(TEXT("Cancel")));
	CancelButton->SetImageSize(ButtonImageSize);
	CancelButton->SetApplyIconTint(bApplyCancelIconTint);
	if (CancelIcon)
		CancelButton->SetButtonImage(CancelIcon);
	if (Style)
		CancelButton->SetStyle(Style);

	UHorizontalBoxSlot* CancelSlot = ButtonRow->AddChildToHorizontalBox(CancelButton);
	if (CancelSlot)
	{
		CancelSlot->SetSize(EqualFill);
		CancelSlot->SetPadding(FMargin(4.0f));
		CancelSlot->SetHorizontalAlignment(HAlign_Fill);
		CancelSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// Exit button (primary action, in the middle)
	ExitButton = WidgetTree->ConstructWidget<URammsImageButton>(URammsImageButton::StaticClass(), TEXT("ExitBtn"));
	ExitButton->SetLabelText(FText::FromString(TEXT("Exit")));
	ExitButton->SetImageSize(ButtonImageSize);
	ExitButton->SetApplyIconTint(bApplyExitIconTint);
	if (ExitIcon)
		ExitButton->SetButtonImage(ExitIcon);
	if (Style)
		ExitButton->SetStyle(Style);

	UHorizontalBoxSlot* ExitSlot = ButtonRow->AddChildToHorizontalBox(ExitButton);
	if (ExitSlot)
	{
		ExitSlot->SetSize(EqualFill);
		ExitSlot->SetPadding(FMargin(4.0f));
		ExitSlot->SetHorizontalAlignment(HAlign_Fill);
		ExitSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// Confirm button (optional, shown via bShowConfirmButton)
	ConfirmButton = WidgetTree->ConstructWidget<URammsImageButton>(URammsImageButton::StaticClass(), TEXT("ConfirmBtn"));
	ConfirmButton->SetLabelText(FText::FromString(TEXT("Confirm")));
	ConfirmButton->SetImageSize(ButtonImageSize);
	ConfirmButton->SetApplyIconTint(bApplyConfirmIconTint);
	if (ConfirmIcon)
		ConfirmButton->SetButtonImage(ConfirmIcon);
	if (Style)
		ConfirmButton->SetStyle(Style);

	UHorizontalBoxSlot* ConfirmSlot = ButtonRow->AddChildToHorizontalBox(ConfirmButton);
	if (ConfirmSlot)
	{
		ConfirmSlot->SetSize(EqualFill);
		ConfirmSlot->SetPadding(FMargin(4.0f));
		ConfirmSlot->SetHorizontalAlignment(HAlign_Fill);
		ConfirmSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// Apply initial visibility — collapsed Fill slots release their share to siblings
	ConfirmButton->SetVisibility(bShowConfirmButton ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
		ExitButton->OnClicked.AddUniqueDynamic(this, &URammsTaskWidget::OnExitClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddUniqueDynamic(this, &URammsTaskWidget::OnCancelClicked);
	}
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddUniqueDynamic(this, &URammsTaskWidget::OnConfirmClicked);
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

	if (ExitButton)
		ExitButton->SetStyle(Style);
	if (CancelButton)
		CancelButton->SetStyle(Style);
	if (ConfirmButton)
		ConfirmButton->SetStyle(Style);

	// Apply style-driven image button size only when the per-instance size
	// matches the legacy default (48×48), preserving explicit overrides.
	static const FVector2D LegacyDefault(48.0f, 48.0f);
	if (ButtonImageSize.Equals(LegacyDefault, 0.1f))
	{
		const FVector2D StyleImgSz = Style->Interaction.ImageButtonSize;
		if (ExitButton)
			ExitButton->SetImageSize(StyleImgSz);
		if (CancelButton)
			CancelButton->SetImageSize(StyleImgSz);
		if (ConfirmButton)
			ConfirmButton->SetImageSize(StyleImgSz);
	}
}

void URammsTaskWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (ExitButton)
	{
		if (ExitIcon)
			ExitButton->SetButtonImage(ExitIcon);
		ExitButton->SetImageSize(ButtonImageSize);
		ExitButton->SetApplyIconTint(bApplyExitIconTint);
	}
	if (CancelButton)
	{
		if (CancelIcon)
			CancelButton->SetButtonImage(CancelIcon);
		CancelButton->SetImageSize(ButtonImageSize);
		CancelButton->SetApplyIconTint(bApplyCancelIconTint);
	}
	if (ConfirmButton)
	{
		if (ConfirmIcon)
			ConfirmButton->SetButtonImage(ConfirmIcon);
		ConfirmButton->SetImageSize(ButtonImageSize);
		ConfirmButton->SetApplyIconTint(bApplyConfirmIconTint);
		ConfirmButton->SetVisibility(bShowConfirmButton
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
	if (HeaderText)
	{
		HeaderText->SetText(HeaderTitle);
		HeaderText->SetVisibility(HeaderTitle.IsEmptyOrWhitespace()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::SelfHitTestInvisible);
	}
	if (StatusLabel)
	{
		StatusLabel->SetText(StatusText);
		StatusLabel->SetVisibility(StatusText.IsEmptyOrWhitespace()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::SelfHitTestInvisible);
	}
}

void URammsTaskWidget::SetHeaderTitle(FText Title)
{
	HeaderTitle = Title;
	if (HeaderText)
	{
		HeaderText->SetText(HeaderTitle);
		HeaderText->SetVisibility(HeaderTitle.IsEmptyOrWhitespace()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::SelfHitTestInvisible);
	}
}

void URammsTaskWidget::SetStatusText(FText Text)
{
	StatusText = Text;
	if (StatusLabel)
	{
		StatusLabel->SetText(StatusText);
		StatusLabel->SetVisibility(StatusText.IsEmptyOrWhitespace()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::SelfHitTestInvisible);
	}
}

void URammsTaskWidget::SetActionIcon(ERammsTaskAction Action, UTexture2D* Icon)
{
	switch (Action)
	{
		case ERammsTaskAction::Exit:
			ExitIcon = Icon;
			if (ExitButton)
				ExitButton->SetButtonImage(Icon);
			break;
		case ERammsTaskAction::Cancel:
			CancelIcon = Icon;
			if (CancelButton)
				CancelButton->SetButtonImage(Icon);
			break;
		case ERammsTaskAction::Confirm:
			ConfirmIcon = Icon;
			if (ConfirmButton)
				ConfirmButton->SetButtonImage(Icon);
			break;
	}
}

void URammsTaskWidget::SetActionEnabled(ERammsTaskAction Action, bool bEnabled)
{
	switch (Action)
	{
		case ERammsTaskAction::Exit:
			if (ExitButton)
				ExitButton->SetButtonEnabled(bEnabled);
			break;
		case ERammsTaskAction::Cancel:
			if (CancelButton)
				CancelButton->SetButtonEnabled(bEnabled);
			break;
		case ERammsTaskAction::Confirm:
			if (ConfirmButton)
				ConfirmButton->SetButtonEnabled(bEnabled);
			break;
	}
}

void URammsTaskWidget::SetConfirmVisible(bool bVisible)
{
	bShowConfirmButton = bVisible;
	if (ConfirmButton)
	{
		ConfirmButton->SetVisibility(bVisible
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
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

void URammsTaskWidget::OnConfirmClicked()
{
	OnTaskAction.Broadcast(ERammsTaskAction::Confirm);

	if (UWorld* World = GetWorld())
	{
		if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
		{
			Subsystem->BroadcastTaskAction(ERammsTaskAction::Confirm);
		}
	}
}
