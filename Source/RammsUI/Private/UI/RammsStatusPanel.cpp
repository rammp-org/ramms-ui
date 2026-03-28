// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsStatusPanel.h"
#include "RammsUISubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

// ── Constructor ────────────────────────────────────────────────────────

URammsStatusPanel::URammsStatusPanel()
{
	// Default fields — backward-compatible with original fixed layout
	{
		FRammsStatusField F;
		F.Key = TEXT("Speed");
		F.Label = FText::FromString(TEXT("Speed"));
		F.Source = ERammsStatusFieldSource::RobotState;
		F.Format = ERammsStatusValueFormat::Float;
		F.Units = FText::FromString(TEXT("m/s"));
		F.DecimalPlaces = 1;
		Fields.Add(MoveTemp(F));
	}
	{
		FRammsStatusField F;
		F.Key = TEXT("Battery");
		F.Label = FText::FromString(TEXT("Battery"));
		F.Source = ERammsStatusFieldSource::RobotState;
		F.Format = ERammsStatusValueFormat::Percent;
		F.bUseThresholdColors = true;
		F.WarningThreshold = 0.5f;
		F.CriticalThreshold = 0.2f;
		Fields.Add(MoveTemp(F));
	}
	{
		FRammsStatusField F;
		F.Key = TEXT("Mode");
		F.Label = FText::FromString(TEXT("Mode"));
		F.Source = ERammsStatusFieldSource::RobotState;
		F.Format = ERammsStatusValueFormat::EnumName;
		F.EnumType = StaticEnum<ERammsRobotMode>();
		Fields.Add(MoveTemp(F));
	}
}

// ── Widget Tree ────────────────────────────────────────────────────────

void URammsStatusPanel::ResetCachedWidgets()
{
	PanelBorder = nullptr;
	ToggleButton = nullptr;
	HeaderText = nullptr;
	HeaderRow = nullptr;
	ContentBox = nullptr;
	FieldWidgets.Empty();
}

void URammsStatusPanel::BuildWidgetTree()
{
	if (!WidgetTree || PanelBorder)
		return;

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetBrushColor(FLinearColor::Transparent);
	PanelBorder->SetPadding(FMargin(12.0f));
	WidgetTree->RootWidget = PanelBorder;

	UVerticalBox* MainBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainBox"));
	PanelBorder->AddChild(MainBox);

	// Header
	HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderRow"));
	if (UVerticalBoxSlot* HeaderRowSlot = MainBox->AddChildToVerticalBox(HeaderRow))
	{
		HeaderRowSlot->SetHorizontalAlignment(HAlign_Fill);
		HeaderRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HeaderText"));
	HeaderText->SetText(HeaderTitle);
	if (UHorizontalBoxSlot* HeaderTextSlot = HeaderRow->AddChildToHorizontalBox(HeaderText))
	{
		HeaderTextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		HeaderTextSlot->SetVerticalAlignment(VAlign_Center);
	}

	ToggleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ToggleButton"));
	if (UHorizontalBoxSlot* ToggleSlot = HeaderRow->AddChildToHorizontalBox(ToggleButton))
	{
		ToggleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		ToggleSlot->SetVerticalAlignment(VAlign_Center);
	}

	UTextBlock* ToggleLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ToggleLabel"));
	ToggleLabel->SetText(FText::FromString(TEXT("\u25BC")));
	ToggleButton->AddChild(ToggleLabel);

	// Content
	ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
	if (UVerticalBoxSlot* ContentSlot = MainBox->AddChildToVerticalBox(ContentBox))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	BuildFieldRows();
}

void URammsStatusPanel::BuildFieldRows()
{
	if (!ContentBox || !WidgetTree)
		return;

	FieldWidgets.Empty();

	for (int32 i = 0; i < Fields.Num(); ++i)
	{
		FString		Name = FString::Printf(TEXT("FieldText_%d"), i);
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *Name);
		Text->SetText(FText::Format(
			FText::FromString(TEXT("{0}: --")),
			Fields[i].Label));
		Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		if (UVerticalBoxSlot* FieldSlot = ContentBox->AddChildToVerticalBox(Text))
		{
			FieldSlot->SetPadding(FMargin(0.0f, 2.0f));
		}
		FieldWidgets.Add(Text);
	}
}

