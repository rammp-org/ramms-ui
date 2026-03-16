// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsStatusPanel.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

void URammsStatusPanel::ResetCachedWidgets()
{
	PanelBorder = nullptr;
	ToggleButton = nullptr;
	HeaderText = nullptr;
	ContentBox = nullptr;
	SpeedText = nullptr;
	BatteryText = nullptr;
	ModeText = nullptr;
	ConnectionText = nullptr;
	ArmText = nullptr;
}

void URammsStatusPanel::BuildWidgetTree()
{
	if (!WidgetTree || PanelBorder)
		return; // Already built or no tree

	// Root: PanelBorder — transparent background; parent container provides the visual frame
	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetBrushColor(FLinearColor::Transparent);
	PanelBorder->SetPadding(FMargin(12.0f));
	WidgetTree->RootWidget = PanelBorder;

	// Main vertical layout inside border
	UVerticalBox* MainBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainBox"));
	PanelBorder->AddChild(MainBox);

	// Header row: HorizontalBox with HeaderText + ToggleButton
	UHorizontalBox*	  HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderRow"));
	UVerticalBoxSlot* HeaderRowSlot = MainBox->AddChildToVerticalBox(HeaderRow);
	if (HeaderRowSlot)
	{
		HeaderRowSlot->SetHorizontalAlignment(HAlign_Fill);
		HeaderRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HeaderText"));
	HeaderText->SetText(FText::FromString(TEXT("Robot Status")));
	UHorizontalBoxSlot* HeaderTextSlot = HeaderRow->AddChildToHorizontalBox(HeaderText);
	if (HeaderTextSlot)
	{
		HeaderTextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		HeaderTextSlot->SetVerticalAlignment(VAlign_Center);
	}

	ToggleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ToggleButton"));
	UHorizontalBoxSlot* ToggleSlot = HeaderRow->AddChildToHorizontalBox(ToggleButton);
	if (ToggleSlot)
	{
		ToggleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		ToggleSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Toggle button label
	UTextBlock* ToggleLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ToggleLabel"));
	ToggleLabel->SetText(FText::FromString(TEXT("\u25BC")));
	ToggleButton->AddChild(ToggleLabel);

	// Content box with status texts
	ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
	UVerticalBoxSlot* ContentSlot = MainBox->AddChildToVerticalBox(ContentBox);
	if (ContentSlot)
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	// Status text entries
	auto AddStatusText = [this](const FString& Name, const FString& DefaultText) -> UTextBlock* {
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *Name);
		Text->SetText(FText::FromString(DefaultText));
		Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		UVerticalBoxSlot* TextSlot = ContentBox->AddChildToVerticalBox(Text);
		if (TextSlot)
		{
			TextSlot->SetPadding(FMargin(0.0f, 2.0f));
		}
		return Text;
	};

	SpeedText = AddStatusText(TEXT("SpeedText"), TEXT("Speed: --"));
	BatteryText = AddStatusText(TEXT("BatteryText"), TEXT("Battery: --"));
	ModeText = AddStatusText(TEXT("ModeText"), TEXT("Mode: --"));
	ConnectionText = AddStatusText(TEXT("ConnectionText"), TEXT("Connection: --"));
	ArmText = AddStatusText(TEXT("ArmText"), TEXT("Arm: --"));
}

void URammsStatusPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsStatusPanel::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind toggle button
	if (ToggleButton)
	{
		ToggleButton->OnClicked.AddDynamic(this, &URammsStatusPanel::OnToggleClicked);
	}

	// Subscribe to state updates if provider is set
	if (StateProvider.GetInterface())
	{
		IRammsStateProvider* Provider = StateProvider.GetInterface();
		StateUpdateHandle = Provider->OnRobotStateUpdate().AddUObject(this, &URammsStatusPanel::OnRobotStateUpdate);
	}

	// Set initial expand state
	SetExpanded(bIsExpanded, false);
}

void URammsStatusPanel::NativeDestruct()
{
	// Unsubscribe from state updates
	if (StateProvider.GetInterface() && StateUpdateHandle.IsValid())
	{
		StateProvider.GetInterface()->OnRobotStateUpdate().Remove(StateUpdateHandle);
		StateUpdateHandle.Reset();
	}

	Super::NativeDestruct();
}

void URammsStatusPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Periodic update (even without callbacks, poll state provider)
	TimeSinceUpdate += InDeltaTime;
	if (TimeSinceUpdate >= UpdateFrequency)
	{
		TimeSinceUpdate = 0.0f;
		UpdateDisplay();
	}
}

void URammsStatusPanel::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	// Apply border styling — transparent; parent container provides background
	if (PanelBorder)
	{
		PanelBorder->SetBrushColor(FLinearColor::Transparent);
		PanelBorder->SetPadding(FMargin(Style->Spacing.Medium));
	}

	// Apply header text styling
	if (HeaderText)
	{
		HeaderText->SetFont(Style->Typography.HeadingSmall);
		HeaderText->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}

	// Apply status text styling
	auto ApplyStatusStyle = [this](UTextBlock* Text) {
		if (Text)
		{
			Text->SetFont(Style->Typography.Body);
			Text->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
		}
	};

	ApplyStatusStyle(SpeedText);
	ApplyStatusStyle(BatteryText);
	ApplyStatusStyle(ModeText);
	ApplyStatusStyle(ConnectionText);
	ApplyStatusStyle(ArmText);
}

void URammsStatusPanel::SetStateProvider(TScriptInterface<IRammsStateProvider> Provider)
{
	// Unsubscribe from old provider
	if (StateProvider.GetInterface() && StateUpdateHandle.IsValid())
	{
		StateProvider.GetInterface()->OnRobotStateUpdate().Remove(StateUpdateHandle);
		StateUpdateHandle.Reset();
	}

	// Set new provider
	StateProvider = Provider;

	// Subscribe to new provider
	if (StateProvider.GetInterface())
	{
		IRammsStateProvider* ProviderInterface = StateProvider.GetInterface();
		StateUpdateHandle = ProviderInterface->OnRobotStateUpdate().AddUObject(this, &URammsStatusPanel::OnRobotStateUpdate);
	}

	// Force immediate update
	UpdateDisplay();
}

void URammsStatusPanel::ToggleExpand()
{
	SetExpanded(!bIsExpanded, true);
}

