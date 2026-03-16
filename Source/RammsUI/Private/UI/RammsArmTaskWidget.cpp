// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsArmTaskWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/UniformGridSlot.h"
#include "Interfaces/IRammsRobotController.h"
#include "RammsUISubsystem.h"

URammsArmTaskWidget::URammsArmTaskWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoFindRobotController = true;
}

void URammsArmTaskWidget::ResetCachedWidgets()
{
	PanelBorder = nullptr;
	HeaderText = nullptr;
	TaskGrid = nullptr;
	OpenDoorButton = nullptr;
	OrderDrinkButton = nullptr;
	DrinkButton = nullptr;
}

void URammsArmTaskWidget::BuildWidgetTree()
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

	// Task grid
	TaskGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("TaskGrid"));
	TaskGrid->SetMinDesiredSlotWidth(ButtonImageSize.X + 32.0f);
	TaskGrid->SetMinDesiredSlotHeight(ButtonImageSize.Y + 40.0f);
	TaskGrid->SetSlotPadding(FMargin(4.0f));
	UVerticalBoxSlot* GridSlot = VBox->AddChildToVerticalBox(TaskGrid);
	if (GridSlot)
	{
		GridSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// Create task buttons
	auto CreateTaskButton = [this](const FName& Name, const FText& LabelText, UTexture2D* Icon) -> URammsImageButton* {
		URammsImageButton* Btn = WidgetTree->ConstructWidget<URammsImageButton>(URammsImageButton::StaticClass(), Name);
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

	OpenDoorButton = CreateTaskButton(TEXT("OpenDoorBtn"), FText::FromString(TEXT("Open Door")), OpenDoorIcon);
	OrderDrinkButton = CreateTaskButton(TEXT("OrderDrinkBtn"), FText::FromString(TEXT("Order Drink")), OrderDrinkIcon);
	DrinkButton = CreateTaskButton(TEXT("DrinkBtn"), FText::FromString(TEXT("Drink")), DrinkIcon);

	// Add to grid — Row 0: all three buttons
	UUniformGridSlot* S0 = TaskGrid->AddChildToUniformGrid(OpenDoorButton, 0, 0);
	UUniformGridSlot* S1 = TaskGrid->AddChildToUniformGrid(OrderDrinkButton, 0, 1);
	UUniformGridSlot* S2 = TaskGrid->AddChildToUniformGrid(DrinkButton, 0, 2);

	if (S0)
		S0->SetHorizontalAlignment(HAlign_Center);
	if (S1)
		S1->SetHorizontalAlignment(HAlign_Center);
	if (S2)
		S2->SetHorizontalAlignment(HAlign_Center);
}

void URammsArmTaskWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsArmTaskWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button click events
	if (OpenDoorButton)
	{
		OpenDoorButton->OnClicked.AddDynamic(this, &URammsArmTaskWidget::OnOpenDoorClicked);
	}
	if (OrderDrinkButton)
	{
		OrderDrinkButton->OnClicked.AddDynamic(this, &URammsArmTaskWidget::OnOrderDrinkClicked);
	}
	if (DrinkButton)
	{
		DrinkButton->OnClicked.AddDynamic(this, &URammsArmTaskWidget::OnDrinkClicked);
	}

	UpdateTaskButtons();
}

void URammsArmTaskWidget::ApplyStyle_Implementation()
{
	if (!Style)
		return;

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
	if (OpenDoorButton)
		OpenDoorButton->SetStyle(Style);
	if (OrderDrinkButton)
		OrderDrinkButton->SetStyle(Style);
	if (DrinkButton)
		DrinkButton->SetStyle(Style);
}

void URammsArmTaskWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (OpenDoorButton)
	{
		if (OpenDoorIcon)
			OpenDoorButton->SetButtonImage(OpenDoorIcon);
		OpenDoorButton->SetImageSize(ButtonImageSize);
	}
	if (OrderDrinkButton)
	{
		if (OrderDrinkIcon)
			OrderDrinkButton->SetButtonImage(OrderDrinkIcon);
		OrderDrinkButton->SetImageSize(ButtonImageSize);
	}
	if (DrinkButton)
	{
		if (DrinkIcon)
			DrinkButton->SetButtonImage(DrinkIcon);
		DrinkButton->SetImageSize(ButtonImageSize);
	}
	if (HeaderText)
	{
		HeaderText->SetText(HeaderTitle);
	}
}

void URammsArmTaskWidget::SetTask(ERammsArmTask NewTask)
{
	if (CurrentTask == NewTask)
		return;

	ERammsArmTask PreviousTask = CurrentTask;
	CurrentTask = NewTask;
	UpdateTaskButtons();
	OnTaskChanged.Broadcast(CurrentTask, PreviousTask);

	// Broadcast via the UI event bus
	if (UWorld* World = GetWorld())
	{
		if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
		{
			Subsystem->BroadcastArmTaskChanged(CurrentTask, PreviousTask);
		}
	}
}

void URammsArmTaskWidget::CancelTask()
{
	SetTask(ERammsArmTask::None);
}

void URammsArmTaskWidget::SetTaskIcon(ERammsArmTask Task, UTexture2D* Icon)
{
	switch (Task)
	{
		case ERammsArmTask::OpenDoor:
			OpenDoorIcon = Icon;
			if (OpenDoorButton)
				OpenDoorButton->SetButtonImage(Icon);
			break;
		case ERammsArmTask::OrderDrink:
			OrderDrinkIcon = Icon;
			if (OrderDrinkButton)
				OrderDrinkButton->SetButtonImage(Icon);
			break;
		case ERammsArmTask::Drink:
			DrinkIcon = Icon;
			if (DrinkButton)
				DrinkButton->SetButtonImage(Icon);
			break;
		default:
			break;
	}
}

void URammsArmTaskWidget::UpdateTaskButtons()
{
	if (OpenDoorButton)
		OpenDoorButton->SetActive(CurrentTask == ERammsArmTask::OpenDoor);
	if (OrderDrinkButton)
		OrderDrinkButton->SetActive(CurrentTask == ERammsArmTask::OrderDrink);
	if (DrinkButton)
		DrinkButton->SetActive(CurrentTask == ERammsArmTask::Drink);
}

void URammsArmTaskWidget::OnOpenDoorClicked()
{
	HandleTaskButtonClicked(ERammsArmTask::OpenDoor);
}

void URammsArmTaskWidget::OnOrderDrinkClicked()
{
	HandleTaskButtonClicked(ERammsArmTask::OrderDrink);
}

void URammsArmTaskWidget::OnDrinkClicked()
{
	HandleTaskButtonClicked(ERammsArmTask::Drink);
}

void URammsArmTaskWidget::HandleTaskButtonClicked(ERammsArmTask Task)
{
	// Toggle: clicking active task deactivates it
	if (CurrentTask == Task)
	{
		SetTask(ERammsArmTask::None);
	}
	else
	{
		SetTask(Task);
	}

	// Forward to robot controller
	if (ResolvedControllerActor.IsValid())
	{
		IRammsRobotController::Execute_RequestArmTask(ResolvedControllerActor.Get(), CurrentTask);
	}
}

void URammsArmTaskWidget::OnRobotControllerResolved(AActor* ControllerActor)
{
	// Sync current task from the robot
	ERammsArmTask ActualTask = IRammsRobotController::Execute_GetCurrentArmTask(ControllerActor);
	if (ActualTask != CurrentTask)
	{
		CurrentTask = ActualTask;
		UpdateTaskButtons();
	}

	UE_LOG(LogTemp, Log, TEXT("URammsArmTaskWidget: Resolved robot controller '%s'"), *ControllerActor->GetName());
}
