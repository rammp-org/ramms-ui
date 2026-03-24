// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsSeatController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/BorderSlot.h"
#include "Components/SizeBoxSlot.h"
#include "Interfaces/IRammsRobotController.h"

URammsSeatController::URammsSeatController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoFindRobotController = true;
}

void URammsSeatController::ResetCachedWidgets()
{
	PanelBorder = nullptr;
	HeaderText = nullptr;
	MainVBox = nullptr;
	ElevationRow = FRammsSeatAxisRow();
	LateralTiltRow = FRammsSeatAxisRow();
	APTiltRow = FRammsSeatAxisRow();
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

	// Build axis rows
	ElevationRow = BuildAxisRow(TEXT("Elevation"), FText::FromString(TEXT("Elevate")),
		ElevationIcon, ERammsSeatAxis::Elevation);

	LateralTiltRow = BuildAxisRow(TEXT("LateralTilt"), FText::FromString(TEXT("Lateral Tilt")),
		LateralTiltIcon, ERammsSeatAxis::LateralTilt);

	APTiltRow = BuildAxisRow(TEXT("APTilt"), FText::FromString(TEXT("A/P Tilt")),
		AnteriorPosteriorTiltIcon, ERammsSeatAxis::AnteriorPosteriorTilt);
}

FRammsSeatAxisRow URammsSeatController::BuildAxisRow(const FName& RowName, const FText& LabelText,
	UTexture2D* AxisIcon, ERammsSeatAxis Axis)
{
	FRammsSeatAxisRow AxisRow;

	FString Prefix = RowName.ToString();

	// Row container
	AxisRow.Row = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("%sRow"), *Prefix)));

	UVerticalBoxSlot* RowSlot = MainVBox->AddChildToVerticalBox(AxisRow.Row);
	if (RowSlot)
	{
		RowSlot->SetPadding(FMargin(0.0f, 4.0f));
		RowSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Icon
	USizeBox* IconSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), FName(*FString::Printf(TEXT("%sIconBox"), *Prefix)));
	IconSizeBox->SetWidthOverride(IconSize.X);
	IconSizeBox->SetHeightOverride(IconSize.Y);

	AxisRow.Icon = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), FName(*FString::Printf(TEXT("%sIcon"), *Prefix)));
	if (AxisIcon)
	{
		AxisRow.Icon->SetBrushFromTexture(AxisIcon);
	}
	AxisRow.Icon->SetColorAndOpacity(FLinearColor::White);
	IconSizeBox->AddChild(AxisRow.Icon);

	UHorizontalBoxSlot* IconSlot = AxisRow.Row->AddChildToHorizontalBox(IconSizeBox);
	if (IconSlot)
	{
		IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetHorizontalAlignment(HAlign_Left);
	}

	// Label (fixed width for alignment)
	USizeBox* LabelSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), FName(*FString::Printf(TEXT("%sLabelBox"), *Prefix)));
	LabelSizeBox->SetMinDesiredWidth(100.0f);

	AxisRow.Label = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("%sLabel"), *Prefix)));
	AxisRow.Label->SetText(LabelText);
	AxisRow.Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	LabelSizeBox->AddChild(AxisRow.Label);

	UHorizontalBoxSlot* LabelSlot = AxisRow.Row->AddChildToHorizontalBox(LabelSizeBox);
	if (LabelSlot)
	{
		LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Decrease button
	AxisRow.DecreaseButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), FName(*FString::Printf(TEXT("%sDecBtn"), *Prefix)));

	AxisRow.DecreaseLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("%sDecLbl"), *Prefix)));
	AxisRow.DecreaseLabel->SetText(FText::FromString(TEXT("\u2212"))); // minus sign
	AxisRow.DecreaseLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	AxisRow.DecreaseButton->AddChild(AxisRow.DecreaseLabel);

	UHorizontalBoxSlot* DecSlot = AxisRow.Row->AddChildToHorizontalBox(AxisRow.DecreaseButton);
	if (DecSlot)
	{
		DecSlot->SetPadding(FMargin(2.0f, 0.0f));
		DecSlot->SetVerticalAlignment(VAlign_Center);
		DecSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// Value display (fixed width for alignment)
	USizeBox* ValueSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), FName(*FString::Printf(TEXT("%sValBox"), *Prefix)));
	ValueSizeBox->SetMinDesiredWidth(56.0f);

	AxisRow.ValueText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("%sValue"), *Prefix)));
	AxisRow.ValueText->SetText(FText::FromString(TEXT("0.0")));
	AxisRow.ValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	AxisRow.ValueText->SetJustification(ETextJustify::Center);
	ValueSizeBox->AddChild(AxisRow.ValueText);

	UHorizontalBoxSlot* ValSlot = AxisRow.Row->AddChildToHorizontalBox(ValueSizeBox);
	if (ValSlot)
	{
		ValSlot->SetPadding(FMargin(4.0f, 0.0f));
		ValSlot->SetVerticalAlignment(VAlign_Center);
		ValSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// Increase button
	AxisRow.IncreaseButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), FName(*FString::Printf(TEXT("%sIncBtn"), *Prefix)));

	AxisRow.IncreaseLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("%sIncLbl"), *Prefix)));
	AxisRow.IncreaseLabel->SetText(FText::FromString(TEXT("+")));
	AxisRow.IncreaseLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	AxisRow.IncreaseButton->AddChild(AxisRow.IncreaseLabel);

	UHorizontalBoxSlot* IncSlot = AxisRow.Row->AddChildToHorizontalBox(AxisRow.IncreaseButton);
	if (IncSlot)
	{
		IncSlot->SetPadding(FMargin(2.0f, 0.0f));
		IncSlot->SetVerticalAlignment(VAlign_Center);
		IncSlot->SetHorizontalAlignment(HAlign_Center);
	}

	return AxisRow;
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

	// Bind button delegates (AddUniqueDynamic prevents accumulation on reparent)
	if (ElevationRow.DecreaseButton)
		ElevationRow.DecreaseButton->OnClicked.AddUniqueDynamic(this, &URammsSeatController::OnElevationDecrease);
	if (ElevationRow.IncreaseButton)
		ElevationRow.IncreaseButton->OnClicked.AddUniqueDynamic(this, &URammsSeatController::OnElevationIncrease);

	if (LateralTiltRow.DecreaseButton)
		LateralTiltRow.DecreaseButton->OnClicked.AddUniqueDynamic(this, &URammsSeatController::OnLateralTiltDecrease);
	if (LateralTiltRow.IncreaseButton)
		LateralTiltRow.IncreaseButton->OnClicked.AddUniqueDynamic(this, &URammsSeatController::OnLateralTiltIncrease);

	if (APTiltRow.DecreaseButton)
		APTiltRow.DecreaseButton->OnClicked.AddUniqueDynamic(this, &URammsSeatController::OnAPTiltDecrease);
	if (APTiltRow.IncreaseButton)
		APTiltRow.IncreaseButton->OnClicked.AddUniqueDynamic(this, &URammsSeatController::OnAPTiltIncrease);

	UpdateAllValueDisplays();
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

	// Style each axis row
	auto StyleRow = [this](FRammsSeatAxisRow& Row) {
		if (Row.Label)
		{
			Row.Label->SetFont(Style->Typography.Body);
			Row.Label->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
		}
		if (Row.ValueText)
		{
			Row.ValueText->SetFont(Style->Typography.Body);
			Row.ValueText->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
		}
		if (Row.DecreaseLabel)
		{
			Row.DecreaseLabel->SetFont(Style->Typography.HeadingSmall);
			Row.DecreaseLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
		}
		if (Row.IncreaseLabel)
		{
			Row.IncreaseLabel->SetFont(Style->Typography.HeadingSmall);
			Row.IncreaseLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
		}

		// Style buttons
		FButtonStyle BtnStyle;
		BtnStyle.SetNormal(FSlateRoundedBoxBrush(Style->Colors.Surface, Style->Border.CornerRadiusMedium));
		BtnStyle.SetHovered(FSlateRoundedBoxBrush(Style->Colors.Primary * 0.7f, Style->Border.CornerRadiusMedium));
		BtnStyle.SetPressed(FSlateRoundedBoxBrush(Style->Colors.Primary, Style->Border.CornerRadiusMedium));

		if (Row.DecreaseButton)
			Row.DecreaseButton->SetStyle(BtnStyle);
		if (Row.IncreaseButton)
			Row.IncreaseButton->SetStyle(BtnStyle);
	};

	StyleRow(ElevationRow);
	StyleRow(LateralTiltRow);
	StyleRow(APTiltRow);
}

