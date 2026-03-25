// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsSeatController.h"
#include "UI/RammsAxisControl.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/BorderSlot.h"
#include "Interfaces/IRammsRobotController.h"

URammsSeatController::URammsSeatController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoFindRobotController = true;

	ElevationConfig.Label = FText::FromString(TEXT("Elevate"));
	ElevationConfig.MinValue = 0.0f;
	ElevationConfig.MaxValue = 1.0f;
	ElevationConfig.DefaultValue = 0.0f;

	LateralTiltConfig.Label = FText::FromString(TEXT("Lateral Tilt"));
	LateralTiltConfig.MinValue = -1.0f;
	LateralTiltConfig.MaxValue = 1.0f;
	LateralTiltConfig.DefaultValue = 0.0f;

	APTiltConfig.Label = FText::FromString(TEXT("A/P Tilt"));
	APTiltConfig.MinValue = -1.0f;
	APTiltConfig.MaxValue = 1.0f;
	APTiltConfig.DefaultValue = 0.0f;
}

void URammsSeatController::ResetCachedWidgets()
{
	PanelBorder = nullptr;
	HeaderText = nullptr;
	MainVBox = nullptr;
	// Axis controls are child UUserWidgets — they manage themselves.
	// Clear our references so BuildWidgetTree can recreate them.
	ElevationControl = nullptr;
	LateralTiltControl = nullptr;
	APTiltControl = nullptr;
}

// ── Widget Tree ──────────────────────────────────────────────────

void URammsSeatController::BuildWidgetTree()
{
	if (!WidgetTree || PanelBorder)
		return;

	// Root border
	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetBrushColor(FLinearColor::Transparent);
	PanelBorder->SetPadding(FMargin(12.0f));
	WidgetTree->RootWidget = PanelBorder;

	// Vertical layout
	MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainVBox"));
	PanelBorder->AddChild(MainVBox);

	// Header
	HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HeaderText"));
	HeaderText->SetText(HeaderTitle);
	HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UVerticalBoxSlot* HeaderSlot = MainVBox->AddChildToVerticalBox(HeaderText);
	if (HeaderSlot)
	{
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		HeaderSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// Create axis controls as child widgets
	auto CreateAxisControl = [this](const FRammsAxisConfig& Cfg) -> URammsAxisControl* {
		// Prefer PlayerController for proper focus/navigation/input handling;
		// fall back to this (e.g. in designer where there's no player)
		APlayerController* PC = GetOwningPlayer();
		URammsAxisControl* Ctrl = PC
			? CreateWidget<URammsAxisControl>(PC)
			: CreateWidget<URammsAxisControl>(this);
		if (Ctrl)
		{
			Ctrl->ApplyConfig(Cfg);
			UVerticalBoxSlot* Slot = MainVBox->AddChildToVerticalBox(Ctrl);
			if (Slot)
			{
				Slot->SetPadding(FMargin(0.0f, 4.0f));
				Slot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
		return Ctrl;
	};

	ElevationControl = CreateAxisControl(ElevationConfig);
	LateralTiltControl = CreateAxisControl(LateralTiltConfig);
	APTiltControl = CreateAxisControl(APTiltConfig);
}

// ── Lifecycle ────────────────────────────────────────────────────

void URammsSeatController::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsSeatController::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind axis control delegates (AddUniqueDynamic prevents accumulation on reparent)
	if (ElevationControl)
		ElevationControl->OnValueChanged.AddUniqueDynamic(this, &URammsSeatController::OnElevationChanged);
	if (LateralTiltControl)
		LateralTiltControl->OnValueChanged.AddUniqueDynamic(this, &URammsSeatController::OnLateralTiltChanged);
	if (APTiltControl)
		APTiltControl->OnValueChanged.AddUniqueDynamic(this, &URammsSeatController::OnAPTiltChanged);
}

void URammsSeatController::ApplyStyle_Implementation()
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

	// Propagate style to axis controls (they are child UUserWidgets,
	// not in our WidgetTree, so PropagateStyleToChildren won't reach them)
	if (ElevationControl)
		ElevationControl->SetStyle(Style);
	if (LateralTiltControl)
		LateralTiltControl->SetStyle(Style);
	if (APTiltControl)
		APTiltControl->SetStyle(Style);
}

void URammsSeatController::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (HeaderText)
	{
		HeaderText->SetText(HeaderTitle);
		HeaderText->SetVisibility(HeaderTitle.IsEmptyOrWhitespace()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::SelfHitTestInvisible);
	}

	// Push updated configs to axis controls
	if (ElevationControl)
		ElevationControl->ApplyConfig(ElevationConfig);
	if (LateralTiltControl)
		LateralTiltControl->ApplyConfig(LateralTiltConfig);
	if (APTiltControl)
		APTiltControl->ApplyConfig(APTiltConfig);
}

// ── Public API ───────────────────────────────────────────────────