// ── Lifecycle ──────────────────────────────────────────────────────────

void URammsStatusPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsStatusPanel::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (HeaderText)
	{
		HeaderText->SetText(HeaderTitle);
		HeaderText->SetVisibility(HeaderTitle.IsEmptyOrWhitespace()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::SelfHitTestInvisible);
	}

	// Rebuild rows if field count changed (designer editing)
	if (ContentBox && WidgetTree && FieldWidgets.Num() != Fields.Num())
	{
		for (UTextBlock* W : FieldWidgets)
		{
			if (W)
			{
				ContentBox->RemoveChild(W);
			}
		}
		BuildFieldRows();
		ApplyStyle_Implementation();
	}

	RefreshAllFields();
}

void URammsStatusPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (ToggleButton)
	{
		ToggleButton->OnClicked.AddUniqueDynamic(this, &URammsStatusPanel::OnToggleClicked);
	}

	// State provider subscription
	if (StateProvider.GetInterface())
	{
		StateUpdateHandle = StateProvider.GetInterface()->OnRobotStateUpdate().AddUObject(
			this, &URammsStatusPanel::OnRobotStateUpdate);
	}

	// Subsystem subscriptions
	if (UWorld* World = GetWorld())
	{
		if (URammsUISubsystem* Sub = World->GetSubsystem<URammsUISubsystem>())
		{
			Sub->OnRobotStateChanged.AddUniqueDynamic(this, &URammsStatusPanel::OnSubsystemStateChanged);
			Sub->OnPropertyChanged.AddUniqueDynamic(this, &URammsStatusPanel::OnSubsystemPropertyChanged);

			if (Sub->HasRobotState())
			{
				ApplyRemoteState(Sub->GetCachedRobotState());
			}
		}
	}

	SetExpanded(bIsExpanded, false);
}

void URammsStatusPanel::NativeDestruct()
{
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

	TimeSinceUpdate += InDeltaTime;
	if (TimeSinceUpdate >= UpdateFrequency)
	{
		TimeSinceUpdate = 0.0f;
		UpdateDisplay();
	}
}

// ── Styling ────────────────────────────────────────────────────────────

void URammsStatusPanel::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	if (PanelBorder)
	{
		PanelBorder->SetBrushColor(FLinearColor::Transparent);
		PanelBorder->SetPadding(FMargin(Style->Spacing.Medium));
	}

	if (HeaderText)
	{
		HeaderText->SetFont(Style->Typography.HeadingSmall);
		HeaderText->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}

	for (UTextBlock* W : FieldWidgets)
	{
		if (W)
		{
			W->SetFont(Style->Typography.Body);
		}
	}
}

// ── State Provider ───────────────────────────────────────────────────────

void URammsStatusPanel::SetStateProvider(TScriptInterface<IRammsStateProvider> Provider)
{
	if (StateProvider.GetInterface() && StateUpdateHandle.IsValid())
	{
		StateProvider.GetInterface()->OnRobotStateUpdate().Remove(StateUpdateHandle);
		StateUpdateHandle.Reset();
	}

	StateProvider = Provider;

	if (StateProvider.GetInterface())
	{
		StateUpdateHandle = StateProvider.GetInterface()->OnRobotStateUpdate().AddUObject(
			this, &URammsStatusPanel::OnRobotStateUpdate);
	}

	UpdateDisplay();
}

// ── Expand / Collapse ────────────────────────────────────────────────────

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
		ContentBox->SetVisibility(bIsExpanded
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
	}
}

// ── Display Update ───────────────────────────────────────────────────────

void URammsStatusPanel::UpdateDisplay()
{
	IRammsStateProvider* Provider = StateProvider.GetInterface();
	if (Provider)
	{
		FRammsRobotState State;
		if (Provider->GetRobotState(State))
		{
			CachedState = State;
			bHasRemoteState = true;
		}
	}

	RefreshAllFields();
}

void URammsStatusPanel::ApplyRemoteState(const FRammsRobotState& State)
{
	bHasRemoteState = true;
	CachedState = State;
	RefreshAllFields();
}

void URammsStatusPanel::SetCustomFieldValue(FName Key, const FString& Value)
{
	CustomFieldValues.Add(Key, Value);
	RefreshAllFields();
}

