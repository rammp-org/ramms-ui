// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsControlSurfacePanel.h"
#include "UI/RammsCollapsibleContainer.h"
#include "UI/RammsControlRow.h"
#include "UI/RammsSurfaceJoystick.h"
#include "UI/RammsUIStyle.h"
#include "RammsControlSink.h"
#include "RammsControlSurfaceProvider.h"
#include "RammsUISubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void URammsControlSurfacePanel::BuildWidgetTree()
{
	if (!WidgetTree || PanelBorder)
	{
		return;
	}
	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(FLinearColor(0.06f, 0.06f, 0.08f, 0.9f), 4.0f, FLinearColor(0.3f, 0.3f, 0.3f, 1.0f), 1.0f);
	PanelBorder->SetPadding(FMargin(8.0f));
	WidgetTree->RootWidget = PanelBorder;

	USizeBox* WidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("WidthBox"));
	WidthBox->SetMinDesiredWidth(MinPanelWidth);
	PanelBorder->AddChild(WidthBox);

	MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainVBox"));
	WidthBox->AddChild(MainVBox);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("No robot")));
	UVerticalBoxSlot* TitleSlot = MainVBox->AddChildToVerticalBox(TitleText);
	if (TitleSlot)
	{
		TitleSlot->SetPadding(FMargin(4.0f, 2.0f, 4.0f, 6.0f));
	}

	GroupsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("GroupsScroll"));
	GroupsScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Never); // the panel handles the wheel itself (NativeOnMouseWheel)
	UVerticalBoxSlot* ScrollSlot = MainVBox->AddChildToVerticalBox(GroupsScroll);
	if (ScrollSlot)
	{
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	GroupsVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GroupsVBox"));
	GroupsScroll->AddChild(GroupsVBox);
}

void URammsControlSurfacePanel::ResetCachedWidgets()
{
	PanelBorder = nullptr;
	MainVBox = nullptr;
	TitleText = nullptr;
	GroupsScroll = nullptr;
	GroupsVBox = nullptr;
	Groups.Reset();
	Rows.Reset();
	Joysticks.Reset();
	BuiltVersion = -1;
}

UWidget* URammsControlSurfacePanel::GetRootWidgetForValidation()
{
	return PanelBorder;
}

void URammsControlSurfacePanel::NativeConstruct()
{
	Super::NativeConstruct();
	if (!bSubscribed)
	{
		if (UWorld* World = GetWorld())
		{
			if (URammsUISubsystem* UI = World->GetSubsystem<URammsUISubsystem>())
			{
				UI->OnControlSurfaceRegistryChanged.AddUniqueDynamic(this, &URammsControlSurfacePanel::OnRegistryChanged);
				bSubscribed = true;
			}
		}
	}
	ResolveSurface();
	Rebuild();
}

void URammsControlSurfacePanel::NativeDestruct()
{
	if (bSubscribed)
	{
		if (UWorld* World = GetWorld())
		{
			if (URammsUISubsystem* UI = World->GetSubsystem<URammsUISubsystem>())
			{
				UI->OnControlSurfaceRegistryChanged.RemoveDynamic(this, &URammsControlSurfacePanel::OnRegistryChanged);
			}
		}
		bSubscribed = false;
	}
	Super::NativeDestruct();
}

void URammsControlSurfacePanel::OnRegistryChanged(UObject* Provider, bool bRegistered)
{
	if (!bRegistered && Provider == TargetSurface)
	{
		TargetSurface = nullptr;
	}
	if (!TargetSurface && bAutoFindSurface)
	{
		ResolveSurface();
		Rebuild();
	}
}

bool URammsControlSurfacePanel::ResolveSurface()
{
	auto IsSurface = [](UObject* O) {
		return O && O->GetClass()->ImplementsInterface(URammsControlSurfaceProvider::StaticClass())
			&& O->GetClass()->ImplementsInterface(URammsControlSink::StaticClass());
	};
	if (IsSurface(TargetSurface))
	{
		return true;
	}
	TargetSurface = nullptr;
	if (!bAutoFindSurface)
	{
		return false;
	}
	UWorld*			   World = GetWorld();
	URammsUISubsystem* UI = World ? World->GetSubsystem<URammsUISubsystem>() : nullptr;
	if (!UI)
	{
		return false;
	}
	UObject* Found = TargetRobotName.IsEmpty() ? nullptr : UI->FindControlSurfaceByRobotName(TargetRobotName);
	if (!Found)
	{
		Found = UI->FindControlSurface();
	}
	if (IsSurface(Found))
	{
		TargetSurface = Found;
		return true;
	}
	return false;
}