void URammsSeatController::SetAxisValue(ERammsSeatAxis Axis, float Value)
{
	FRammsAxisConfig& Cfg = GetAxisConfig(Axis);
	Value = FMath::Clamp(Value, Cfg.MinValue, Cfg.MaxValue);

	float OldValue = CurrentState.GetAxisValue(Axis);
	if (FMath::IsNearlyEqual(OldValue, Value, KINDA_SMALL_NUMBER))
		return;

	CurrentState.SetAxisValue(Axis, Value);

	// Update the axis control display (SetValue does NOT fire OnValueChanged)
	if (URammsAxisControl* Ctrl = GetAxisControl(Axis))
	{
		Ctrl->SetValue(Value);
	}

	OnValueChanged.Broadcast(Axis, Value);
}

float URammsSeatController::GetAxisValue(ERammsSeatAxis Axis) const
{
	return CurrentState.GetAxisValue(Axis);
}

void URammsSeatController::SetAxisIcon(ERammsSeatAxis Axis, UTexture2D* Icon)
{
	GetAxisConfig(Axis).Icon = Icon;
	if (URammsAxisControl* Ctrl = GetAxisControl(Axis))
	{
		Ctrl->SetIcon(Icon);
	}
}

void URammsSeatController::ResetAxis(ERammsSeatAxis Axis)
{
	// ResetToDefault fires the axis control's OnValueChanged,
	// which triggers HandleAxisChanged → robot forward + broadcast
	if (URammsAxisControl* Ctrl = GetAxisControl(Axis))
	{
		Ctrl->ResetToDefault();
	}
}

void URammsSeatController::ResetAllAxes()
{
	ResetAxis(ERammsSeatAxis::Elevation);
	ResetAxis(ERammsSeatAxis::LateralTilt);
	ResetAxis(ERammsSeatAxis::AnteriorPosteriorTilt);
}

// ── Internal ─────────────────────────────────────────────────────

URammsAxisControl* URammsSeatController::GetAxisControl(ERammsSeatAxis Axis) const
{
	switch (Axis)
	{
		case ERammsSeatAxis::Elevation:
			return ElevationControl;
		case ERammsSeatAxis::LateralTilt:
			return LateralTiltControl;
		case ERammsSeatAxis::AnteriorPosteriorTilt:
			return APTiltControl;
		default:
			return nullptr;
	}
}

FRammsAxisConfig& URammsSeatController::GetAxisConfig(ERammsSeatAxis Axis)
{
	switch (Axis)
	{
		case ERammsSeatAxis::LateralTilt:
			return LateralTiltConfig;
		case ERammsSeatAxis::AnteriorPosteriorTilt:
			return APTiltConfig;
		default:
			return ElevationConfig;
	}
}

// ── Axis Value Changed Handlers ──────────────────────────────────

void URammsSeatController::OnElevationChanged(float Value)
{
	HandleAxisChanged(ERammsSeatAxis::Elevation, Value);
}

void URammsSeatController::OnLateralTiltChanged(float Value)
{
	HandleAxisChanged(ERammsSeatAxis::LateralTilt, Value);
}

void URammsSeatController::OnAPTiltChanged(float Value)
{
	HandleAxisChanged(ERammsSeatAxis::AnteriorPosteriorTilt, Value);
}

void URammsSeatController::HandleAxisChanged(ERammsSeatAxis Axis, float Value)
{
	float OldValue = CurrentState.GetAxisValue(Axis);
	CurrentState.SetAxisValue(Axis, Value);

	// Broadcast seat-level delegate
	OnValueChanged.Broadcast(Axis, Value);

	// Forward delta to robot controller
	float Delta = Value - OldValue;
	if (ResolvedControllerActor.IsValid() && !FMath::IsNearlyZero(Delta))
	{
		IRammsRobotController::Execute_RequestSeatAdjust(ResolvedControllerActor.Get(), Axis, Delta);
	}

	// Broadcast via event bus
	if (UWorld* World = GetWorld())
	{
		if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
		{
			Subsystem->BroadcastSeatStateChanged(Axis, Value);
		}
	}
}

// ── Robot Controller ─────────────────────────────────────────────

void URammsSeatController::OnRobotControllerResolved(AActor* ControllerActor)
{
	FRammsSeatState RobotState;
	if (IRammsRobotController::Execute_GetSeatState(ControllerActor, RobotState))
	{
		CurrentState = RobotState;

		// Sync axis controls to robot state (programmatic, no delegate fire)
		if (ElevationControl)
			ElevationControl->SetValue(CurrentState.Elevation);
		if (LateralTiltControl)
			LateralTiltControl->SetValue(CurrentState.LateralTilt);
		if (APTiltControl)
			APTiltControl->SetValue(CurrentState.AnteriorPosteriorTilt);
	}

	UE_LOG(LogTemp, Log, TEXT("URammsSeatController: Resolved robot controller '%s'"), *ControllerActor->GetName());
}
