// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsMebotController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/UniformGridSlot.h"
#include "Interfaces/IRammsRobotController.h"

URammsMebotController::URammsMebotController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoFindRobotController = true;
}

void URammsMebotController::BuildWidgetTree()
{
	if (!WidgetTree || PanelBorder)
		return;

	// Root border — transparent background; container provides the visual frame
	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetBrushColor(FLinearColor::Transparent);
	PanelBorder->SetPadding(FMargin(12.0f));
	WidgetTree->RootWidget = PanelBorder;

	// Vertical layout: header + grid
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

	// Mode grid
	ModeGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("ModeGrid"));
	ModeGrid->SetMinDesiredSlotWidth(ButtonImageSize.X + 32.0f);
	ModeGrid->SetMinDesiredSlotHeight(ButtonImageSize.Y + 40.0f);
	ModeGrid->SetSlotPadding(FMargin(4.0f));
	UVerticalBoxSlot* GridSlot = VBox->AddChildToVerticalBox(ModeGrid);
	if (GridSlot)
	{
		GridSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// Create mode buttons
	auto CreateModeButton = [this](const FName& Name, const FText& LabelText, UTexture2D* Icon) -> URammsImageButton*
	{
		URammsImageButton* Btn = WidgetTree->ConstructWidget<URammsImageButton>(URammsImageButton::StaticClass(), Name);
		// Set properties via the public API after construction
		Btn->SetLabelText(LabelText);
		Btn->SetImageSize(ButtonImageSize);
		if (Icon)
		{
			Btn->SetButtonImage(Icon);
		}
		if (Style)
		{
			Btn->SetStyle(Style);
		}
		return Btn;
	};

	SelfLevelButton = CreateModeButton(TEXT("SelfLevelBtn"), FText::FromString(TEXT("Self-Level")), SelfLevelIcon);
	CurbAscentButton = CreateModeButton(TEXT("CurbAscentBtn"), FText::FromString(TEXT("Curb Ascent")), CurbAscentIcon);
	CurbDescentButton = CreateModeButton(TEXT("CurbDescentBtn"), FText::FromString(TEXT("Curb Descent")), CurbDescentIcon);

	// Add to grid
	// Row 0: all three buttons
	UUniformGridSlot* S0 = ModeGrid->AddChildToUniformGrid(SelfLevelButton, 0, 0);
	UUniformGridSlot* S1 = ModeGrid->AddChildToUniformGrid(CurbAscentButton, 0, 1);
	UUniformGridSlot* S2 = ModeGrid->AddChildToUniformGrid(CurbDescentButton, 0, 2);

	if (S0) S0->SetHorizontalAlignment(HAlign_Center);
	if (S1) S1->SetHorizontalAlignment(HAlign_Center);
	if (S2) S2->SetHorizontalAlignment(HAlign_Center);
}

void URammsMebotController::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsMebotController::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button click events
	if (SelfLevelButton)
	{
		SelfLevelButton->OnClicked.AddDynamic(this, &URammsMebotController::OnSelfLevelClicked);
	}
	if (CurbAscentButton)
	{
		CurbAscentButton->OnClicked.AddDynamic(this, &URammsMebotController::OnCurbAscentClicked);
	}
	if (CurbDescentButton)
	{
		CurbDescentButton->OnClicked.AddDynamic(this, &URammsMebotController::OnCurbDescentClicked);
	}

	UpdateModeButtons();
}

void URammsMebotController::ApplyStyle_Implementation()
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

	// Propagate style to child buttons
	if (SelfLevelButton) SelfLevelButton->SetStyle(Style);
	if (CurbAscentButton) CurbAscentButton->SetStyle(Style);
	if (CurbDescentButton) CurbDescentButton->SetStyle(Style);
}

void URammsMebotController::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// Push icon/property changes from editor to child buttons
	if (SelfLevelButton)
	{
		if (SelfLevelIcon) SelfLevelButton->SetButtonImage(SelfLevelIcon);
		SelfLevelButton->SetImageSize(ButtonImageSize);
	}
	if (CurbAscentButton)
	{
		if (CurbAscentIcon) CurbAscentButton->SetButtonImage(CurbAscentIcon);
		CurbAscentButton->SetImageSize(ButtonImageSize);
	}
	if (CurbDescentButton)
	{
		if (CurbDescentIcon) CurbDescentButton->SetButtonImage(CurbDescentIcon);
		CurbDescentButton->SetImageSize(ButtonImageSize);
	}
	if (HeaderText)
	{
		HeaderText->SetText(HeaderTitle);
	}
}

void URammsMebotController::SetMode(ERammsMebotMode NewMode)
{
	if (CurrentMode == NewMode)
		return;

	ERammsMebotMode PreviousMode = CurrentMode;
	CurrentMode = NewMode;
	UpdateModeButtons();
	OnModeChanged.Broadcast(CurrentMode, PreviousMode);
}

void URammsMebotController::CancelMode()
{
	SetMode(ERammsMebotMode::None);
}

void URammsMebotController::SetModeIcon(ERammsMebotMode Mode, UTexture2D* Icon)
{
	switch (Mode)
	{
	case ERammsMebotMode::SelfLevel:
		SelfLevelIcon = Icon;
		if (SelfLevelButton) SelfLevelButton->SetButtonImage(Icon);
		break;
	case ERammsMebotMode::CurbAscent:
		CurbAscentIcon = Icon;
		if (CurbAscentButton) CurbAscentButton->SetButtonImage(Icon);
		break;
	case ERammsMebotMode::CurbDescent:
		CurbDescentIcon = Icon;
		if (CurbDescentButton) CurbDescentButton->SetButtonImage(Icon);
		break;
	default:
		break;
	}
}

void URammsMebotController::UpdateModeButtons()
{
	if (SelfLevelButton) SelfLevelButton->SetActive(CurrentMode == ERammsMebotMode::SelfLevel);
	if (CurbAscentButton) CurbAscentButton->SetActive(CurrentMode == ERammsMebotMode::CurbAscent);
	if (CurbDescentButton) CurbDescentButton->SetActive(CurrentMode == ERammsMebotMode::CurbDescent);
}

void URammsMebotController::OnSelfLevelClicked()
{
	HandleModeButtonClicked(ERammsMebotMode::SelfLevel);
}

void URammsMebotController::OnCurbAscentClicked()
{
	HandleModeButtonClicked(ERammsMebotMode::CurbAscent);
}

void URammsMebotController::OnCurbDescentClicked()
{
	HandleModeButtonClicked(ERammsMebotMode::CurbDescent);
}

void URammsMebotController::HandleModeButtonClicked(ERammsMebotMode Mode)
{
	// Toggle: clicking active mode deactivates it
	if (CurrentMode == Mode)
	{
		SetMode(ERammsMebotMode::None);
	}
	else
	{
		SetMode(Mode);
	}

	// Forward to robot controller
	if (ResolvedControllerActor.IsValid())
	{
		IRammsRobotController::Execute_RequestMebotMode(ResolvedControllerActor.Get(), CurrentMode);
	}
}

void URammsMebotController::OnRobotControllerResolved(AActor* ControllerActor)
{
	// Sync current mode from the robot
	ERammsMebotMode ActualMode = IRammsRobotController::Execute_GetCurrentMebotMode(ControllerActor);
	if (ActualMode != CurrentMode)
	{
		CurrentMode = ActualMode;
		UpdateModeButtons();
	}

	UE_LOG(LogTemp, Log, TEXT("URammsMebotController: Resolved robot controller '%s'"), *ControllerActor->GetName());
}
