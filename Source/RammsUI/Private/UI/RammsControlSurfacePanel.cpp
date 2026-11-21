// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsControlSurfacePanel.h"
#include "UI/RammsCollapsibleContainer.h"
#include "UI/RammsControlRow.h"
#include "UI/RammsSurfaceJoystick.h"
#include "UI/RammsSurfacePositionPad.h"
#include "UI/RammsUIStyle.h"
#include "RammsControlSink.h"
#include "RammsControlSurfaceProvider.h"
#include "RammsControlSurfaceRegistry.h"
#include "RammsControlHUDSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	/** An actor or component name made readable: "BP_MyRobot_Curb_C_0"
	 *  -> "My Robot Curb". */
	FText ReadableName(FString Name)
	{
		Name.RemoveFromStart(TEXT("BP_"));
		int32 Cut = INDEX_NONE;
		if (Name.FindLastChar(TEXT('_'), Cut) && Cut > 0 && Name.Mid(Cut + 1).IsNumeric())
		{
			Name.LeftInline(Cut); // the instance number
		}
		Name.RemoveFromEnd(TEXT("_C"));
		return FText::FromString(FName::NameToDisplayString(Name, /*bIsBool=*/false));
	}

	/** A pair's caption in the Sim look. Names that start alike say the shared part once:
	 *  "Seat height" and "Seat fore/aft" -> "Seat: height × fore/aft". */
	FText PairCaption(const FText& Vertical, const FText& Horizontal)
	{
		TArray<FString> A, B;
		Vertical.ToString().ParseIntoArrayWS(A);
		Horizontal.ToString().ParseIntoArrayWS(B);
		int32 Shared = 0;
		while (Shared < A.Num() - 1 && Shared < B.Num() - 1 && A[Shared].Equals(B[Shared], ESearchCase::CaseSensitive))
		{
			++Shared;
		}
		const auto Join = [](const TArray<FString>& Words, int32 From, int32 To) {
			FString Out;
			for (int32 I = From; I < To; ++I)
			{
				Out += (Out.IsEmpty() ? TEXT("") : TEXT(" ")) + Words[I];
			}
			return Out;
		};
		if (Shared == 0)
		{
			return FText::FromString(FString::Printf(TEXT("%s \u00D7 %s"), *Vertical.ToString(), *Horizontal.ToString()));
		}
		return FText::FromString(
			FString::Printf(TEXT("%s: %s \u00D7 %s"), *Join(A, 0, Shared), *Join(A, Shared, A.Num()), *Join(B, Shared, B.Num())));
	}
} // namespace