void URammsStatusPanel::SetExpanded(bool bExpanded, bool bAnimated)
{
	if (bIsExpanded == bExpanded)
		return;

	bIsExpanded = bExpanded;

	if (ContentBox)
	{
		if (bIsExpanded)
		{
			ContentBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			ContentBox->SetRenderOpacity(1.0f);
		}
		else
		{
			ContentBox->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void URammsStatusPanel::UpdateDisplay()
{
	IRammsStateProvider* Provider = StateProvider.GetInterface();
	if (!Provider)
	{
		// No provider — if remote state was applied, re-apply it; otherwise show placeholder
		if (bHasRemoteState)
		{
			ApplyStateToDisplay(CachedState);
		}
		else
		{
			if (SpeedText)
				SpeedText->SetText(FText::FromString(TEXT("Speed: --")));
			if (BatteryText)
				BatteryText->SetText(FText::FromString(TEXT("Battery: --")));
			if (ModeText)
				ModeText->SetText(FText::FromString(TEXT("Mode: --")));
			if (ConnectionText)
				ConnectionText->SetText(FText::FromString(TEXT("Connection: --")));
			if (ArmText)
				ArmText->SetText(FText::FromString(TEXT("Arm: --")));
		}
		return;
	}

	// Get current state
	FRammsRobotState State;
	if (Provider->GetRobotState(State))
	{
		ApplyStateToDisplay(State);
	}
	else
	{
		// State not available
		FSlateColor DisabledColor = FSlateColor(Style ? Style->Colors.TextDisabled : FLinearColor::Gray);
		if (SpeedText)
		{
			SpeedText->SetText(FText::FromString(TEXT("Speed: No Data")));
			SpeedText->SetColorAndOpacity(DisabledColor);
		}
		if (BatteryText)
		{
			BatteryText->SetText(FText::FromString(TEXT("Battery: No Data")));
			BatteryText->SetColorAndOpacity(DisabledColor);
		}
		if (ModeText)
		{
			ModeText->SetText(FText::FromString(TEXT("Mode: No Data")));
			ModeText->SetColorAndOpacity(DisabledColor);
		}
		if (ConnectionText)
		{
			ConnectionText->SetText(FText::FromString(TEXT("Connection: No Data")));
			ConnectionText->SetColorAndOpacity(DisabledColor);
		}
		if (ArmText)
		{
			ArmText->SetText(FText::FromString(TEXT("Arm: No Data")));
			ArmText->SetColorAndOpacity(DisabledColor);
		}
	}
}

void URammsStatusPanel::OnRobotStateUpdate(const FRammsRobotState& State)
{
	// State was updated via callback - apply directly
	ApplyStateToDisplay(State);
}

void URammsStatusPanel::ApplyRemoteState(const FRammsRobotState& State)
{
	bHasRemoteState = true;
	ApplyStateToDisplay(State);
}

void URammsStatusPanel::ApplyStateToDisplay(const FRammsRobotState& State)
{
	CachedState = State;

	// Update speed
	if (SpeedText)
	{
		float SpeedMagnitude = State.LinearVelocity.Size();
		FText SpeedLabel = FText::Format(
			FText::FromString(TEXT("Speed: {0} m/s")),
			FText::AsNumber(SpeedMagnitude, &FNumberFormattingOptions::DefaultNoGrouping()));
		SpeedText->SetText(SpeedLabel);
		SpeedText->SetColorAndOpacity(FSlateColor(Style ? Style->Colors.TextPrimary : FLinearColor::White));
	}

	// Update battery
	if (BatteryText)
	{
		int32 BatteryPercent = FMath::RoundToInt(State.BatteryLevel * 100.0f);
		FText BatteryLabel = FText::Format(
			FText::FromString(TEXT("Battery: {0}%")),
			FText::AsNumber(BatteryPercent));
		BatteryText->SetText(BatteryLabel);
		BatteryText->SetColorAndOpacity(FSlateColor(GetBatteryColor(State.BatteryLevel)));
	}

	// Update mode
	if (ModeText)
	{
		FString ModeString;
		switch (State.Mode)
		{
			case ERammsRobotMode::Standby:
				ModeString = TEXT("Standby");
				break;
			case ERammsRobotMode::Manual:
				ModeString = TEXT("Manual");
				break;
			case ERammsRobotMode::Autonomous:
				ModeString = TEXT("Autonomous");
				break;
			case ERammsRobotMode::Emergency:
				ModeString = TEXT("EMERGENCY");
				break;
			default:
				ModeString = TEXT("Unknown");
		}

		if (State.bEmergencyStop)
		{
			ModeString += TEXT(" [E-STOP]");
		}

		FText ModeLabel = FText::Format(
			FText::FromString(TEXT("Mode: {0}")),
			FText::FromString(ModeString));
		ModeText->SetText(ModeLabel);
		ModeText->SetColorAndOpacity(FSlateColor(GetModeColor(State.Mode)));
	}
}

void URammsStatusPanel::OnToggleClicked()
{
	ToggleExpand();
}

FLinearColor URammsStatusPanel::GetBatteryColor(float BatteryLevel) const
{
	if (!Style)
		return FLinearColor::White;

	if (BatteryLevel > 0.5f)
		return Style->Colors.Success;
	else if (BatteryLevel > 0.2f)
		return Style->Colors.Warning;
	else
		return Style->Colors.Error;
}

FLinearColor URammsStatusPanel::GetModeColor(ERammsRobotMode Mode) const
{
	if (!Style)
		return FLinearColor::White;

	switch (Mode)
	{
		case ERammsRobotMode::Standby:
			return Style->Colors.TextSecondary;
		case ERammsRobotMode::Manual:
			return Style->Colors.Info;
		case ERammsRobotMode::Autonomous:
			return Style->Colors.Success;
		case ERammsRobotMode::Emergency:
			return Style->Colors.Error;
		default:
			return Style->Colors.TextDisabled;
	}
}
