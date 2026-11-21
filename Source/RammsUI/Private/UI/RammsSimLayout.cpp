// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsSimLayout.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Overlay.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "RammsControlHUDSettings.h"
#include "RammsControlHUDSubsystem.h"
#include "RammsHUDLayoutUserSettings.h"
#include "RammsHUDPanelProvider.h"
#include "RammsControlSink.h"
#include "RammsControlSurfaceProvider.h"
#include "RammsControlSurfaceRegistry.h"
#include "UI/RammsButton.h"
#include "UI/RammsControlRow.h"
#include "UI/RammsControlSurfacePanel.h"
#include "UI/RammsFloatingPanel.h"
#include "UI/RammsLayoutHost.h"
#include "UI/RammsSlateHostWidget.h"
#include "UI/RammsSurfaceJoystick.h"
#include "UI/RammsUIStyle.h"

namespace
{
	const FName JoystickId(TEXT("Joystick"));
	const FName AllId(TEXT("All"));
} // namespace

TArray<FName> URammsSimLayout::GetLayoutSlotNames_Implementation() const
{
	return { FName("SurfacePanel"), FName("Joystick"), FName("Status"), FName("CameraCapturePanel") };
}

void URammsSimLayout::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;
	RootCanvas = Root;

	const URammsControlHUDSettings* Settings = GetDefault<URammsControlHUDSettings>();
	bFloating = bFloatingPanels && (!Settings || Settings->bFloatingPanels);
	if (bFloating)
	{
		BuildFloating(Root);
	}
	else
	{
		BuildFixed(Root);
	}
	LayoutDisplayName = FText::FromString(TEXT("Sim"));
}

void URammsSimLayout::ResetCachedWidgets()
{
	RootCanvas = nullptr;
	FloatingPanels.Reset();
	JoystickSlot = nullptr;
	TrayBorder = nullptr;
	TrayRow = nullptr;
	TrayEntries.Reset();
	ResetButton = nullptr;
	StatusBorder = nullptr;
	StatusRow = nullptr;
	StatusNames.Reset();
	StatusValues.Reset();
	StatusAxes.Reset();
	PromptBox = nullptr;
	PromptFrames.Reset();
	PromptTexts.Reset();
	PromptAxes.Reset();
	PromptStates.Reset();
	TrayAxes.Reset();
	StatusVersion = -1;
	bStickUnused = false;
}

void URammsSimLayout::BuildFixed(UCanvasPanel* Root)
{
	// Right column: the control-surface panel, full height.
	{
		UOverlay* SlotWidget = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SurfacePanel"));
		if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(SlotWidget))
		{
			CanvasSlot->SetAnchors(FAnchors(1.0f - PanelColumnFraction, 0.0f, 1.0f, 1.0f));
			CanvasSlot->SetOffsets(FMargin(8.0f, 8.0f, 8.0f, 8.0f));
			CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		}
		RegisterSlot(TEXT("SurfacePanel"), SlotWidget);
	}
	// Bottom-left: the drive joystick.
	{
		UOverlay* SlotWidget = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Joystick"));
		if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(SlotWidget))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
			CanvasSlot->SetAlignment(FVector2D(0.0f, 1.0f));
			CanvasSlot->SetPosition(FVector2D(24.0f, -24.0f));
			CanvasSlot->SetSize(JoystickAreaSize);
		}
		RegisterSlot(TEXT("Joystick"), SlotWidget);
		JoystickSlot = SlotWidget;
	}
	// Top-left: status.
	{
		UOverlay* SlotWidget = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Status"));
		if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(SlotWidget))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
			CanvasSlot->SetPosition(FVector2D(16.0f, 16.0f));
			CanvasSlot->SetAutoSize(true);
		}
		RegisterSlot(TEXT("Status"), SlotWidget);
	}
	// Left column, below status: the camera capture panel. Left rather than
	// right so it does not fight the control-surface panel for the same
	// column, and anchored top-to-bottom so it never grows off the screen.
	{
		UOverlay* SlotWidget = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CameraCapturePanel"));
		if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(SlotWidget))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 1.0f));
			CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
			// Width and bottom margin in ONE call. With these anchors the slot
			// is stretched vertically but not horizontally, so Offsets.Right is
			// the width and Offsets.Bottom is the gap above the bottom edge --
			// and SetSize writes both of them, which is how a following
			// SetSize(width, 0) reset the 200 px margin to zero and ran the
			// panel down into the joystick.
			CanvasSlot->SetOffsets(FMargin(16.0f, 64.0f, CameraPanelWidth, 200.0f));
		}
		RegisterSlot(TEXT("CameraCapturePanel"), SlotWidget);
	}
}