void URammsStatusPanel::OnRobotStateUpdate(const FRammsRobotState& State)
{
	CachedState = State;
	bHasRemoteState = true;
	RefreshAllFields();
}

void URammsStatusPanel::OnSubsystemStateChanged(const FRammsRobotState& State)
{
	bHasRemoteState = true;
	CachedState = State;
	RefreshAllFields();
}

void URammsStatusPanel::OnSubsystemPropertyChanged(FName Key, const FString& Value)
{
	// Only refresh if a displayed field cares about this key
	for (const FRammsStatusField& Field : Fields)
	{
		if (Field.Source == ERammsStatusFieldSource::PropertyStore && Field.Key == Key)
		{
			RefreshAllFields();
			return;
		}
	}
}

void URammsStatusPanel::OnToggleClicked()
{
	ToggleExpand();
}

// ── Field Resolution & Formatting ────────────────────────────────────────────

void URammsStatusPanel::RefreshAllFields()
{
	if (FieldWidgets.Num() != Fields.Num())
		return;

	URammsUISubsystem* Sub = nullptr;
	if (UWorld* World = GetWorld())
	{
		Sub = World->GetSubsystem<URammsUISubsystem>();
	}

	static const FName SpeedKey(TEXT("Speed"));
	static const FName BatteryKey(TEXT("Battery"));
	static const FName ModeKey(TEXT("Mode"));
	static const FName EmergencyStopKey(TEXT("EmergencyStop"));
	static const FName LinearVelocityKey(TEXT("LinearVelocity"));
	static const FName AngularVelocityKey(TEXT("AngularVelocity"));

	const FLinearColor DefaultColor = Style ? Style->Colors.TextPrimary : FLinearColor::White;
	const FLinearColor DisabledColor = Style ? Style->Colors.TextDisabled : FLinearColor::Gray;

	for (int32 i = 0; i < Fields.Num(); ++i)
	{
		const FRammsStatusField& Field = Fields[i];
		UTextBlock*				 Widget = FieldWidgets[i];
		if (!Widget)
			continue;

		// ── Resolve raw value ────────────────────────────────────────────
		float	NumericValue = 0.0f;
		FString StringValue;
		bool	bIsNumeric = false;
		bool	bIsValid = false;

		switch (Field.Source)
		{
			case ERammsStatusFieldSource::RobotState:
			{
				if (!bHasRemoteState)
					break;

				bIsValid = true;
				if (Field.Key == SpeedKey)
				{
					NumericValue = CachedState.LinearVelocity.Size();
					bIsNumeric = true;
				}
				else if (Field.Key == BatteryKey)
				{
					NumericValue = CachedState.BatteryLevel;
					bIsNumeric = true;
				}
				else if (Field.Key == ModeKey)
				{
					NumericValue = static_cast<float>(static_cast<uint8>(CachedState.Mode));
					bIsNumeric = true;
				}
				else if (Field.Key == EmergencyStopKey)
				{
					NumericValue = CachedState.bEmergencyStop ? 1.0f : 0.0f;
					StringValue = CachedState.bEmergencyStop ? TEXT("ACTIVE") : TEXT("Inactive");
					bIsNumeric = true;
				}
				else if (Field.Key == LinearVelocityKey)
				{
					StringValue = CachedState.LinearVelocity.ToString();
				}
				else if (Field.Key == AngularVelocityKey)
				{
					StringValue = CachedState.AngularVelocity.ToString();
				}
				else
				{
					bIsValid = false;
				}
				break;
			}
			case ERammsStatusFieldSource::PropertyStore:
			{
				if (Sub && Sub->HasProperty(Field.Key))
				{
					StringValue = Sub->GetProperty(Field.Key);
					if (StringValue.IsNumeric())
					{
						NumericValue = FCString::Atof(*StringValue);
						bIsNumeric = true;
					}
					bIsValid = true;
				}
				break;
			}
			case ERammsStatusFieldSource::Custom:
			{
				if (const FString* Val = CustomFieldValues.Find(Field.Key))
				{
					StringValue = *Val;
					if (StringValue.IsNumeric())
					{
						NumericValue = FCString::Atof(*StringValue);
						bIsNumeric = true;
					}
					bIsValid = true;
				}
				break;
			}
		}

		// ── Format value ───────────────────────────────────────────────
		FText FormattedValue;
		if (!bIsValid)
		{
			FormattedValue = FText::FromString(TEXT("--"));
		}
		else
		{
			switch (Field.Format)
			{
				case ERammsStatusValueFormat::Float:
				{
					FNumberFormattingOptions Opts = FNumberFormattingOptions::DefaultNoGrouping();
					Opts.MinimumFractionalDigits = Field.DecimalPlaces;
					Opts.MaximumFractionalDigits = Field.DecimalPlaces;
					FText Num = FText::AsNumber(bIsNumeric ? NumericValue : 0.0f, &Opts);
					FormattedValue = Field.Units.IsEmpty()
						? Num
						: FText::Format(FText::FromString(TEXT("{0} {1}")), Num, Field.Units);
					break;
				}
				case ERammsStatusValueFormat::Integer:
				{
					FText Num = FText::AsNumber(FMath::RoundToInt(bIsNumeric ? NumericValue : 0.0f));
					FormattedValue = Field.Units.IsEmpty()
						? Num
						: FText::Format(FText::FromString(TEXT("{0} {1}")), Num, Field.Units);
					break;
				}
				case ERammsStatusValueFormat::Percent:
				{
					int32 Pct = FMath::RoundToInt((bIsNumeric ? NumericValue : 0.0f) * 100.0f);
					FormattedValue = FText::Format(
						FText::FromString(TEXT("{0}%")),
						FText::AsNumber(Pct));
					break;
				}
				case ERammsStatusValueFormat::Boolean:
					FormattedValue = FText::FromString(
						(bIsNumeric ? (NumericValue > 0.5f) : false) ? TEXT("Yes") : TEXT("No"));
					break;
				case ERammsStatusValueFormat::EnumName:
				{
					if (Field.EnumType && bIsNumeric)
					{
						int64 IntVal = static_cast<int64>(FMath::RoundToInt(NumericValue));
						FormattedValue = Field.EnumType->GetDisplayNameTextByValue(IntVal);
					}
					else
					{
						FormattedValue = FText::FromString(StringValue.IsEmpty() ? TEXT("--") : StringValue);
					}
					break;
				}
				case ERammsStatusValueFormat::RawText:
					FormattedValue = FText::FromString(StringValue);
					break;
				case ERammsStatusValueFormat::Auto:
				default:
				{
					if (bIsNumeric)
					{
						FNumberFormattingOptions Opts = FNumberFormattingOptions::DefaultNoGrouping();
						Opts.MinimumFractionalDigits = Field.DecimalPlaces;
						Opts.MaximumFractionalDigits = Field.DecimalPlaces;
						FText Num = FText::AsNumber(NumericValue, &Opts);
						FormattedValue = Field.Units.IsEmpty()
							? Num
							: FText::Format(FText::FromString(TEXT("{0} {1}")), Num, Field.Units);
					}
					else
					{
						FormattedValue = FText::FromString(StringValue.IsEmpty() ? TEXT("--") : StringValue);
					}
					break;
				}
			}
		}

		// Combine label + value
		Widget->SetText(FText::Format(
			FText::FromString(TEXT("{0}: {1}")),
			Field.Label,
			FormattedValue));

		// ── Determine color ──────────────────────────────────────────────
		FLinearColor Color = DefaultColor;

		if (!bIsValid)
		{
			Color = DisabledColor;
		}
		else if (Field.Key == ModeKey && Field.Source == ERammsStatusFieldSource::RobotState)
		{
			Color = GetModeColor(CachedState.Mode);
		}
		else if (Field.bUseThresholdColors && bIsNumeric)
		{
			if (NumericValue <= Field.CriticalThreshold)
				Color = Style ? Style->Colors.Error : FLinearColor::Red;
			else if (NumericValue <= Field.WarningThreshold)
				Color = Style ? Style->Colors.Warning : FLinearColor::Yellow;
			else
				Color = Style ? Style->Colors.Success : FLinearColor::Green;
		}

		Widget->SetColorAndOpacity(FSlateColor(Color));
	}
}

// ── Color Helpers ──────────────────────────────────────────────────────────

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
