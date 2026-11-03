// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsRammpGen1Controller.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/UniformGridSlot.h"
#include "Interfaces/IRammsRobotController.h"

URammsRammpGen1Controller::URammsRammpGen1Controller(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoFindRobotController = true;
}

void URammsRammpGen1Controller::ResetCachedWidgets()
{
	PanelBorder = nullptr;
	HeaderText = nullptr;
	ModeGrid = nullptr;
	SelfLevelButton = nullptr;
	CurbAscentButton = nullptr;
	CurbDescentButton = nullptr;
}

void URammsRammpGen1Controller::BuildWidgetTree()
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
	auto CreateModeButton = [this](const FName& Name, const FText& LabelText, UTexture2D* Icon) -> URammsImageButton* {
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

	// Add to grid using GridColumns for layout
	const int32				   Cols = FMath::Max(GridColumns, 1);
	TArray<URammsImageButton*> Buttons = { SelfLevelButton, CurbAscentButton, CurbDescentButton };
	for (int32 i = 0; i < Buttons.Num(); ++i)
	{
		UUniformGridSlot* CellSlot = ModeGrid->AddChildToUniformGrid(Buttons[i], i / Cols, i % Cols);
		if (CellSlot)
			CellSlot->SetHorizontalAlignment(HAlign_Center);
	}
}

void URammsRammpGen1Controller::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsRammpGen1Controller::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button click events (AddUniqueDynamic prevents duplicate bindings on reparent)
	if (SelfLevelButton)
	{
		SelfLevelButton->OnClicked.AddUniqueDynamic(this, &URammsRammpGen1Controller::OnSelfLevelClicked);
	}
	if (CurbAscentButton)
	{
		CurbAscentButton->OnClicked.AddUniqueDynamic(this, &URammsRammpGen1Controller::OnCurbAscentClicked);
	}
	if (CurbDescentButton)
	{
		CurbDescentButton->OnClicked.AddUniqueDynamic(this, &URammsRammpGen1Controller::OnCurbDescentClicked);
	}

	UpdateModeButtons();
}

void URammsRammpGen1Controller::ApplyStyle_Implementation()
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
	if (SelfLevelButton)
		SelfLevelButton->SetStyle(Style);
	if (CurbAscentButton)
		CurbAscentButton->SetStyle(Style);
	if (CurbDescentButton)
		CurbDescentButton->SetStyle(Style);
}

void URammsRammpGen1Controller::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// Push icon/property changes from editor to child buttons
	if (SelfLevelButton)
	{
		if (SelfLevelIcon)
			SelfLevelButton->SetButtonImage(SelfLevelIcon);
		SelfLevelButton->SetImageSize(ButtonImageSize);
	}
	if (CurbAscentButton)
	{
		if (CurbAscentIcon)
			CurbAscentButton->SetButtonImage(CurbAscentIcon);
		CurbAscentButton->SetImageSize(ButtonImageSize);
	}
	if (CurbDescentButton)
	{
		if (CurbDescentIcon)
			CurbDescentButton->SetButtonImage(CurbDescentIcon);
		CurbDescentButton->SetImageSize(ButtonImageSize);
	}
	if (HeaderText)
	{
		HeaderText->SetText(HeaderTitle);
		HeaderText->SetVisibility(HeaderTitle.IsEmptyOrWhitespace()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::SelfHitTestInvisible);
	}
}

void URammsRammpGen1Controller::SetMode(ERammsRammpGen1Mode NewMode)
{
	if (CurrentMode == NewMode)
		return;

	ERammsRammpGen1Mode PreviousMode = CurrentMode;
	CurrentMode = NewMode;
	UpdateModeButtons();
	OnModeChanged.Broadcast(CurrentMode, PreviousMode);
}

void URammsRammpGen1Controller::CancelMode()
{
	SetMode(ERammsRammpGen1Mode::None);
}

void URammsRammpGen1Controller::SetModeIcon(ERammsRammpGen1Mode Mode, UTexture2D* Icon)
{
	switch (Mode)
	{
		case ERammsRammpGen1Mode::SelfLevel:
			SelfLevelIcon = Icon;
			if (SelfLevelButton)
				SelfLevelButton->SetButtonImage(Icon);
			break;
		case ERammsRammpGen1Mode::CurbAscent:
			CurbAscentIcon = Icon;
			if (CurbAscentButton)
				CurbAscentButton->SetButtonImage(Icon);
			break;
		case ERammsRammpGen1Mode::CurbDescent:
			CurbDescentIcon = Icon;
			if (CurbDescentButton)
				CurbDescentButton->SetButtonImage(Icon);
			break;
		default:
			break;
	}
}

void URammsRammpGen1Controller::UpdateModeButtons()
{
	if (SelfLevelButton)
		SelfLevelButton->SetActive(CurrentMode == ERammsRammpGen1Mode::SelfLevel);
	if (CurbAscentButton)
		CurbAscentButton->SetActive(CurrentMode == ERammsRammpGen1Mode::CurbAscent);
	if (CurbDescentButton)
		CurbDescentButton->SetActive(CurrentMode == ERammsRammpGen1Mode::CurbDescent);
}

void URammsRammpGen1Controller::OnSelfLevelClicked()
{
	HandleModeButtonClicked(ERammsRammpGen1Mode::SelfLevel);
}

void URammsRammpGen1Controller::OnCurbAscentClicked()
{
	HandleModeButtonClicked(ERammsRammpGen1Mode::CurbAscent);
}

void URammsRammpGen1Controller::OnCurbDescentClicked()
{
	HandleModeButtonClicked(ERammsRammpGen1Mode::CurbDescent);
}

void URammsRammpGen1Controller::HandleModeButtonClicked(ERammsRammpGen1Mode Mode)
{
	// Toggle: clicking active mode deactivates it
	if (CurrentMode == Mode)
	{
		SetMode(ERammsRammpGen1Mode::None);
	}
	else
	{
		SetMode(Mode);
	}

	// Forward to robot controller
	if (ResolvedControllerActor.IsValid())
	{
		IRammsRobotController::Execute_RequestRammpGen1Mode(ResolvedControllerActor.Get(), CurrentMode);
	}
}

void URammsRammpGen1Controller::OnRobotControllerResolved(AActor* ControllerActor)
{
	// Sync current mode from the robot (SetMode tells OnModeChanged's listeners too)
	SetMode(IRammsRobotController::Execute_GetCurrentRammpGen1Mode(ControllerActor));

	UE_LOG(LogTemp, Log, TEXT("URammsRammpGen1Controller: Resolved robot controller '%s'"), *ControllerActor->GetName());
}