void URammsSimLayout::BuildFloating(UCanvasPanel* Root)
{
	// The joystick stays where a thumb finds it; the tray can hide it.
	{
		UOverlay* SlotWidget = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Joystick"));
		if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(SlotWidget))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
			CanvasSlot->SetAlignment(FVector2D(0.0f, 1.0f));
			CanvasSlot->SetPosition(FVector2D(24.0f, -24.0f));
			CanvasSlot->SetSize(JoystickAreaSize);
		}
		RegisterSlot(TEXT("Joystick"), SlotWidget);
		JoystickSlot = SlotWidget;
		FRammsFloatingPanelState Stored;
		if (URammsHUDLayoutUserSettings::Get()->Find(JoystickId, Stored))
		{
			bJoystickHidden = Stored.bHidden;
		}
		SlotWidget->SetVisibility(bJoystickHidden ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	{
		UOverlay* SlotWidget = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Status"));
		if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(SlotWidget))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			CanvasSlot->SetPosition(FVector2D(16.0f, 16.0f));
			CanvasSlot->SetAutoSize(true);
		}
		RegisterSlot(TEXT("Status"), SlotWidget);
	}
	// The robot's controls down the right, the camera feeds top-left -- where
	// the fixed layout had them -- until the user moves them.
	if (URammsFloatingPanel* Panel =
			AddFloatingPanel(TEXT("SurfacePanel"), FText::FromString(TEXT("Robot")), FVector2D(1.0, 0.0), /*bResizable=*/true))
	{
		// About the fixed layout's column; its widest rows are wider than that.
		Panel->DefaultSize = FVector2D(420.0, 0.0);
		RegisterSlot(TEXT("SurfacePanel"), Panel->GetContentSlot());
	}
	if (URammsFloatingPanel* Panel = AddFloatingPanel(TEXT("CameraCapturePanel"), FText::FromString(TEXT("Camera capture")),
			FVector2D(0.0, 0.07), /*bResizable=*/false))
	{
		RegisterSlot(TEXT("CameraCapturePanel"), Panel->GetContentSlot());
	}

	// The panel tray, top centre: a button per panel, show / hide all, reset.
	TrayBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelTray"));
	TrayBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(FLinearColor(0.06f, 0.06f, 0.08f, 0.6f), 8.0f);
	TrayBorder->SetPadding(FMargin(8.0f, 3.0f));
	if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(TrayBorder))
	{
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		CanvasSlot->SetPosition(FVector2D(0.0f, 4.0f));
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetZOrder(1000000);
	}
	TrayRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PanelTrayRow"));
	TrayBorder->AddChild(TrayRow);
	bTrayDirty = true;

	// Sim look: the status strip, top left -- a chip per state worth knowing without opening a panel (the
	// drive mode, whether the sim is running) -- and, bottom centre, what the robot wants of the user. Both
	// are empty and collapsed until PollStatus has something for them, and in the Classic look always.
	StatusBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatusStrip"));
	StatusBorder->SetPadding(FMargin(4.0f, 3.0f));
	StatusBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(StatusBorder))
	{
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		CanvasSlot->SetPosition(FVector2D(FloatingMargin, 4.0f));
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetZOrder(1000000);
	}
	StatusRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("StatusRow"));
	StatusBorder->AddChild(StatusRow);

	PromptBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Prompts"));
	PromptBox->SetVisibility(ESlateVisibility::HitTestInvisible); // words to read: clicks go through to the scene
	if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(PromptBox))
	{
		CanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		CanvasSlot->SetPosition(FVector2D(0.0f, -40.0f));
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetZOrder(1000001);
	}
}

URammsFloatingPanel* URammsSimLayout::AddFloatingPanel(FName Id, const FText& Title, const FVector2D& DefaultPlacement, bool bResizable)
{
	if (!WidgetTree || !RootCanvas)
	{
		return nullptr;
	}
	URammsFloatingPanel* Panel = WidgetTree->ConstructWidget<URammsFloatingPanel>(URammsFloatingPanel::StaticClass(),
		FName(*FString::Printf(TEXT("Floating_%s"), *Id.ToString())));
	if (!Panel)
	{
		return nullptr;
	}
	Panel->PanelId = Id;
	Panel->Title = Title;
	Panel->DefaultPlacement = DefaultPlacement;
	Panel->bResizable = bResizable;
	if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(Panel))
	{
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		CanvasSlot->SetAutoSize(true);
	}
	if (Style)
	{
		Panel->SetStyle(Style);
	}
	Panel->OnPanelChanged.AddDynamic(this, &URammsSimLayout::HandlePanelChanged);
	FloatingPanels.Add(Panel);
	bTrayDirty = true;
	return Panel;
}

void URammsSimLayout::ApplyStyle_Implementation()
{
	Super::ApplyStyle_Implementation();
	if (!Style)
	{
		return;
	}
	for (URammsFloatingPanel* Panel : FloatingPanels)
	{
		if (Panel)
		{
			Panel->SetStyle(Style);
		}
	}
	if (TrayBorder)
	{
		FLinearColor Back = Style->Colors.Background;
		Back.A = 0.6f;
		TrayBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(Back, Style->Border.CornerRadiusLarge);
	}
	bTrayDirty = true;
	StatusVersion = -1; // the strip and prompts are the look's: build (or clear) them for this theme
	StyleStatus();
}

// ---------------------------------------------------------------------------
// Per tick
// ---------------------------------------------------------------------------