void URammsPanelTabEntry::HandleClicked()
{
	if (URammsControlSurfacePanel* Target = Panel.Get())
	{
		Target->SelectTab(Name);
	}
}

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

	// Sim look: the tabs. Empty and collapsed otherwise.
	TabBar = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("TabBar"));
	TabBar->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* TabSlot = MainVBox->AddChildToVerticalBox(TabBar))
	{
		TabSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
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
	TabBar = nullptr;
	TabEntries.Reset();
	SectionLabels.Reset();
	Groups.Reset();
	Rows.Reset();
	Captions.Reset();
	Joysticks.Reset();
	Pads.Reset();
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
			if (URammsControlSurfaceRegistry* Reg = World->GetSubsystem<URammsControlSurfaceRegistry>())
			{
				Reg->OnControlSurfaceRegistryChanged.AddUniqueDynamic(this, &URammsControlSurfacePanel::OnRegistryChanged);
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
			if (URammsControlSurfaceRegistry* Reg = World->GetSubsystem<URammsControlSurfaceRegistry>())
			{
				Reg->OnControlSurfaceRegistryChanged.RemoveDynamic(this, &URammsControlSurfacePanel::OnRegistryChanged);
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
	UWorld*						  World = GetWorld();
	URammsControlSurfaceRegistry* Reg = World ? World->GetSubsystem<URammsControlSurfaceRegistry>() : nullptr;
	if (!Reg)
	{
		return false;
	}
	UObject* Found = TargetRobotName.IsEmpty() ? nullptr : Reg->FindControlSurfaceByRobotName(TargetRobotName);
	if (!Found)
	{
		Found = Reg->FindControlSurface();
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
	Captions.Reset();
	Joysticks.Reset();
	Pads.Reset();
	TabEntries.Reset();
	SectionLabels.Reset();
	if (TabBar)
	{
		TabBar->ClearChildren();
		TabBar->SetVisibility(ESlateVisibility::Collapsed);
	}
	bBuiltSim = URammsUIStyle::UsesSimLook(Style);

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
		// A name that is an identifier (an actor's, say) is made readable; one already
		// written for people is kept.
		const FString Name = Surface.RobotName.IsEmpty() ? TargetSurface->GetName() : Surface.RobotName.ToString();
		TitleText->SetText(Name.Contains(TEXT(" ")) ? FText::FromString(Name) : ReadableName(Name));
		ApplyChrome();
	}

	// Groups in the surface's order; axes inside by Order.
	TArray<FName> GroupOrder = Surface.Groups;
	for (const FRammsControlAxis& Axis : Surface.Axes)
	{
		GroupOrder.AddUnique(Axis.Group);
	}
	if (bBuiltSim)
	{
		BuildSimTabs(Surface, GroupOrder);
		ApplyStyle();
		return;
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

// --- Sim look: tabs of groups ------------------------------------------------

void URammsControlSurfacePanel::BuildSimTabs(const FRammsControlSurface& Surface, const TArray<FName>& GroupOrder)
{
	// Which groups go under which tab: the settings' tabs in their order, each with the groups of its that
	// this robot has; then a tab apiece for any group no tab names, in the surface's order.
	struct FTabPlan
	{
		FName		  Name;
		TArray<FName> Groups;
	};
	const auto HasAxes = [&Surface](FName Group) {
		return Surface.Axes.ContainsByPredicate([Group](const FRammsControlAxis& Axis) { return Axis.Group == Group; });
	};
	TArray<FTabPlan> Plan;
	TSet<FName>		 Claimed;
	for (const FRammsHudTab& Configured : GetDefault<URammsControlHUDSettings>()->Tabs)
	{
		FTabPlan Tab;
		Tab.Name = Configured.Name;
		for (const FName& Group : Configured.Groups)
		{
			if (!Claimed.Contains(Group) && GroupOrder.Contains(Group) && HasAxes(Group))
			{
				Tab.Groups.Add(Group);
				Claimed.Add(Group);
			}
		}
		FTabPlan* Same = Plan.FindByPredicate([&Tab](const FTabPlan& Other) { return Other.Name == Tab.Name; });
		if (Tab.Groups.Num() > 0 && Same)
		{
			Same->Groups.Append(Tab.Groups); // two tabs of one name are one tab: a name is what picks it
		}
		else if (Tab.Groups.Num() > 0 && !Tab.Name.IsNone())
		{
			Plan.Add(MoveTemp(Tab));
		}
		else
		{
			// A tab left out gives its groups back, so they still get a place.
			for (const FName& Group : Tab.Groups)
			{
				Claimed.Remove(Group);
			}
		}
	}
	for (const FName& Group : GroupOrder)
	{
		if (!Claimed.Contains(Group) && HasAxes(Group))
		{
			// Under the tab of its own name if there is one (a "View" group the View tab does not list).
			if (FTabPlan* Same = Plan.FindByPredicate([Group](const FTabPlan& Other) { return Other.Name == Group; }))
			{
				Same->Groups.Add(Group);
				continue;
			}
			FTabPlan Tab;
			Tab.Name = Group;
			Tab.Groups.Add(Group);
			Plan.Add(MoveTemp(Tab));
		}
	}

	for (const FTabPlan& Tab : Plan)
	{
		URammsPanelTabEntry* Entry = NewObject<URammsPanelTabEntry>(this);
		Entry->Panel = this;
		Entry->Name = Tab.Name;

		// The tab: its name, a dot when something under it wants attention, and a line under the one on show.
		Entry->Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Entry->Button->SetStyle(URammsUIStyle::MakeTransparentButtonStyle());
		UVerticalBox*	Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Words = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Entry->Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Entry->Label->SetText(FText::FromName(Tab.Name));
		Words->AddChildToHorizontalBox(Entry->Label);
		Entry->Badge = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Entry->Badge->SetText(FText::FromString(TEXT("\u25CF")));
		Entry->Badge->SetVisibility(ESlateVisibility::Collapsed);
		if (UHorizontalBoxSlot* BadgeSlot = Words->AddChildToHorizontalBox(Entry->Badge))
		{
			BadgeSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
			BadgeSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* WordsSlot = Stack->AddChildToVerticalBox(Words))
		{
			WordsSlot->SetPadding(FMargin(10.0f, 6.0f, 10.0f, 5.0f));
			WordsSlot->SetHorizontalAlignment(HAlign_Center);
		}
		Entry->Underline = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Entry->Underline->SetPadding(FMargin(0.0f, 1.0f)); // a border has no size of its own: 2 high
		Stack->AddChildToVerticalBox(Entry->Underline);
		Entry->Button->AddChild(Stack);
		Entry->Button->OnClicked.AddUniqueDynamic(Entry, &URammsPanelTabEntry::HandleClicked);
		TabBar->AddChildToWrapBox(Entry->Button);

		Entry->Page = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		GroupsVBox->AddChildToVerticalBox(Entry->Page);
		TabEntries.Add(Entry);

		for (const FName& Group : Tab.Groups)
		{
			TArray<FRammsControlAxis> Axes;
			Surface.GetAxesInGroup(Group, Axes);
			Axes.StableSort([](const FRammsControlAxis& A, const FRammsControlAxis& B) { return A.Order < B.Order; });
			// A tab of one group named for it needs no heading saying so again.
			BuildSimGroup(Entry, Group, Axes, Tab.Groups.Num() > 1 || Group != Tab.Name);
		}
	}
	// One tab is no choice: no bar for it.
	TabBar->SetVisibility(TabEntries.Num() > 1 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	// The tab the user picked while the robot has it, the first otherwise -- without forgetting the pick, so
	// a tab that is gone for one version of the surface comes back selected.
	ShownTab = TabEntries.ContainsByPredicate([this](const URammsPanelTabEntry* Entry) { return Entry && Entry->Name == SelectedTab; })
		? SelectedTab
		: (TabEntries.Num() > 0 ? TabEntries[0]->Name : NAME_None);
	StyleTabs();
}

UWidget* URammsControlSurfacePanel::MakePairCell(const FRammsControlAxis& Axis, const TArray<FRammsControlAxis>& Axes, TSet<FName>& Consumed,
	TArray<FName>& OutIds)
{
	const bool bRate = Axis.Kind == ERammsControlKind::Continuous;
	if ((!bRate && Axis.Kind != ERammsControlKind::Position) || Axis.bReadOnly || Axis.PairedAxis.IsNone())
	{
		return nullptr;
	}
	const FRammsControlAxis* Pair = Axes.FindByPredicate([&Axis](const FRammsControlAxis& A) { return A.Id == Axis.PairedAxis; });
	if (!Pair || Pair->Kind != Axis.Kind || Pair->bReadOnly || Consumed.Contains(Pair->Id))
	{
		return nullptr;
	}
	// The same pairing as the Classic look (BuildGroup): the lower-Order axis is the vertical one.
	const FRammsControlAxis& Vertical = (Axis.Order <= Pair->Order) ? Axis : *Pair;
	const FRammsControlAxis& Horizontal = (&Vertical == &Axis) ? *Pair : Axis;

	UWidget* Control = nullptr;
	if (bRate)
	{
		URammsSurfaceJoystick* Joystick = CreateWidget<URammsSurfaceJoystick>(this);
		Joystick->bAutoFindSink = false;
		Joystick->SetRadii(JoystickRadius, JoystickRadius * 0.35f);
		Joystick->Source = Source;
		Joystick->SetTarget(TargetSurface, Horizontal.Id, Vertical.Id);
		if (Style)
		{
			Joystick->SetStyle(Style);
		}
		Joysticks.Add(Joystick);
		Control = Joystick;
	}
	else
	{
		URammsSurfacePositionPad* Pad = CreateWidget<URammsSurfacePositionPad>(this);
		Pad->SetStyle(Style);
		Pad->bAutoFindSink = false;
		Pad->Source = Source;
		Pad->SetTarget(TargetSurface, Horizontal.Id, Vertical.Id);
		Pad->SetRegion(URammsControlRow::DisplayRangeFor(Horizontal), URammsControlRow::DisplayRangeFor(Vertical), Vertical.RegionOutline);
		Pads.Add(Pad);
		Control = Pad;
	}
	Consumed.Add(Axis.Id);
	Consumed.Add(Pair->Id);
	OutIds.Add(Axis.Id);
	OutIds.Add(Pair->Id);

	// The control in a box of its own size (its canvas places its images absolutely), its caption under it.
	UVerticalBox* Cell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	USizeBox*	  Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	const float	  BoxSize = JoystickRadius * 2.0f + 8.0f;
	Box->SetWidthOverride(BoxSize);
	Box->SetHeightOverride(BoxSize);
	Box->AddChild(Control);
	if (UVerticalBoxSlot* BoxSlot = Cell->AddChildToVerticalBox(Box))
	{
		BoxSlot->SetHorizontalAlignment(HAlign_Center);
		BoxSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 2.0f));
	}
	Cell->AddChildToVerticalBox(
		MakeCaption(PairCaption(URammsControlRow::SimLabel(Vertical.DisplayName), URammsControlRow::SimLabel(Horizontal.DisplayName))));
	return Cell;
}

void URammsControlSurfacePanel::BuildSimGroup(URammsPanelTabEntry* Tab, FName Group, const TArray<FRammsControlAxis>& Axes, bool bHeading)
{
	UVerticalBox* Page = Tab->Page;
	if (bHeading)
	{
		UTextBlock* Heading = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Heading->SetText(FText::FromString(Group.ToString().ToUpper()));
		if (UVerticalBoxSlot* HeadingSlot = Page->AddChildToVerticalBox(Heading))
		{
			HeadingSlot->SetPadding(FMargin(4.0f, Page->GetChildrenCount() > 1 ? 12.0f : 2.0f, 4.0f, 4.0f));
		}
		SectionLabels.Add(Heading);
	}

	// Rows go one to a line. Actions and pads sit side by side: a run of either fills lines of up to
	// LineMax before starting the next.
	enum class ELine : uint8
	{
		None,
		Actions,
		Pairs
	};
	UHorizontalBox* Line = nullptr;
	ELine			LineKind = ELine::None;
	int32			LineMax = 0;
	// A line of actions that the run did not fill keeps its cells' width: empty cells take the rest, so the
	// last button of a run is the size of the ones before it and not stretched across what is left. (A
	// lone action, a run of one, has nothing to match and takes the line.)
	bool	   bLineContinuesRun = false;
	const auto CloseLine = [&]() {
		if (Line && LineKind == ELine::Actions && (Line->GetChildrenCount() > 1 || bLineContinuesRun))
		{
			while (Line->GetChildrenCount() < LineMax)
			{
				if (UHorizontalBoxSlot* Empty = Line->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass())))
				{
					Empty->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
					Empty->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
				}
			}
		}
	};
	const auto Place = [&](UWidget* Widget, ELine Kind, int32 Max) {
		if (Kind == ELine::None)
		{
			CloseLine();
			Line = nullptr;
			LineKind = ELine::None;
			if (UVerticalBoxSlot* RowSlot = Page->AddChildToVerticalBox(Widget))
			{
				RowSlot->SetPadding(FMargin(4.0f, 3.0f));
			}
			return;
		}
		if (!Line || LineKind != Kind || Line->GetChildrenCount() >= LineMax)
		{
			CloseLine();
			bLineContinuesRun = Line && LineKind == Kind; // a full line of the same run came before this one
			Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			LineKind = Kind;
			LineMax = Max;
			if (UVerticalBoxSlot* LineSlot = Page->AddChildToVerticalBox(Line))
			{
				LineSlot->SetPadding(FMargin(4.0f, 2.0f));
			}
		}
		if (UHorizontalBoxSlot* CellSlot = Line->AddChildToHorizontalBox(Widget))
		{
			CellSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			CellSlot->SetPadding(FMargin(Line->GetChildrenCount() > 1 ? 8.0f : 0.0f, 0.0f, 0.0f, 0.0f));
		}
	};

	TSet<FName> Consumed;
	for (int32 I = 0; I < Axes.Num(); ++I)
	{
		const FRammsControlAxis& Axis = Axes[I];
		if (Consumed.Contains(Axis.Id))
		{
			continue;
		}
		if (UWidget* Cell = MakePairCell(Axis, Axes, Consumed, Tab->ControlIds))
		{
			Place(Cell, ELine::Pairs, 2);
			// The group's other sticks and pads join this one, two to a line, rather than each waiting its
			// turn among the rows: side by side they take half the height, and the rows that follow stay in
			// their own order.
			for (int32 J = I + 1; J < Axes.Num(); ++J)
			{
				if (!Consumed.Contains(Axes[J].Id))
				{
					if (UWidget* More = MakePairCell(Axes[J], Axes, Consumed, Tab->ControlIds))
					{
						Place(More, ELine::Pairs, 2);
					}
				}
			}
			continue;
		}
		URammsControlRow* Row = CreateWidget<URammsControlRow>(this);
		if (Style)
		{
			Row->SetStyle(Style);
		}
		Row->Setup(TargetSurface, Axis, Source);
		Rows.Add(Row);
		Tab->Rows.Add(Row);
		Tab->ControlIds.Add(Axis.Id);
		Consumed.Add(Axis.Id);
		if (!Axis.IsAction())
		{
			Place(Row, ELine::None, 0);
			continue;
		}
		// Three short names to a line, or two longer ones: decided for the whole run, so its lines match.
		int32 Max = 3;
		if (LineKind != ELine::Actions || !Line)
		{
			for (int32 J = I; J < Axes.Num() && Axes[J].IsAction(); ++J)
			{
				const FText Name = URammsControlRow::SimLabel(Axes[J].DisplayName.IsEmpty() ? FText::FromName(Axes[J].Id) : Axes[J].DisplayName);
				if (Name.ToString().Len() > 10)
				{
					Max = 2;
					break;
				}
			}
		}
		else
		{
			Max = LineMax;
		}
		Place(Row, ELine::Actions, Max);
	}
	CloseLine();
}

void URammsControlSurfacePanel::SelectTab(FName Tab)
{
	if (!TabEntries.ContainsByPredicate([Tab](const URammsPanelTabEntry* Entry) { return Entry && Entry->Name == Tab; }))
	{
		return;
	}
	SelectedTab = Tab;
	ShownTab = Tab;
	StyleTabs();
	SetScrollOffset(0.0f); // a new page starts at its top
}

TArray<FName> URammsControlSurfacePanel::GetTabNames() const
{
	TArray<FName> Names;
	for (const URammsPanelTabEntry* Entry : TabEntries)
	{
		if (Entry)
		{
			Names.Add(Entry->Name);
		}
	}
	return Names;
}

FName URammsControlSurfacePanel::FindTabOfControl(FName ControlId) const
{
	for (const URammsPanelTabEntry* Entry : TabEntries)
	{
		if (Entry && Entry->ControlIds.Contains(ControlId))
		{
			return Entry->Name;
		}
	}
	return NAME_None;
}

void URammsControlSurfacePanel::StyleTabs()
{
	for (URammsPanelTabEntry* Entry : TabEntries)
	{
		if (!Entry)
		{
			continue;
		}
		const bool bShown = Entry->Name == ShownTab;
		if (Entry->Page)
		{
			Entry->Page->SetVisibility(bShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (!Style)
		{
			continue;
		}
		if (Entry->Label)
		{
			Entry->Label->SetFont(Style->Typography.Body);
			Entry->Label->SetColorAndOpacity(FSlateColor(bShown ? Style->Colors.TextPrimary : Style->Colors.TextSecondary));
		}
		if (Entry->Badge)
		{
			Entry->Badge->SetFont(Style->Typography.Caption);
		}
		if (Entry->Underline)
		{
			URammsUIStyle::ApplyRoundedBrushToBorder(Entry->Underline,
				URammsUIStyle::MakeRoundedBoxBrush(bShown ? Style->Colors.Info : FLinearColor::Transparent, 1.0f));
		}
	}
	for (UTextBlock* Heading : SectionLabels)
	{
		if (Heading && Style)
		{
			Heading->SetFont(Style->Typography.Caption);
			Heading->SetColorAndOpacity(FSlateColor(Style->Colors.TextDisabled));
		}
	}
	RefreshTabBadges();
}

void URammsControlSurfacePanel::RefreshTabBadges()
{
	// A status that wants attention (a crossing paused for the rider, a fault) marks its tab, so it is not
	// missed for being on a page that is not showing.
	for (URammsPanelTabEntry* Entry : TabEntries)
	{
		if (!Entry || !Entry->Badge)
		{
			continue;
		}
		int32 Severity = 0;
		for (const URammsControlRow* Row : Entry->Rows)
		{
			if (Row)
			{
				Severity = FMath::Max(Severity, Row->GetTextSeverity());
			}
		}
		Entry->Badge->SetVisibility(Severity > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Severity > 0 && Style)
		{
			Entry->Badge->SetColorAndOpacity(
				FSlateColor(Severity >= static_cast<int32>(ERammsControlTextSeverity::Fault) ? Style->Colors.Error : Style->Colors.Warning));
		}
	}
}

void URammsControlSurfacePanel::InsetInGroup(UWidget* Child, float Top, float Bottom)
{
	if (UVerticalBoxSlot* S = Child ? Cast<UVerticalBoxSlot>(Child->Slot) : nullptr)
	{
		S->SetPadding(FMargin(8.0f, Top, 8.0f, Bottom));
	}
}

UTextBlock* URammsControlSurfacePanel::MakeCaption(const FText& Text)
{
	UTextBlock* Caption = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Caption->SetText(Text);
	Caption->SetJustification(ETextJustify::Center);
	Caption->SetAutoWrapText(true);
	if (Style)
	{
		Caption->SetFont(Style->Typography.Caption);
		Caption->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
	}
	Captions.Add(Caption);
	return Caption;
}

void URammsControlSurfacePanel::BuildGroup(FName Group, const TArray<FRammsControlAxis>& Axes)
{
	URammsCollapsibleContainer* Container = CreateWidget<URammsCollapsibleContainer>(this);
	Container->SetHeaderTitle(FText::FromName(Group));
	Container->SetCompactHeader(true);						   // a group in a panel, not a panel
	Container->SetMaxContentHeight(0.0f);					   // size to content; the panel's own ScrollBox scrolls
	Container->SetWheelConsumption(EConsumeMouseWheel::Never); // the wheel bubbles up to the panel, which scrolls the list
	Container->SetScrollingEnabled(false);					   // it sizes to its content; only the panel's list scrolls
	UVerticalBoxSlot* GroupSlot = GroupsVBox->AddChildToVerticalBox(Container);
	if (GroupSlot)
	{
		GroupSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}
	Groups.Add(Container);
	if (Style)
	{
		Container->SetStyle(Style); // created after construction: propagate explicitly
	}

	TSet<FName> Consumed;
	for (const FRammsControlAxis& Axis : Axes)
	{
		if (Consumed.Contains(Axis.Id))
		{
			continue;
		}
		// A pair of Continuous axes -> joystick (the lower-Order axis is vertical).
		if (Axis.Kind == ERammsControlKind::Continuous && !Axis.bReadOnly && !Axis.PairedAxis.IsNone())
		{
			const FRammsControlAxis* Pair = Axes.FindByPredicate([&Axis](const FRammsControlAxis& A) { return A.Id == Axis.PairedAxis; });
			if (Pair && Pair->Kind == ERammsControlKind::Continuous && !Pair->bReadOnly && !Consumed.Contains(Pair->Id))
			{
				const FRammsControlAxis& Vertical = (Axis.Order <= Pair->Order) ? Axis : *Pair;
				const FRammsControlAxis& Horizontal = (&Vertical == &Axis) ? *Pair : Axis;
				URammsSurfaceJoystick*	 Joystick = CreateWidget<URammsSurfaceJoystick>(this);
				Joystick->bAutoFindSink = false;
				Joystick->SetRadii(JoystickRadius, JoystickRadius * 0.35f);
				Joystick->Source = Source;
				Joystick->SetTarget(TargetSurface, Horizontal.Id, Vertical.Id);
				if (Style)
				{
					Joystick->SetStyle(Style);
				}

				UTextBlock* Caption =
					MakeCaption(FText::Format(NSLOCTEXT("RammsUI", "JoystickCaption", "{0} / {1}"), Vertical.DisplayName, Horizontal.DisplayName));
				Container->AddContentChild(Caption);
				InsetInGroup(Caption, 6.0f, 0.0f);
				// The joystick's canvas positions its images absolutely, so give it
				// a fixed box of its own size (plus a margin) in both dimensions and
				// centre that in the row; otherwise the row is narrower / shorter than
				// the circle and neighbours draw over it.
				USizeBox*	Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				const float BoxSize = JoystickRadius * 2.0f + 8.0f;
				Box->SetWidthOverride(BoxSize);
				Box->SetHeightOverride(BoxSize);
				Box->AddChild(Joystick);
				UHorizontalBox* Centre = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				if (UHorizontalBoxSlot* CentreSlot = Centre->AddChildToHorizontalBox(Box))
				{
					CentreSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
					CentreSlot->SetHorizontalAlignment(HAlign_Center);
					CentreSlot->SetPadding(FMargin(0.0f, 4.0f));
				}
				Container->AddContentChild(Centre);
				Joysticks.Add(Joystick);
				Consumed.Add(Axis.Id);
				Consumed.Add(Pair->Id);
				continue;
			}
		}
		// A pair of Position axes -> pad. Same pairing convention as the
		// joystick above, but the values are places rather than rates, so the
		// widget draws where the pair IS inside what it can reach -- which for
		// a 5-bar is a curved region the two Ranges badly over-claim.
		if (Axis.Kind == ERammsControlKind::Position && !Axis.bReadOnly && !Axis.PairedAxis.IsNone())
		{
			const FRammsControlAxis* Pair = Axes.FindByPredicate([&Axis](const FRammsControlAxis& A) { return A.Id == Axis.PairedAxis; });
			if (Pair && Pair->Kind == ERammsControlKind::Position && !Pair->bReadOnly && !Consumed.Contains(Pair->Id))
			{
				const FRammsControlAxis& Vertical = (Axis.Order <= Pair->Order) ? Axis : *Pair;
				const FRammsControlAxis& Horizontal = (&Vertical == &Axis) ? *Pair : Axis;

				URammsSurfacePositionPad* Pad = CreateWidget<URammsSurfacePositionPad>(this);
				Pad->SetStyle(Style); // created after construction: propagate explicitly
				Pad->bAutoFindSink = false;
				Pad->Source = Source;
				Pad->SetTarget(TargetSurface, Horizontal.Id, Vertical.Id);
				// The region rides on the vertical axis, which is where the
				// contributor puts it so there is one per pair.
				// Display ranges, the same rule a slider row uses. A pair that
				// defers its range to the backend has a non-increasing one, and
				// mapping a pad through that collapses every value onto one
				// pixel and draws a fallback box of zero size.
				Pad->SetRegion(URammsControlRow::DisplayRangeFor(Horizontal),
					URammsControlRow::DisplayRangeFor(Vertical), Vertical.RegionOutline);

				UTextBlock* Caption =
					MakeCaption(FText::Format(NSLOCTEXT("RammsUI", "PadCaption", "{0} / {1}"), Vertical.DisplayName, Horizontal.DisplayName));
				Container->AddContentChild(Caption);
				InsetInGroup(Caption, 6.0f, 0.0f);

				USizeBox*	Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				const float PadSize = JoystickRadius * 2.0f + 8.0f;
				Box->SetWidthOverride(PadSize);
				Box->SetHeightOverride(PadSize);
				Box->AddChild(Pad);
				UHorizontalBox* Centre = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				if (UHorizontalBoxSlot* CentreSlot = Centre->AddChildToHorizontalBox(Box))
				{
					CentreSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
					CentreSlot->SetHorizontalAlignment(HAlign_Center);
					CentreSlot->SetPadding(FMargin(0.0f, 4.0f));
				}
				Container->AddContentChild(Centre);
				Pads.Add(Pad);
				Consumed.Add(Axis.Id);
				Consumed.Add(Pair->Id);
				continue;
			}
		}

		URammsControlRow* Row = CreateWidget<URammsControlRow>(this);
		if (Style)
		{
			Row->SetStyle(Style);
		}
		Row->Setup(TargetSurface, Axis, Source);
		Container->AddContentChild(Row);
		InsetInGroup(Row);
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
	if (bBuiltSim)
	{
		RefreshTabBadges();
	}
}

void URammsControlSurfacePanel::ApplyStyle_Implementation()
{
	Super::ApplyStyle_Implementation();
	if (!Style)
	{
		return;
	}
	if (Style->IsSimLook() != bBuiltSim && BuiltVersion != -1)
	{
		// A theme of the other look: its rows are different widgets, not the same ones restyled. (Rebuild
		// applies the style itself when it finds a robot, with the looks then agreeing; the rest of this
		// runs either way, so the title and frame are restyled when it finds none.)
		Rebuild();
	}
	StyleTabs();
	if (TitleText)
	{
		TitleText->SetFont(Style->Typography.HeadingMedium);
		TitleText->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}
	ApplyChrome();
	for (UTextBlock* Caption : Captions)
	{
		if (Caption)
		{
			Caption->SetFont(Style->Typography.Caption);
			Caption->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
		}
	}
	// The groups, rows and joysticks are created at runtime, outside this
	// widget's tree: hand them the style directly.
	for (URammsCollapsibleContainer* Group : Groups)
	{
		if (Group)
		{
			Group->SetStyle(Style);
		}
	}
	for (URammsControlRow* Row : Rows)
	{
		if (Row)
		{
			Row->SetStyle(Style);
		}
	}
	for (URammsSurfaceJoystick* Joystick : Joysticks)
	{
		if (Joystick)
		{
			Joystick->SetStyle(Style);
		}
	}
	for (URammsSurfacePositionPad* Pad : Pads)
	{
		if (Pad)
		{
			Pad->SetStyle(Style);
		}
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

TArray<FString> URammsControlSurfacePanel::GetLayoutReport() const
{
	TArray<FString> Out;
	for (const URammsCollapsibleContainer* Group : Groups)
	{
		if (Group)
		{
			const FVector2D Size = Group->GetCachedGeometry().GetLocalSize();
			Out.Add(FString::Printf(TEXT("group %s: %.0f x %.0f (desired %.0f x %.0f)"), *Group->GetName(), Size.X, Size.Y, Group->GetDesiredSize().X, Group->GetDesiredSize().Y));
		}
	}
	for (const URammsSurfaceJoystick* Joystick : Joysticks)
	{
		if (Joystick)
		{
			const FVector2D Size = Joystick->GetCachedGeometry().GetLocalSize();
			Out.Add(FString::Printf(TEXT("joystick %s/%s: %.0f x %.0f (desired %.0f x %.0f)"), *Joystick->ControlIdY.ToString(), *Joystick->ControlIdX.ToString(), Size.X, Size.Y, Joystick->GetDesiredSize().X, Joystick->GetDesiredSize().Y));
		}
	}
	return Out;
}

FText URammsControlSurfacePanel::GetFloatingTitle() const
{
	return TitleText ? TitleText->GetText() : FText::GetEmpty();
}

void URammsControlSurfacePanel::SetHostedInFloatingPanel(bool bHosted)
{
	bHostedInFloatingPanel = bHosted;
	ApplyChrome();
}

void URammsControlSurfacePanel::ApplyChrome()
{
	// Hosted, the floating panel's frame and title bar stand in for its own.
	if (PanelBorder)
	{
		if (bHostedInFloatingPanel)
		{
			PanelBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(FLinearColor::Transparent, 0.0f);
			PanelBorder->SetPadding(FMargin(6.0f, 4.0f));
		}
		else
		{
			PanelBorder->Background = Style ? URammsUIStyle::MakeRoundedBoxBrush(Style->Colors.Background, Style->Border.CornerRadiusMedium,
												  Style->Colors.Border, Style->Border.BorderWidth)
											: URammsUIStyle::MakeRoundedBoxBrush(FLinearColor(0.06f, 0.06f, 0.08f, 0.9f), 4.0f,
												  FLinearColor(0.3f, 0.3f, 0.3f, 1.0f), 1.0f);
			PanelBorder->SetPadding(FMargin(8.0f));
		}
	}
	if (TitleText)
	{
		TitleText->SetVisibility(bShowRobotName && !bHostedInFloatingPanel ? ESlateVisibility::SelfHitTestInvisible
																		   : ESlateVisibility::Collapsed);
	}
}