void URammsControlSurfacePanel::SetTargetSurface(UObject* Surface)
{
	TargetSurface = Surface;
	BuiltVersion = -1;
	Rebuild();
}

void URammsControlSurfacePanel::Rebuild()
{
	if (!WidgetTree || !GroupsVBox)
	{
		return;
	}
	GroupsVBox->ClearChildren();
	Groups.Reset();
	Rows.Reset();
	Joysticks.Reset();

	if (!ResolveSurface())
	{
		if (TitleText)
		{
			TitleText->SetText(FText::FromString(TEXT("No robot")));
		}
		BuiltVersion = -1;
		return;
	}

	const FRammsControlSurface Surface = IRammsControlSurfaceProvider::Execute_GetControlSurface(TargetSurface);
	BuiltVersion = IRammsControlSurfaceProvider::Execute_GetControlSurfaceVersion(TargetSurface);
	if (TitleText)
	{
		TitleText->SetText(Surface.RobotName.IsEmpty() ? FText::FromString(TargetSurface->GetName()) : Surface.RobotName);
		TitleText->SetVisibility(bShowRobotName ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	// Groups in the surface's order; axes inside by Order.
	TArray<FName> GroupOrder = Surface.Groups;
	for (const FRammsControlAxis& Axis : Surface.Axes)
	{
		GroupOrder.AddUnique(Axis.Group);
	}
	for (const FName& Group : GroupOrder)
	{
		TArray<FRammsControlAxis> Axes;
		Surface.GetAxesInGroup(Group, Axes);
		if (Axes.Num() == 0)
		{
			continue;
		}
		Axes.StableSort([](const FRammsControlAxis& A, const FRammsControlAxis& B) { return A.Order < B.Order; });
		BuildGroup(Group, Axes);
	}
	ApplyStyle();
}

void URammsControlSurfacePanel::BuildGroup(FName Group, const TArray<FRammsControlAxis>& Axes)
{
	URammsCollapsibleContainer* Container = CreateWidget<URammsCollapsibleContainer>(this);
	Container->SetHeaderTitle(FText::FromName(Group));
	Container->SetMaxContentHeight(0.0f); // size to content; the panel's own ScrollBox scrolls
	Container->SetWheelConsumption(EConsumeMouseWheel::Never); // the wheel bubbles up to the panel, which scrolls the list
	UVerticalBoxSlot* GroupSlot = GroupsVBox->AddChildToVerticalBox(Container);
	if (GroupSlot)
	{
		GroupSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}
	Groups.Add(Container);

	TSet<FName> Consumed;
	for (const FRammsControlAxis& Axis : Axes)
	{
		if (Consumed.Contains(Axis.Id))
		{
			continue;
		}
		// A pair of Continuous axes -> joystick (the lower-Order axis is vertical).
		if (Axis.Kind == ERammsControlKind::Continuous && !Axis.PairedAxis.IsNone())
		{
			const FRammsControlAxis* Pair = Axes.FindByPredicate([&Axis](const FRammsControlAxis& A) { return A.Id == Axis.PairedAxis; });
			if (Pair && Pair->Kind == ERammsControlKind::Continuous && !Consumed.Contains(Pair->Id))
			{
				const FRammsControlAxis& Vertical = (Axis.Order <= Pair->Order) ? Axis : *Pair;
				const FRammsControlAxis& Horizontal = (&Vertical == &Axis) ? *Pair : Axis;
				URammsSurfaceJoystick*	 Joystick = CreateWidget<URammsSurfaceJoystick>(this);
				Joystick->bAutoFindSink = false;
				Joystick->SetRadii(JoystickRadius, JoystickRadius * 0.35f);
				Joystick->Source = Source;
				Joystick->SetTarget(TargetSurface, Horizontal.Id, Vertical.Id);

				UTextBlock* Caption = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				Caption->SetText(FText::Format(NSLOCTEXT("RammsUI", "JoystickCaption", "{0} / {1}"), Vertical.DisplayName, Horizontal.DisplayName));
				Caption->SetJustification(ETextJustify::Center);
				Container->AddContentChild(Caption);
				USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				Box->SetHeightOverride(JoystickRadius * 2.0f + 8.0f);
				Box->AddChild(Joystick);
				Container->AddContentChild(Box);
				Joysticks.Add(Joystick);
				Consumed.Add(Axis.Id);
				Consumed.Add(Pair->Id);
				continue;
			}
		}
		URammsControlRow* Row = CreateWidget<URammsControlRow>(this);
		Row->Setup(TargetSurface, Axis, Source);
		Container->AddContentChild(Row);
		Rows.Add(Row);
		Consumed.Add(Axis.Id);
	}
	const bool bExpanded = ExpandedGroups.Num() == 0 || ExpandedGroups.Contains(Group);
	Container->SetExpanded(bExpanded, false);
}

URammsControlRow* URammsControlSurfacePanel::FindRow(FName ControlId) const
{
	for (URammsControlRow* Row : Rows)
	{
		if (Row && Row->GetControlId() == ControlId)
		{
			return Row;
		}
	}
	return nullptr;
}

URammsSurfaceJoystick* URammsControlSurfacePanel::FindJoystick(FName ControlId) const
{
	for (URammsSurfaceJoystick* Joystick : Joysticks)
	{
		if (Joystick && (Joystick->ControlIdX == ControlId || Joystick->ControlIdY == ControlId))
		{
			return Joystick;
		}
	}
	return nullptr;
}

void URammsControlSurfacePanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ReadbackAccum += InDeltaTime;
	if (ReadbackAccum < ReadbackInterval)
	{
		return;
	}
	ReadbackAccum = 0.0f;

	if (!TargetSurface)
	{
		if (bAutoFindSurface && ResolveSurface())
		{
			Rebuild();
		}
		return;
	}
	const int32 Version = IRammsControlSurfaceProvider::Execute_GetControlSurfaceVersion(TargetSurface);
	if (Version != BuiltVersion)
	{
		Rebuild();
		return;
	}
	for (URammsControlRow* Row : Rows)
	{
		if (Row)
		{
			Row->Refresh();
		}
	}
}

void URammsControlSurfacePanel::ApplyStyle_Implementation()
{
	Super::ApplyStyle_Implementation();
	if (!Style)
	{
		return;
	}
	if (PanelBorder)
	{
		PanelBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(Style->Colors.Background, Style->Border.CornerRadiusMedium, Style->Colors.Border, Style->Border.BorderWidth);
	}
	if (TitleText)
	{
		TitleText->SetFont(Style->Typography.HeadingMedium);
		TitleText->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}
}

float URammsControlSurfacePanel::GetScrollOffset() const
{
	return GroupsScroll ? GroupsScroll->GetScrollOffset() : 0.0f;
}

FVector2D URammsControlSurfacePanel::GetScrollExtent() const
{
	if (!GroupsScroll)
	{
		return FVector2D::ZeroVector;
	}
	return FVector2D(GroupsScroll->GetCachedGeometry().GetLocalSize().Y, GroupsScroll->GetScrollOffsetOfEnd());
}

void URammsControlSurfacePanel::SetScrollOffset(float Offset)
{
	if (GroupsScroll)
	{
		GroupsScroll->SetScrollOffset(FMath::Clamp(Offset, 0.0f, GroupsScroll->GetScrollOffsetOfEnd()));
	}
}

FReply URammsControlSurfacePanel::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// The wheel anywhere over the panel scrolls the groups list (the nested
	// scroll boxes and rows bubble it up here); it never reaches the game
	// viewport, so it never zooms the camera.
	if (GroupsScroll)
	{
		const float Before = GroupsScroll->GetScrollOffset();
		SetScrollOffset(Before - InMouseEvent.GetWheelDelta() * WheelScrollStep);
		UE_LOG(LogTemp, Verbose, TEXT("[ControlSurfacePanel] wheel %.2f: %.0f -> %.0f (end %.0f)"), InMouseEvent.GetWheelDelta(), Before, GroupsScroll->GetScrollOffset(), GroupsScroll->GetScrollOffsetOfEnd());
		return FReply::Handled();
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}