void URammsSimLayout::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bFloating || !RootCanvas)
	{
		return;
	}
	ProviderPollSeconds -= InDeltaTime;
	if (ProviderPollSeconds <= 0.0f)
	{
		ProviderPollSeconds = 0.25f;
		PollProviders();
	}
	StatusPollSeconds -= InDeltaTime;
	if (StatusPollSeconds <= 0.0f)
	{
		StatusPollSeconds = 0.1f;
		PollStatus();
	}
	TickPrompts(InDeltaTime);
	// Panels keep clear of the tray along the top.
	const FVector2D Canvas = RootCanvas->GetCachedGeometry().GetLocalSize();
	float			TopInset = TrayBorder ? 4.0f + static_cast<float>(TrayBorder->GetDesiredSize().Y) + FloatingMargin : FloatingMargin;
	if (StatusBorder && StatusBorder->GetVisibility() != ESlateVisibility::Collapsed)
	{
		// Beside the tray while there is room to its left; under it, pushing the panels down, once a narrow
		// window or a long status would run the two together.
		const FVector2D Status = StatusBorder->GetDesiredSize();
		const float		TrayWidth = TrayBorder ? static_cast<float>(TrayBorder->GetDesiredSize().X) : 0.0f;
		const float		TrayLeft = (static_cast<float>(Canvas.X) - TrayWidth) * 0.5f;
		const bool		bBeside = Canvas.X <= 0.0 || FloatingMargin + static_cast<float>(Status.X) + FloatingMargin <= TrayLeft;
		const float		StatusTop = bBeside ? 4.0f : TopInset - FloatingMargin + 4.0f;
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(StatusBorder->Slot))
		{
			const FVector2D Wanted(FloatingMargin, StatusTop);
			if (!CanvasSlot->GetPosition().Equals(Wanted, 0.5))
			{
				CanvasSlot->SetPosition(Wanted);
			}
		}
		if (!bBeside)
		{
			TopInset += static_cast<float>(Status.Y) + 4.0f;
		}
		else
		{
			// Beside the tray: the panels keep clear of whichever of the two reaches lower.
			TopInset = FMath::Max(TopInset, 4.0f + static_cast<float>(Status.Y) + FloatingMargin);
		}
	}
	for (URammsFloatingPanel* Panel : FloatingPanels)
	{
		if (Panel)
		{
			Panel->UpdateLayout(Canvas, FloatingMargin, TopInset, InDeltaTime);
		}
	}
	if (bTrayDirty)
	{
		RebuildTray();
	}
}

void URammsSimLayout::NativeDestruct()
{
	// Provider content goes with the HUD (its makers hold it weakly and fall
	// back to drawing without the HUD).
	for (URammsFloatingPanel* Panel : FloatingPanels)
	{
		if (URammsSlateHostWidget* Host = Panel ? Cast<URammsSlateHostWidget>(Panel->GetContent()) : nullptr)
		{
			Host->SetSlateContent(nullptr);
		}
	}
	Super::NativeDestruct();
}

void URammsSimLayout::PollProviders()
{
	// Panels offered by components on the pawn the player controls.
	const APlayerController* Player = GetOwningPlayer();
	const APawn*			 Pawn = Player ? Player->GetPawn() : nullptr;
	TSet<FName>				 Offered;
	if (Pawn)
	{
		for (UActorComponent* Component : Pawn->GetComponents())
		{
			const IRammsHUDPanelProvider* Provider = Cast<IRammsHUDPanelProvider>(Component);
			if (!Provider)
			{
				continue;
			}
			TArray<FRammsHUDPanelDesc> Panels;
			Provider->GetHUDPanels(Panels);
			for (FRammsHUDPanelDesc& Desc : Panels)
			{
				if (Desc.Id.IsNone() || !Desc.MakeContent)
				{
					continue;
				}
				bool bAlready = false;
				Offered.Add(Desc.Id, &bAlready);
				if (bAlready)
				{
					continue; // another component offered this Id first
				}
				URammsFloatingPanel*		   Panel = FindFloatingPanel(Desc.Id);
				const TWeakObjectPtr<UObject>* By = ProviderOf.Find(Desc.Id);
				if (Panel && By && By->Get() == Component)
				{
					ProviderWanted.Add(Desc.Id, MoveTemp(Desc.IsWanted));
					continue;
				}
				if (!Panel)
				{
					Panel = AddFloatingPanel(Desc.Id, Desc.Title, Desc.DefaultPlacement, Desc.bResizable);
					if (!Panel)
					{
						continue;
					}
					Panel->DefaultSize = Desc.DefaultSize;
				}
				// New, or now offered by another component (another pawn):
				// fresh content from its maker; the old lets go of its own.
				if (URammsSlateHostWidget* Old = Cast<URammsSlateHostWidget>(Panel->GetContent()))
				{
					Old->SetSlateContent(nullptr);
				}
				URammsSlateHostWidget* Host = WidgetTree->ConstructWidget<URammsSlateHostWidget>(URammsSlateHostWidget::StaticClass());
				Host->SetSlateContent(Desc.MakeContent());
				Panel->SetContent(Host);
				ProviderOf.Add(Desc.Id, Component);
				ProviderWanted.Add(Desc.Id, MoveTemp(Desc.IsWanted));
			}
		}
	}
	for (const TPair<FName, TWeakObjectPtr<UObject>>& Entry : ProviderOf)
	{
		if (URammsFloatingPanel* Panel = FindFloatingPanel(Entry.Key))
		{
			const TFunction<bool()>* Wanted = ProviderWanted.Find(Entry.Key);
			Panel->SetWanted(Offered.Contains(Entry.Key) && Entry.Value.IsValid() && (!Wanted || !*Wanted || (*Wanted)()));
		}
	}
}

// ---------------------------------------------------------------------------
// Panels
// ---------------------------------------------------------------------------

TArray<URammsFloatingPanel*> URammsSimLayout::GetFloatingPanels() const
{
	TArray<URammsFloatingPanel*> Out;
	for (URammsFloatingPanel* Panel : FloatingPanels)
	{
		if (Panel)
		{
			Out.Add(Panel);
		}
	}
	return Out;
}

URammsFloatingPanel* URammsSimLayout::FindFloatingPanel(FName PanelId) const
{
	for (URammsFloatingPanel* Panel : FloatingPanels)
	{
		if (Panel && Panel->PanelId == PanelId)
		{
			return Panel;
		}
	}
	return nullptr;
}

void URammsSimLayout::ResetPanelLayout()
{
	URammsHUDLayoutUserSettings::Get()->ResetAll();
	for (URammsFloatingPanel* Panel : FloatingPanels)
	{
		if (Panel)
		{
			Panel->ResetToDefault();
		}
	}
	SetJoystickHidden(false);
	bTrayDirty = true;
}

