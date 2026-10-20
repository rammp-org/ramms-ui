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
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "RammsControlHUDSettings.h"
#include "RammsControlHUDSubsystem.h"
#include "RammsHUDLayoutUserSettings.h"
#include "RammsHUDPanelProvider.h"
#include "UI/RammsButton.h"
#include "UI/RammsFloatingPanel.h"
#include "UI/RammsLayoutHost.h"
#include "UI/RammsSlateHostWidget.h"
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
	// Panels keep clear of the tray along the top.
	const FVector2D Canvas = RootCanvas->GetCachedGeometry().GetLocalSize();
	const float		TopInset = TrayBorder ? 4.0f + static_cast<float>(TrayBorder->GetDesiredSize().Y) + FloatingMargin : FloatingMargin;
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
	return bJoystickHidden || !JoystickSlot;
}

void URammsSimLayout::SetJoystickHidden(bool bHidden)
{
	bJoystickHidden = bHidden;
	if (JoystickSlot)
	{
		JoystickSlot->SetVisibility(bHidden ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
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
	if (JoystickSlot)
	{
		Add(nullptr, JoystickId, FText::FromString(TEXT("Joystick")));
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
	ResetButton->SetText(FText::FromString(TEXT("Reset")));
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