void URammsSeatController::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (HeaderText)
		HeaderText->SetText(HeaderTitle);

	// Push icon changes
	if (ElevationRow.Icon && ElevationIcon)
		ElevationRow.Icon->SetBrushFromTexture(ElevationIcon);
	if (LateralTiltRow.Icon && LateralTiltIcon)
		LateralTiltRow.Icon->SetBrushFromTexture(LateralTiltIcon);
	if (APTiltRow.Icon && AnteriorPosteriorTiltIcon)
		APTiltRow.Icon->SetBrushFromTexture(AnteriorPosteriorTiltIcon);

	UpdateAllValueDisplays();
}

// ── Public API ───────────────────────────────────────────────────

void URammsSeatController::SetAxisValue(ERammsSeatAxis Axis, float Value)
{
	float Min, Max;
	GetAxisLimits(Axis, Min, Max);
	Value = FMath::Clamp(Value, Min, Max);

	float OldValue = CurrentState.GetAxisValue(Axis);
	if (FMath::IsNearlyEqual(OldValue, Value, KINDA_SMALL_NUMBER))
		return;

	CurrentState.SetAxisValue(Axis, Value);
	UpdateValueDisplay(Axis);
	OnValueChanged.Broadcast(Axis, Value);
}

float URammsSeatController::GetAxisValue(ERammsSeatAxis Axis) const
{
	return CurrentState.GetAxisValue(Axis);
}