void URammsSimLayout::SetAllPanelsHidden(bool bHidden)
{
	for (URammsFloatingPanel* Panel : FloatingPanels)
	{
		if (Panel)
		{
			Panel->SetPanelHidden(bHidden);
		}
	}
	SetJoystickHidden(bHidden);
}

bool URammsSimLayout::AreAllPanelsHidden() const
{
	for (const URammsFloatingPanel* Panel : FloatingPanels)
	{
		if (Panel && Panel->IsWanted() && !Panel->IsPanelHidden())
		{
			return false;
		}
	}
	return bJoystickHidden || bStickUnused || !JoystickSlot;
}

void URammsSimLayout::ApplyJoystickVisibility()
{
	if (JoystickSlot)
	{
		JoystickSlot->SetVisibility(bJoystickHidden || bStickUnused ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void URammsSimLayout::SetJoystickHidden(bool bHidden)
{
	bJoystickHidden = bHidden;
	ApplyJoystickVisibility();
	FRammsFloatingPanelState State;
	URammsHUDLayoutUserSettings::Get()->Find(JoystickId, State);
	State.Id = JoystickId;
	State.bHidden = bHidden;
	URammsHUDLayoutUserSettings::Get()->Store(State);
	bTrayDirty = true;
}

void URammsSimLayout::HandlePanelChanged(URammsFloatingPanel* /*Panel*/)
{
	bTrayDirty = true;
}

void URammsSimLayout::HandleResetClicked()
{
	ResetPanelLayout();
}

// ---------------------------------------------------------------------------
// The panel tray
// ---------------------------------------------------------------------------

void URammsSimLayout::RebuildTray()
{
	bTrayDirty = false;
	if (!TrayRow || !WidgetTree)
	{
		return;
	}
	TrayRow->ClearChildren();
	TrayEntries.Reset();
	// "Panels", a button per panel marked with its state, a divider, then the
	// actions: compact, so the tray stays a slim strip.
	UTextBlock* Heading = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Heading->SetText(FText::FromString(TEXT("Panels")));
	if (Style)
	{
		Heading->SetFont(Style->Typography.Caption);
		Heading->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
	}
	if (UHorizontalBoxSlot* S = TrayRow->AddChildToHorizontalBox(Heading))
	{
		S->SetPadding(FMargin(2.0f, 0.0f, 6.0f, 0.0f));
		S->SetVerticalAlignment(VAlign_Center);
	}
	auto Add = [this](URammsFloatingPanel* Panel, FName Special, const FText& Label) {
		URammsButton* Button = WidgetTree->ConstructWidget<URammsButton>(URammsButton::StaticClass());
		Button->SetText(Label);
		Button->SetCompact(true);
		if (Style)
		{
			Button->SetStyle(Style);
		}
		if (UHorizontalBoxSlot* S = TrayRow->AddChildToHorizontalBox(Button))
		{
			S->SetPadding(FMargin(2.0f, 0.0f));
			S->SetVerticalAlignment(VAlign_Center);
		}
		URammsPanelTrayEntry* Entry = NewObject<URammsPanelTrayEntry>(this);
		Entry->Panel = Panel;
		Entry->Layout = this;
		Entry->Special = Special;
		Entry->Label = Label;
		Entry->Button = Button;
		Button->OnClicked.AddDynamic(Entry, &URammsPanelTrayEntry::HandleClicked);
		Entry->Refresh();
		TrayEntries.Add(Entry);
	};
	for (URammsFloatingPanel* Panel : FloatingPanels)
	{
		// A panel that has nothing to show (its component does not want it) is
		// not offered.
		if (Panel && Panel->IsWanted())
		{
			Add(Panel, NAME_None, Panel->Title);
		}
	}
	if (JoystickSlot && !bStickUnused)
	{
		Add(nullptr, JoystickId, FText::FromString(TEXT("Joystick")));
	}
	// Sim look: the robot's own actions the settings want one click away (pause the sim), after a divider.
	const bool bSim = URammsUIStyle::UsesSimLook(Style);
	const auto AddDivider = [this]() {
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(1.0f);
		Box->SetHeightOverride(16.0f);
		UBorder* Rule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Rule->SetBrushColor(Style ? Style->Colors.Border : FLinearColor(0.3f, 0.3f, 0.3f, 1.0f));
		Box->AddChild(Rule);
		if (UHorizontalBoxSlot* S = TrayRow->AddChildToHorizontalBox(Box))
		{
			S->SetPadding(FMargin(6.0f, 0.0f));
			S->SetVerticalAlignment(VAlign_Center);
		}
	};
	if (bSim && TrayAxes.Num() > 0)
	{
		AddDivider();
		for (const FRammsControlAxis& Axis : TrayAxes)
		{
			Add(nullptr, NAME_None, URammsControlRow::SimLabel(Axis.DisplayName.IsEmpty() ? FText::FromName(Axis.Id) : Axis.DisplayName));
			TrayEntries.Last()->Control = Axis.Id;
			TrayEntries.Last()->Refresh();
		}
	}
	USizeBox* Divider = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Divider->SetWidthOverride(1.0f);
	Divider->SetHeightOverride(16.0f);
	UBorder* Line = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Line->SetBrushColor(Style ? Style->Colors.Border : FLinearColor(0.3f, 0.3f, 0.3f, 1.0f));
	Divider->AddChild(Line);
	if (UHorizontalBoxSlot* S = TrayRow->AddChildToHorizontalBox(Divider))
	{
		S->SetPadding(FMargin(6.0f, 0.0f));
		S->SetVerticalAlignment(VAlign_Center);
	}
	Add(nullptr, AllId, FText::FromString(AreAllPanelsHidden() ? TEXT("Show all") : TEXT("Hide all")));
	ResetButton = WidgetTree->ConstructWidget<URammsButton>(URammsButton::StaticClass());
	// "Reset layout" where the robot's own Reset (the sim's) can share the tray or the screen with it.
	ResetButton->SetText(FText::FromString(bSim ? TEXT("Reset layout") : TEXT("Reset")));
	ResetButton->SetCompact(true);
	ResetButton->SetVariant(ERammsButtonVariant::Ghost);
	ResetButton->SetToolTipText(FText::FromString(TEXT("Every panel back where it started (ramms.UI.ResetLayout)")));
	if (Style)
	{
		ResetButton->SetStyle(Style);
	}
	ResetButton->OnClicked.AddDynamic(this, &URammsSimLayout::HandleResetClicked);
	if (UHorizontalBoxSlot* S = TrayRow->AddChildToHorizontalBox(ResetButton))
	{
		S->SetPadding(FMargin(2.0f, 0.0f));
		S->SetVerticalAlignment(VAlign_Center);
	}
}

void URammsPanelTrayEntry::HandleClicked()
{
	URammsSimLayout* L = Layout.Get();
	if (!Control.IsNone())
	{
		if (L)
		{
			L->TriggerControl(Control);
		}
		return;
	}
	if (Special == AllId)
	{
		if (L)
		{
			L->SetAllPanelsHidden(!L->AreAllPanelsHidden());
		}
		return;
	}
	if (Special == JoystickId)
	{
		if (L)
		{
			L->SetJoystickHidden(!L->IsJoystickHidden());
		}
		return;
	}
	URammsFloatingPanel* P = Panel.Get();
	if (!P)
	{
		return;
	}
	// In a window: to the front. Hidden: shown. Minimised: restored. Shown: hidden.
	if (P->IsPoppedOut())
	{
		P->FocusWindow();
	}
	else if (P->IsPanelHidden())
	{
		P->SetPanelHidden(false);
		P->BringToFront();
	}
	else if (P->IsMinimized())
	{
		P->SetMinimized(false);
		P->BringToFront();
	}
	else
	{
		P->SetPanelHidden(true);
	}
}

void URammsPanelTrayEntry::Refresh()
{
	if (!Button)
	{
		return;
	}
	// A mark for the state: shown, minimised, hidden, in a window of its own.
	const TCHAR* Mark = nullptr;
	FString		 Tip;
	if (const URammsFloatingPanel* P = Panel.Get())
	{
		Mark = P->IsPoppedOut() ? TEXT("\u2197") : P->IsPanelHidden() ? TEXT("\u25CB")
			: P->IsMinimized()										  ? TEXT("\u25AC")
																	  : TEXT("\u25CF");
		Tip = P->IsPoppedOut()	 ? TEXT("In its own window: click to bring it to the front")
			: P->IsPanelHidden() ? TEXT("Hidden: click to show")
			: P->IsMinimized()	 ? TEXT("Minimised: click to restore")
								 : TEXT("Shown: click to hide");
		Button->SetVariant(P->IsPoppedOut() ? ERammsButtonVariant::Primary
				: P->IsPanelHidden()		? ERammsButtonVariant::Ghost
											: ERammsButtonVariant::Secondary);
	}
	else if (!Control.IsNone())
	{
		Tip = FString::Printf(TEXT("%s (the robot's %s control)"), *Label.ToString(), *Control.ToString());
		// The weight its row has in the panel: red for what the settings name a stop or a reset, filled for a main action.
		const URammsControlHUDSettings* Settings = GetDefault<URammsControlHUDSettings>();
		Button->SetVariant(URammsControlHUDSettings::MatchesAny(Settings->DangerControls, Control) ? ERammsButtonVariant::Error
				: URammsControlHUDSettings::MatchesAny(Settings->PrimaryControls, Control)		   ? ERammsButtonVariant::Primary
																								   : ERammsButtonVariant::Secondary);
	}
	else if (Special == JoystickId)
	{
		const URammsSimLayout* L = Layout.Get();
		const bool			   bShown = L && !L->IsJoystickHidden();
		Mark = bShown ? TEXT("\u25CF") : TEXT("\u25CB");
		Tip = bShown ? TEXT("Shown: click to hide") : TEXT("Hidden: click to show");
		Button->SetVariant(bShown ? ERammsButtonVariant::Secondary : ERammsButtonVariant::Ghost);
	}
	else
	{
		Tip = TEXT("Hide or show every panel (ramms.UI.Panels)");
		Button->SetVariant(ERammsButtonVariant::Ghost);
	}
	Button->SetText(Mark ? FText::FromString(FString::Printf(TEXT("%s  %s"), Mark, *Label.ToString())) : Label);
	Button->SetToolTipText(FText::FromString(Tip));
}

// ---------------------------------------------------------------------------
// Console commands
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Sim look: status strip, prompts, tray actions
// ---------------------------------------------------------------------------

namespace
{
	/** A control's reading as a few words: an Enum's choice, a Text's words, Yes / No for a 0..1 state, a number. */
	FString StatusWords(UObject* Surface, const FRammsControlAxis& Axis)
	{
		const float Value = IRammsControlSink::Execute_GetAxisValue(Surface, Axis.Id);
		if (Axis.IsText())
		{
			FText Words;
			IRammsControlSink::Execute_GetAxisText(Surface, Axis.Id, Words);
			return Words.ToString();
		}
		if (Axis.Kind == ERammsControlKind::Enum && Axis.EnumLabels.Num() > 0)
		{
			return Axis.EnumLabels[FMath::Clamp(FMath::RoundToInt(Value), 0, Axis.EnumLabels.Num() - 1)].ToString();
		}
		if (Axis.Units == ERammsControlUnits::Normalized && FMath::IsNearlyEqual(Axis.Range.X, 0.0) && FMath::IsNearlyEqual(Axis.Range.Y, 1.0))
		{
			return Value >= 0.99f ? TEXT("Yes") : Value <= 0.01f ? TEXT("No")
																 : FString::Printf(TEXT("%.0f%%"), Value * 100.0f);
		}
		return FString::Printf(TEXT("%.2f"), Value);
	}

	/** How far below its place a prompt starts its fade in, and ends its fade out. */
	const float PromptRisePixels = 10.0f;

	/** Whether two prompts are one message with other numbers ("12.4 cm to go" then "7.6 cm to go"): a
	 *  count ticking down is the same message still on show, not a new one. */
	bool SameMessage(const FString& A, const FString& B)
	{
		const auto Words = [](const FString& In) {
			FString Out;
			Out.Reserve(In.Len());
			for (const TCHAR Ch : In)
			{
				if (!FChar::IsDigit(Ch) && Ch != TEXT('.') && Ch != TEXT(',') && Ch != TEXT('-'))
				{
					Out.AppendChar(Ch);
				}
			}
			return Out;
		};
		return Words(A) == Words(B);
	}

	bool IsSurface(const UObject* Object)
	{
		return Object && Object->GetClass()->ImplementsInterface(URammsControlSurfaceProvider::StaticClass())
			&& Object->GetClass()->ImplementsInterface(URammsControlSink::StaticClass());
	}
} // namespace

void URammsSimLayout::PollStatus()
{
	if (!StatusBorder || !PromptBox)
	{
		return;
	}
	const bool					  bSim = URammsUIStyle::UsesSimLook(Style);
	UWorld*						  World = GetWorld();
	URammsControlSurfaceRegistry* Registry = bSim && World ? World->GetSubsystem<URammsControlSurfaceRegistry>() : nullptr;
	// The robot the HUD's panel shows -- the strip, the prompts and the tray's buttons are about that one,
	// not whichever registered first -- and the registry's first only where no panel says.
	UObject* Surface = nullptr;
	if (bSim)
	{
		const URammsControlHUDSubsystem* HUD = URammsControlHUDSubsystem::Get(this);
		const URammsControlSurfacePanel* Panel = HUD ? HUD->GetSurfacePanel() : nullptr;
		if (Panel)
		{
			Surface = Panel->GetTargetSurface();
			StatusSource = Panel->Source;
		}
		else if (Registry)
		{
			Surface = Registry->FindControlSurface();
		}
	}
	if (!IsSurface(Surface))
	{
		Surface = nullptr;
	}
	const int32 Version = Surface ? IRammsControlSurfaceProvider::Execute_GetControlSurfaceVersion(Surface) : -1;
	if (Surface != StatusSurface.Get() || Version != StatusVersion)
	{
		RebuildStatus(Surface);
		StatusSurface = Surface;
		StatusVersion = Version;
	}
	if (!Surface)
	{
		return;
	}
	for (int32 I = 0; I < StatusAxes.Num() && I < StatusValues.Num(); ++I)
	{
		const FText Words = FText::FromString(StatusWords(Surface, StatusAxes[I]));
		if (StatusValues[I] && !StatusValues[I]->GetText().EqualTo(Words))
		{
			StatusValues[I]->SetText(Words);
		}
	}
	for (int32 I = 0; I < PromptAxes.Num() && I < PromptTexts.Num() && I < PromptStates.Num(); ++I)
	{
		const float Reading = IRammsControlSink::Execute_GetAxisValue(Surface, PromptAxes[I].Id);
		const uint8 Severity = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Reading), 0, static_cast<int32>(ERammsControlTextSeverity::Fault)));
		FText		Words;
		IRammsControlSink::Execute_GetAxisText(Surface, PromptAxes[I].Id, Words);
		FPromptState& State = PromptStates[I];
		State.Wanted = Words.IsEmpty() ? 0 : Severity;
		if (State.Wanted == 0 || !PromptTexts[I])
		{
			continue; // nothing to say: TickPrompts lets what is up have its time, then fades it
		}
		const FText Old = PromptTexts[I]->GetText();
		if (!Old.EqualTo(Words))
		{
			if (State.Drawn == 0 || !SameMessage(Old.ToString(), Words.ToString()))
			{
				State.ShownSeconds = 0.0f; // a new message earns its own time on show
			}
			PromptTexts[I]->SetText(Words);
		}
		if (State.Drawn != State.Wanted)
		{
			State.Drawn = State.Wanted;
			StyleStatus();
		}
	}
}

