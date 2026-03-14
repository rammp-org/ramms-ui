// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsArmController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Interfaces/IRammsRobotController.h"

URammsArmController::URammsArmController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoFindRobotController = true;
}

void URammsArmController::ResetCachedWidgets()
{
	PanelBorder = nullptr;
	HeaderText = nullptr;
	HomeButton = nullptr;
	RetractButton = nullptr;
}

void URammsArmController::BuildWidgetTree()
{
	if (!WidgetTree || PanelBorder)
		return;

	// Root border — transparent background; container provides the visual frame
	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetBrushColor(FLinearColor::Transparent);
	PanelBorder->SetPadding(FMargin(12.0f));
	WidgetTree->RootWidget = PanelBorder;

	// Vertical layout: header + buttons row
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainVBox"));
	PanelBorder->AddChild(VBox);

	// Header
	HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HeaderText"));
	HeaderText->SetText(HeaderTitle);
	HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UVerticalBoxSlot* HeaderSlot = VBox->AddChildToVerticalBox(HeaderText);
	if (HeaderSlot)
	{
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		HeaderSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// Buttons row
	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonRow"));
	UVerticalBoxSlot* RowSlot = VBox->AddChildToVerticalBox(ButtonRow);
	if (RowSlot)
	{
		RowSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// Home button
	HomeButton = WidgetTree->ConstructWidget<URammsImageButton>(URammsImageButton::StaticClass(), TEXT("HomeBtn"));
	HomeButton->SetLabelText(FText::FromString(TEXT("Home")));
	HomeButton->SetImageSize(ButtonImageSize);
	if (HomeIcon) HomeButton->SetButtonImage(HomeIcon);
	if (Style) HomeButton->SetStyle(Style);

	UHorizontalBoxSlot* HomeSlot = ButtonRow->AddChildToHorizontalBox(HomeButton);
	if (HomeSlot)
	{
		HomeSlot->SetPadding(FMargin(4.0f));
		HomeSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// Retract button
	RetractButton = WidgetTree->ConstructWidget<URammsImageButton>(URammsImageButton::StaticClass(), TEXT("RetractBtn"));
	RetractButton->SetLabelText(FText::FromString(TEXT("Retract")));
	RetractButton->SetImageSize(ButtonImageSize);
	if (RetractIcon) RetractButton->SetButtonImage(RetractIcon);
	if (Style) RetractButton->SetStyle(Style);

	UHorizontalBoxSlot* RetractSlot = ButtonRow->AddChildToHorizontalBox(RetractButton);
	if (RetractSlot)
	{
		RetractSlot->SetPadding(FMargin(4.0f));
		RetractSlot->SetHorizontalAlignment(HAlign_Center);
	}
}

void URammsArmController::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsArmController::NativeConstruct()
{
	Super::NativeConstruct();

	if (HomeButton)
	{
		HomeButton->OnClicked.AddDynamic(this, &URammsArmController::OnHomeClicked);
	}
	if (RetractButton)
	{
		RetractButton->OnClicked.AddDynamic(this, &URammsArmController::OnRetractClicked);
	}
}

void URammsArmController::ApplyStyle_Implementation()
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

	if (HomeButton) HomeButton->SetStyle(Style);
	if (RetractButton) RetractButton->SetStyle(Style);
}

void URammsArmController::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (HomeButton)
	{
		if (HomeIcon) HomeButton->SetButtonImage(HomeIcon);
		HomeButton->SetImageSize(ButtonImageSize);
	}
	if (RetractButton)
	{
		if (RetractIcon) RetractButton->SetButtonImage(RetractIcon);
		RetractButton->SetImageSize(ButtonImageSize);
	}
	if (HeaderText)
	{
		HeaderText->SetText(HeaderTitle);
	}
}

void URammsArmController::SetActionIcon(ERammsArmAction Action, UTexture2D* Icon)
{
	switch (Action)
	{
	case ERammsArmAction::Home:
		HomeIcon = Icon;
		if (HomeButton) HomeButton->SetButtonImage(Icon);
		break;
	case ERammsArmAction::Retract:
		RetractIcon = Icon;
		if (RetractButton) RetractButton->SetButtonImage(Icon);
		break;
	}
}

void URammsArmController::SetActionEnabled(ERammsArmAction Action, bool bEnabled)
{
	switch (Action)
	{
	case ERammsArmAction::Home:
		if (HomeButton) HomeButton->SetButtonEnabled(bEnabled);
		break;
	case ERammsArmAction::Retract:
		if (RetractButton) RetractButton->SetButtonEnabled(bEnabled);
		break;
	}
}

void URammsArmController::OnHomeClicked()
{
	OnArmAction.Broadcast(ERammsArmAction::Home);

	if (ResolvedControllerActor.IsValid())
	{
		IRammsRobotController::Execute_RequestArmAction(ResolvedControllerActor.Get(), ERammsArmAction::Home);
	}
}

void URammsArmController::OnRetractClicked()
{
	OnArmAction.Broadcast(ERammsArmAction::Retract);

	if (ResolvedControllerActor.IsValid())
	{
		IRammsRobotController::Execute_RequestArmAction(ResolvedControllerActor.Get(), ERammsArmAction::Retract);
	}
}

void URammsArmController::OnRobotControllerResolved(AActor* ControllerActor)
{
	UE_LOG(LogTemp, Log, TEXT("URammsArmController: Resolved robot controller '%s'"), *ControllerActor->GetName());
}