void URammsSeatController::SetAxisIcon(ERammsSeatAxis Axis, UTexture2D* Icon)
{
	FRammsSeatAxisRow& Row = GetAxisRow(Axis);
	if (Row.Icon && Icon)
	{
		Row.Icon->SetBrushFromTexture(Icon);
	}

	switch (Axis)
	{
		case ERammsSeatAxis::Elevation:
			ElevationIcon = Icon;
			break;
		case ERammsSeatAxis::LateralTilt:
			LateralTiltIcon = Icon;
			break;
		case ERammsSeatAxis::AnteriorPosteriorTilt:
			AnteriorPosteriorTiltIcon = Icon;
			break;
	}
}

// ── Internals ────────────────────────────────────────────────────

void URammsSeatController::UpdateValueDisplay(ERammsSeatAxis Axis)
{
	FRammsSeatAxisRow& Row = GetAxisRow(Axis);
	if (Row.ValueText)
	{
		float Value = CurrentState.GetAxisValue(Axis);
		Row.ValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Value)));
	}
}

void URammsSeatController::UpdateAllValueDisplays()
{
	UpdateValueDisplay(ERammsSeatAxis::Elevation);
	UpdateValueDisplay(ERammsSeatAxis::LateralTilt);
	UpdateValueDisplay(ERammsSeatAxis::AnteriorPosteriorTilt);
}

void URammsSeatController::GetAxisLimits(ERammsSeatAxis Axis, float& OutMin, float& OutMax) const
{
	switch (Axis)
	{
		case ERammsSeatAxis::Elevation:
			OutMin = ElevationMin;
			OutMax = ElevationMax;
			break;
		default:
			OutMin = TiltMin;
			OutMax = TiltMax;
			break;
	}
}

FRammsSeatAxisRow& URammsSeatController::GetAxisRow(ERammsSeatAxis Axis)
{
	switch (Axis)
	{
		case ERammsSeatAxis::LateralTilt:
			return LateralTiltRow;
		case ERammsSeatAxis::AnteriorPosteriorTilt:
			return APTiltRow;
		default:
			return ElevationRow;
	}
}

// ── Button Handlers ──────────────────────────────────────────────

void URammsSeatController::OnElevationDecrease()
{
	HandleAxisAdjust(ERammsSeatAxis::Elevation, -StepSize);
}

void URammsSeatController::OnElevationIncrease()
{
	HandleAxisAdjust(ERammsSeatAxis::Elevation, StepSize);
}

void URammsSeatController::OnLateralTiltDecrease()
{
	HandleAxisAdjust(ERammsSeatAxis::LateralTilt, -StepSize);
}

void URammsSeatController::OnLateralTiltIncrease()
{
	HandleAxisAdjust(ERammsSeatAxis::LateralTilt, StepSize);
}

void URammsSeatController::OnAPTiltDecrease()
{
	HandleAxisAdjust(ERammsSeatAxis::AnteriorPosteriorTilt, -StepSize);
}

void URammsSeatController::OnAPTiltIncrease()
{
	HandleAxisAdjust(ERammsSeatAxis::AnteriorPosteriorTilt, StepSize);
}

void URammsSeatController::HandleAxisAdjust(ERammsSeatAxis Axis, float Delta)
{
	float Current = CurrentState.GetAxisValue(Axis);
	SetAxisValue(Axis, Current + Delta);

	// Forward to robot controller
	if (ResolvedControllerActor.IsValid())
	{
		IRammsRobotController::Execute_RequestSeatAdjust(ResolvedControllerActor.Get(), Axis, Delta);
	}

	// Broadcast via event bus
	if (UWorld* World = GetWorld())
	{
		if (URammsUISubsystem* Subsystem = World->GetSubsystem<URammsUISubsystem>())
		{
			Subsystem->BroadcastSeatStateChanged(Axis, CurrentState.GetAxisValue(Axis));
		}
	}
}

// ── Robot Controller ─────────────────────────────────────────────

void URammsSeatController::OnRobotControllerResolved(AActor* ControllerActor)
{
	// Sync current state from the robot
	FRammsSeatState RobotState;
	if (IRammsRobotController::Execute_GetSeatState(ControllerActor, RobotState))
	{
		CurrentState = RobotState;
		UpdateAllValueDisplays();
	}

	UE_LOG(LogTemp, Log, TEXT("URammsSeatController: Resolved robot controller '%s'"), *ControllerActor->GetName());
}