void URammsSimLayout::TickPrompts(float DeltaSeconds)
{
	if (PromptStates.Num() == 0)
	{
		return;
	}
	const URammsControlHUDSettings* Settings = GetDefault<URammsControlHUDSettings>();
	const float						Hold = FMath::Max(Settings->PromptMinSeconds, 0.0f);
	const float						Fade = FMath::Max(Settings->PromptFadeSeconds, 0.0f);
	// A hitch must not swallow the fade or the hold in one step.
	const float Delta = FMath::Clamp(DeltaSeconds, 0.0f, 0.1f);
	for (int32 I = 0; I < PromptStates.Num() && I < PromptFrames.Num(); ++I)
	{
		FPromptState& State = PromptStates[I];
		UBorder*	  Frame = PromptFrames[I];
		if (!Frame || State.Drawn == 0)
		{
			continue; // hidden, and the robot has nothing for it
		}
		State.ShownSeconds += Delta;
		// Up while the robot says so, and after that until it has had long enough to be read: a message that
		// came and went within a moment would otherwise only flicker.
		const bool	bUp = State.Wanted > 0 || State.ShownSeconds < Hold;
		const float Step = Fade > 0.0f ? Delta / Fade : 1.0f;
		State.Alpha = FMath::Clamp(State.Alpha + (bUp ? Step : -Step), 0.0f, 1.0f);
		if (!bUp && State.Alpha <= 0.0f)
		{
			State = FPromptState();
			Frame->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		// Eased, with a short rise: it settles into place rather than switching on.
		const float Eased = FMath::InterpEaseOut(0.0f, 1.0f, State.Alpha, 2.0f);
		Frame->SetRenderOpacity(Eased);
		Frame->SetRenderTranslation(FVector2D(0.0, (1.0 - Eased) * PromptRisePixels));
		if (Frame->GetVisibility() == ESlateVisibility::Collapsed)
		{
			Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void URammsSimLayout::RebuildStatus(UObject* Surface)
{
	StatusRow->ClearChildren();
	StatusNames.Reset();
	StatusValues.Reset();
	StatusAxes.Reset();
	// A prompt on show stays on show through a rebuild: the surface changes version when the drive mode
	// does, which is just when a crossing starts and its prompts matter. Kept by control id, for the same robot.
	TMap<FName, TPair<FPromptState, FText>> PromptsUp;
	if (Surface && Surface == StatusSurface.Get())
	{
		for (int32 I = 0; I < PromptAxes.Num() && I < PromptStates.Num() && I < PromptTexts.Num(); ++I)
		{
			if (PromptStates[I].Drawn > 0 && PromptTexts[I])
			{
				PromptsUp.Add(PromptAxes[I].Id, TPair<FPromptState, FText>(PromptStates[I], PromptTexts[I]->GetText()));
			}
		}
	}
	PromptBox->ClearChildren();
	PromptFrames.Reset();
	PromptTexts.Reset();
	PromptAxes.Reset();
	PromptStates.Reset();
	const int32 TrayBefore = TrayAxes.Num();
	TrayAxes.Reset();
	const bool bStickWasUnused = bStickUnused;
	bStickUnused = false;
	if (!Surface)
	{
		StatusBorder->SetVisibility(ESlateVisibility::Collapsed);
		bTrayDirty = bTrayDirty || TrayBefore > 0 || bStickWasUnused;
		ApplyJoystickVisibility();
		return;
	}
	const URammsControlHUDSettings* Settings = GetDefault<URammsControlHUDSettings>();
	const FRammsControlSurface		Described = IRammsControlSurfaceProvider::Execute_GetControlSurface(Surface);
	const auto						Find = [&Described](FName Id) {
		 return Described.Axes.FindByPredicate([Id](const FRammsControlAxis& Axis) { return Axis.Id == Id; });
	};

	// The drive stick has nothing to drive on a robot with neither of its controls (the robot picker, before
	// a robot is chosen): it and its tray button are left out rather than sitting there dead.
	if (const URammsControlHUDSubsystem* HUD = URammsControlHUDSubsystem::Get(this))
	{
		const URammsSurfaceJoystick* Stick = HUD->GetJoystick();
		bStickUnused = Stick && !Find(Stick->ControlIdX) && !Find(Stick->ControlIdY);
	}
	ApplyJoystickVisibility();

	for (const FName& Id : Settings->StatusControls)
	{
		const FRammsControlAxis* Axis = Find(Id);
		if (!Axis || Axis->IsAction() || !Axis->bReadback)
		{
			continue; // not on this robot, or nothing to read
		}
		UBorder* Chip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Chip->SetPadding(FMargin(9.0f, 4.0f));
		UHorizontalBox* Words = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock*		Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Name->SetText(URammsControlRow::SimLabel(Axis->DisplayName.IsEmpty() ? FText::FromName(Axis->Id) : Axis->DisplayName));
		Words->AddChildToHorizontalBox(Name);
		UTextBlock* Value = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		if (UHorizontalBoxSlot* ValueSlot = Words->AddChildToHorizontalBox(Value))
		{
			ValueSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
		}
		Chip->AddChild(Words);
		if (UHorizontalBoxSlot* ChipSlot = StatusRow->AddChildToHorizontalBox(Chip))
		{
			ChipSlot->SetPadding(FMargin(StatusAxes.Num() > 0 ? 4.0f : 0.0f, 0.0f, 0.0f, 0.0f));
			ChipSlot->SetVerticalAlignment(VAlign_Center);
		}
		StatusNames.Add(Name);
		StatusValues.Add(Value);
		StatusAxes.Add(*Axis);
	}
	StatusBorder->SetVisibility(StatusAxes.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (Settings->bShowPrompts)
	{
		for (const FRammsControlAxis& Axis : Described.Axes)
		{
			if (!Axis.IsText() || !Axis.bReadback)
			{
				continue;
			}
			UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Frame->SetPadding(FMargin(18.0f, 10.0f));
			Frame->SetVisibility(ESlateVisibility::Collapsed);
			Frame->SetRenderOpacity(0.0f); // TickPrompts fades it in when the robot has something to say
			UTextBlock* Words = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Words->SetJustification(ETextJustify::Center);
			Words->SetAutoWrapText(true); // a long status folds rather than running off a narrow window
			Words->SetWrapTextAt(640.0f);
			Frame->AddChild(Words);
			if (UVerticalBoxSlot* FrameSlot = PromptBox->AddChildToVerticalBox(Frame))
			{
				FrameSlot->SetHorizontalAlignment(HAlign_Center);
				FrameSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
			}
			PromptFrames.Add(Frame);
			PromptTexts.Add(Words);
			PromptAxes.Add(Axis);
			FPromptState& State = PromptStates.AddDefaulted_GetRef();
			if (const TPair<FPromptState, FText>* Up = PromptsUp.Find(Axis.Id))
			{
				State = Up->Key;
				Words->SetText(Up->Value);
				Frame->SetVisibility(ESlateVisibility::HitTestInvisible); // TickPrompts, next, sets how much of it shows
			}
		}
	}

	for (const FName& Id : Settings->TrayControls)
	{
		const FRammsControlAxis* Axis = Find(Id);
		if (Axis && Axis->IsAction())
		{
			TrayAxes.Add(*Axis);
		}
	}
	bTrayDirty = true;
	StyleStatus();
}

void URammsSimLayout::StyleStatus()
{
	if (!Style || !StatusBorder)
	{
		return;
	}
	FLinearColor Back = Style->Colors.Background;
	Back.A = 0.6f;
	StatusBorder->SetBrush(URammsUIStyle::MakeRoundedBoxBrush(Back, Style->Border.CornerRadiusLarge));
	for (int32 I = 0; I < StatusNames.Num(); ++I)
	{
		if (StatusNames[I])
		{
			StatusNames[I]->SetFont(Style->Typography.Caption);
			StatusNames[I]->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
			if (UBorder* Chip = Cast<UBorder>(StatusNames[I]->GetParent() ? StatusNames[I]->GetParent()->GetParent() : nullptr))
			{
				Chip->SetBrush(URammsUIStyle::MakeRoundedBoxBrush(Style->Colors.Secondary, 12.0f, Style->Colors.Border, Style->Border.BorderWidth));
			}
		}
		if (StatusValues.IsValidIndex(I) && StatusValues[I])
		{
			StatusValues[I]->SetFont(Style->Typography.Caption);
			StatusValues[I]->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
		}
	}
	for (int32 I = 0; I < PromptFrames.Num(); ++I)
	{
		const uint8 Severity = PromptStates.IsValidIndex(I) ? PromptStates[I].Drawn : 0;
		if (PromptFrames[I])
		{
			// An outline in the severity's colour: the words stay the text colour, readable on the dark frame.
			// Whether it shows is TickPrompts' business.
			const FLinearColor Edge = Severity >= static_cast<uint8>(ERammsControlTextSeverity::Fault) ? Style->Colors.Error : Style->Colors.Warning;
			PromptFrames[I]->SetBrush(URammsUIStyle::MakeRoundedBoxBrush(Style->Colors.Background, Style->Border.CornerRadiusLarge, Edge, 2.0f));
		}
		if (PromptTexts.IsValidIndex(I) && PromptTexts[I])
		{
			PromptTexts[I]->SetFont(Style->Typography.HeadingSmall);
			PromptTexts[I]->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
		}
	}
}

void URammsSimLayout::TriggerControl(FName ControlId)
{
	UObject* Surface = StatusSurface.Get();
	if (IsSurface(Surface))
	{
		IRammsControlSink::Execute_TriggerAction(Surface, ControlId, StatusSource);
	}
}

TArray<FString> URammsSimLayout::GetStatusChips() const
{
	TArray<FString> Out;
	for (int32 I = 0; I < StatusNames.Num() && I < StatusValues.Num(); ++I)
	{
		if (StatusNames[I] && StatusValues[I])
		{
			Out.Add(FString::Printf(TEXT("%s: %s"), *StatusNames[I]->GetText().ToString(), *StatusValues[I]->GetText().ToString()));
		}
	}
	return Out;
}

TArray<FString> URammsSimLayout::GetPrompts() const
{
	TArray<FString> Out;
	for (int32 I = 0; I < PromptTexts.Num(); ++I)
	{
		if (PromptTexts[I] && PromptStates.IsValidIndex(I) && PromptStates[I].Drawn > 0)
		{
			Out.Add(PromptTexts[I]->GetText().ToString());
		}
	}
	return Out;
}

namespace
{
	URammsSimLayout* SimLayoutIn(UWorld* World)
	{
		URammsControlHUDSubsystem* HUD = World ? URammsControlHUDSubsystem::Get(World) : nullptr;
		URammsLayoutHost*		   Host = HUD ? HUD->GetHost() : nullptr;
		return Host ? Cast<URammsSimLayout>(Host->GetActiveLayout()) : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs ResetLayoutCommand(TEXT("ramms.UI.ResetLayout"),
		TEXT("Every HUD panel back where it started, shown and docked; the stored layout forgotten."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) {
			if (URammsSimLayout* Layout = SimLayoutIn(World))
			{
				Layout->ResetPanelLayout();
			}
			else
			{
				URammsHUDLayoutUserSettings::Get()->ResetAll();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs PanelsCommand(TEXT("ramms.UI.Panels"),
		TEXT("ramms.UI.Panels [show|hide|toggle]: hide or show every HUD panel and the joystick."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) {
			URammsSimLayout* Layout = SimLayoutIn(World);
			if (!Layout)
			{
				return;
			}
			const FString What = Args.Num() > 0 ? Args[0].ToLower() : FString(TEXT("toggle"));
			Layout->SetAllPanelsHidden(What == TEXT("hide") || (What != TEXT("show") && !Layout->AreAllPanelsHidden()));
		}));
} // namespace
