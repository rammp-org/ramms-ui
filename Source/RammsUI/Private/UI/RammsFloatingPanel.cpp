// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsFloatingPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "RammsControlHUDSettings.h"
#include "RammsHUDLayoutUserSettings.h"
#include "UI/RammsButton.h"
#include "UI/RammsFloatingPanelContent.h"
#include "UI/RammsUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SWindow.h"

int32 URammsFloatingPanel::NextZOrder = 1;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

void URammsFloatingPanel::BuildWidgetTree()
{
	if (!WidgetTree || FrameBorder)
	{
		return;
	}
	// FrameSize keeps the panel's width while it is minimised to its title bar.
	FrameSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FrameSize"));
	WidgetTree->RootWidget = FrameSize;

	// The frame over a soft shadow a few units down, to lift it off the scene.
	UOverlay* FrameStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("FrameStack"));
	FrameSize->AddChild(FrameStack);
	Shadow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Shadow"));
	Shadow->Background = URammsUIStyle::MakeRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), 8.0f);
	Shadow->SetVisibility(ESlateVisibility::HitTestInvisible);
	Shadow->SetRenderTranslation(FVector2D(0.0f, 3.0f));
	if (UOverlaySlot* S = FrameStack->AddChildToOverlay(Shadow))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	FrameBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FrameBorder"));
	FrameBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(FLinearColor(0.06f, 0.06f, 0.08f, 0.94f), 8.0f,
		FLinearColor(0.3f, 0.3f, 0.3f, 0.8f), 1.0f);
	FrameBorder->SetPadding(FMargin(1.0f));
	if (UOverlaySlot* S = FrameStack->AddChildToOverlay(FrameBorder))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	FrameBorder->AddChild(Column);

	// The title bar: the title, then pop out, minimise and close.
	// Its width follows the body's (TickPanel): a long title clips rather than
	// widening the panel.
	TitleSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TitleSize"));
	Column->AddChildToVerticalBox(TitleSize);
	TitleBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TitleBar"));
	TitleBar->SetPadding(FMargin(8.0f, 2.0f, 2.0f, 2.0f));
	TitleBar->SetCursor(EMouseCursor::CardinalCross);
	TitleSize->AddChild(TitleBar);

	UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleRow"));
	TitleBar->AddChild(TitleRow);

	// An accent strip: the bar to take hold of.
	USizeBox* AccentSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AccentSize"));
	AccentSize->SetWidthOverride(3.0f);
	AccentSize->SetHeightOverride(16.0f);
	AccentSize->SetVisibility(ESlateVisibility::HitTestInvisible);
	Accent = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Accent"));
	Accent->Background = URammsUIStyle::MakeRoundedBoxBrush(FLinearColor(0.0f, 0.478f, 0.8f, 1.0f), 1.5f);
	AccentSize->AddChild(Accent);
	if (UHorizontalBoxSlot* S = TitleRow->AddChildToHorizontalBox(AccentSize))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	}

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(Title);
	TitleText->SetVisibility(ESlateVisibility::HitTestInvisible);
	TitleText->SetClipping(EWidgetClipping::ClipToBounds);
	if (UHorizontalBoxSlot* S = TitleRow->AddChildToHorizontalBox(TitleText))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	}
	auto AddButton = [this, TitleRow](const TCHAR* Name, const TCHAR* Label) {
		URammsButton* Button = WidgetTree->ConstructWidget<URammsButton>(URammsButton::StaticClass(), Name);
		Button->SetText(FText::FromString(Label));
		Button->SetVariant(ERammsButtonVariant::Ghost);
		// A target easy to hit: the whole box takes the click, the glyph
		// centred in it at a size to read.
		Button->SetCustomSize(FVector2D(32.0f, 28.0f));
		Button->SetContentPadding(FMargin(0.0f));
		Button->SetLabelFontSize(14);
		// Its own cursor, not the title bar's move cross: this is a button.
		Button->SetCursor(EMouseCursor::Hand);
		if (UHorizontalBoxSlot* S = TitleRow->AddChildToHorizontalBox(Button))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(2.0f, 0.0f, 0.0f, 0.0f));
		}
		return Button;
	};
	PopOutButton = AddButton(TEXT("PopOutButton"), TEXT("↗"));
	PopOutButton->SetToolTipText(FText::FromString(TEXT("Pop out into a window of its own (close that window to dock it back)")));
	PopOutButton->OnClicked.AddDynamic(this, &URammsFloatingPanel::HandlePopOutClicked);
	MinimizeButton = AddButton(TEXT("MinimizeButton"), TEXT("\u2013"));
	MinimizeButton->SetToolTipText(FText::FromString(TEXT("Minimise to the title bar")));
	MinimizeButton->OnClicked.AddDynamic(this, &URammsFloatingPanel::HandleMinimizeClicked);
	CloseButton = AddButton(TEXT("CloseButton"), TEXT("\u00D7"));
	CloseButton->SetToolTipText(FText::FromString(TEXT("Hide (the panel tray at the top shows it again)")));
	CloseButton->SetGhostHoverColor(FLinearColor(0.85f, 0.2f, 0.2f, 0.9f)); // red under the mouse
	CloseButton->OnClicked.AddDynamic(this, &URammsFloatingPanel::HandleCloseClicked);

	// The body: the content, and over its bottom-right corner the resize grip.
	BodyArea = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BodyArea"));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BodyArea))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	BodyBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BodyBox"));
	BodyBox->SetClipping(EWidgetClipping::ClipToBounds);
	if (UOverlaySlot* S = BodyArea->AddChildToOverlay(BodyBox))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	ContentSlot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ContentSlot"));
	BodyBox->AddChild(ContentSlot);

	Grip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Grip"));
	Grip->SetBrushColor(FLinearColor::Transparent);
	Grip->SetPadding(FMargin(6.0f, 2.0f, 3.0f, 0.0f));
	Grip->SetCursor(EMouseCursor::ResizeSouthEast);
	Grip->SetToolTipText(FText::FromString(TEXT("Drag to resize")));
	if (UOverlaySlot* S = BodyArea->AddChildToOverlay(Grip))
	{
		S->SetHorizontalAlignment(HAlign_Right);
		S->SetVerticalAlignment(VAlign_Bottom);
	}
	GripText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GripText"));
	GripText->SetText(FText::FromString(TEXT("◢")));
	GripText->SetVisibility(ESlateVisibility::HitTestInvisible);
	Grip->AddChild(GripText);

	ApplyVisibility();
}

void URammsFloatingPanel::ResetCachedWidgets()
{
	FrameSize = nullptr;
	Shadow = nullptr;
	FrameBorder = nullptr;
	Accent = nullptr;
	TitleSize = nullptr;
	TitleBar = nullptr;
	TitleText = nullptr;
	PopOutButton = nullptr;
	MinimizeButton = nullptr;
	CloseButton = nullptr;
	BodyArea = nullptr;
	BodyBox = nullptr;
	ContentSlot = nullptr;
	Grip = nullptr;
	GripText = nullptr;
}

void URammsFloatingPanel::ApplyStyle_Implementation()
{
	Super::ApplyStyle_Implementation();
	if (!Style)
	{
		return;
	}
	const float R = Style->Border.CornerRadiusLarge;
	if (Shadow)
	{
		Shadow->Background = URammsUIStyle::MakeRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), R);
	}
	if (FrameBorder)
	{
		FLinearColor Edge = Style->Colors.Border;
		Edge.A *= 0.8f;
		FrameBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(Style->Colors.Background, R, Edge, Style->Border.BorderWidth);
	}
	if (TitleBar)
	{
		TitleBar->Background = URammsUIStyle::MakeRoundedBoxBrushEx(Style->Colors.Surface, FVector4(R, R, 0.0f, 0.0f));
	}
	if (Accent)
	{
		Accent->Background = URammsUIStyle::MakeRoundedBoxBrush(Style->Colors.Primary, 1.5f);
	}
	if (TitleText)
	{
		TitleText->SetFont(Style->Typography.Body);
		TitleText->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}
	if (GripText)
	{
		FSlateFontInfo Font = Style->Typography.Caption;
		GripText->SetFont(Font);
		GripText->SetColorAndOpacity(FSlateColor(Style->Colors.TextDisabled));
	}
	for (URammsButton* Button : { PopOutButton.Get(), MinimizeButton.Get(), CloseButton.Get() })
	{
		if (Button)
		{
			Button->SetStyle(Style);
		}
	}
	if (CloseButton)
	{
		FLinearColor Red = Style->Colors.Error;
		Red.A = 0.9f;
		CloseButton->SetGhostHoverColor(Red);
	}
	WindowBrush = URammsUIStyle::MakeRoundedBoxBrush(Style->Colors.Background, 0.0f);
}

// ---------------------------------------------------------------------------
// Content
// ---------------------------------------------------------------------------

UPanelWidget* URammsFloatingPanel::GetContentSlot() const
{
	return ContentSlot;
}

UWidget* URammsFloatingPanel::GetContent() const
{
	if (PoppedContent)
	{
		return PoppedContent;
	}
	return ContentSlot && ContentSlot->GetChildrenCount() > 0 ? ContentSlot->GetChildAt(0) : nullptr;
}

void URammsFloatingPanel::SetContent(UWidget* Content)
{
	if (PoppedWindow.IsValid())
	{
		// In its window: the window shows the new content.
		PoppedContent = Content;
		PoppedWindow->SetContent(Content ? MakeWindowContent(Content) : SNullWidget::NullWidget);
		HostedContent = nullptr;
		return;
	}
	if (!ContentSlot)
	{
		return;
	}
	ContentSlot->ClearChildren();
	if (Content)
	{
		ContentSlot->AddChild(Content);
	}
}

FText URammsFloatingPanel::GetDisplayTitle() const
{
	if (const IRammsFloatingPanelContent* Content = Cast<IRammsFloatingPanelContent>(GetContent()))
	{
		const FText T = Content->GetFloatingTitle();
		if (!T.IsEmpty())
		{
			return T;
		}
	}
	return Title;
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

void URammsFloatingPanel::SetMinimized(bool bInMinimized)
{
	if (bMinimized == bInMinimized)
	{
		return;
	}
	// Keep the width it had: the title bar alone would shrink to its text.
	if (FrameSize)
	{
		if (bInMinimized)
		{
			FrameSize->SetMinDesiredWidth(static_cast<float>(GetCachedGeometry().GetLocalSize().X));
		}
		else
		{
			FrameSize->ClearMinDesiredWidth();
		}
	}
	if (bInMinimized)
	{
		// Placed as the whole panel while minimised, so its title bar stays
		// where the panel's top is (GetPlacementSize).
		const FVector2D Whole = GetCachedGeometry().GetLocalSize();
		if (Whole.Y > 0.0)
		{
			ExpandedSize = Whole;
		}
	}
	else
	{
		// Moved lower than the whole panel fits while minimised: it comes back
		// as low as it fits.
		Placement = FVector2D(FMath::Min(Placement.X, 1.0), FMath::Min(Placement.Y, 1.0));
	}
	bMinimized = bInMinimized;
	ApplyVisibility();
	SaveState();
	Changed();
}

void URammsFloatingPanel::SetPanelHidden(bool bInHidden)
{
	if (bUserHidden == bInHidden)
	{
		return;
	}
	bUserHidden = bInHidden;
	if (bUserHidden && PoppedWindow.IsValid())
	{
		CloseWindow(/*bKeepPoppedOut=*/false);
	}
	ApplyVisibility();
	SaveState();
	Changed();
}

void URammsFloatingPanel::SetWanted(bool bInWanted)
{
	if (bWanted == bInWanted)
	{
		return;
	}
	bWanted = bInWanted;
	// Not wanted while popped out: its window closes, to open again when it is.
	if (!bWanted && PoppedWindow.IsValid())
	{
		CloseWindow(/*bKeepPoppedOut=*/true);
	}
	ApplyVisibility();
	Changed();
}

bool URammsFloatingPanel::IsShownInViewport() const
{
	return bWanted && !bUserHidden && !PoppedWindow.IsValid();
}

void URammsFloatingPanel::SetPlacement(FVector2D InPlacement)
{
	Placement = FVector2D(FMath::Clamp(InPlacement.X, 0.0, 1.0), FMath::Clamp(InPlacement.Y, 0.0, 1.0));
	SaveState();
}

void URammsFloatingPanel::SetBodySize(FVector2D InSize)
{
	BodySize = FVector2D(InSize.X > 0.0 ? FMath::Max(InSize.X, MinSize.X) : 0.0, InSize.Y > 0.0 ? FMath::Max(InSize.Y, MinSize.Y) : 0.0);
	ApplySize();
	SaveState();
}

void URammsFloatingPanel::ResetToDefault()
{
	if (PoppedWindow.IsValid())
	{
		CloseWindow(/*bKeepPoppedOut=*/false);
	}
	bPopOutPending = false;
	Placement = FVector2D(-1.0, -1.0);
	BodySize = DefaultSize;
	WindowPosition = WindowSize = FVector2D::ZeroVector;
	bMinimized = false;
	bUserHidden = false;
	if (FrameSize)
	{
		FrameSize->ClearMinDesiredWidth();
	}
	ApplySize();
	ApplyVisibility();
	SaveState();
	Changed();
}

void URammsFloatingPanel::BringToFront()
{
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
	if (CanvasSlot && CanvasSlot->GetZOrder() != NextZOrder - 1)
	{
		CanvasSlot->SetZOrder(NextZOrder++);
	}
}

void URammsFloatingPanel::ApplyVisibility()
{
	SetVisibility(IsShownInViewport() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (BodyArea)
	{
		BodyArea->SetVisibility(bMinimized ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (MinimizeButton)
	{
		MinimizeButton->SetText(FText::FromString(bMinimized ? TEXT("+") : TEXT("\u2013")));
	}
	if (PopOutButton)
	{
		PopOutButton->SetVisibility(bAllowPopOut && IsPopOutAvailable() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (CloseButton)
	{
		CloseButton->SetVisibility(bAllowClose ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Grip)
	{
		Grip->SetVisibility(bResizable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void URammsFloatingPanel::ApplySize()
{
	if (!BodyBox)
	{
		return;
	}
	// No larger than the viewport has room for (BodySize is kept).
	const double Width = FitSize.X > 0.0 ? FMath::Min(BodySize.X, FitSize.X) : BodySize.X;
	const double Height = FitSize.Y > 0.0 ? FMath::Min(BodySize.Y, FitSize.Y) : BodySize.Y;
	if (BodySize.X > 0.0)
	{
		BodyBox->SetWidthOverride(static_cast<float>(Width));
	}
	else
	{
		BodyBox->ClearWidthOverride();
	}
	if (BodySize.Y > 0.0)
	{
		BodyBox->SetHeightOverride(static_cast<float>(Height));
	}
	else
	{
		BodyBox->ClearHeightOverride();
	}
}

void URammsFloatingPanel::LoadState()
{
	bStateLoaded = true;
	BodySize = DefaultSize;
	FRammsFloatingPanelState S;
	if (!PanelId.IsNone() && URammsHUDLayoutUserSettings::Get()->Find(PanelId, S))
	{
		Placement = S.Placement;
		BodySize = S.Size;
		bMinimized = S.bMinimized;
		bUserHidden = S.bHidden;
		bPopOutPending = S.bPoppedOut;
		WindowPosition = S.WindowPosition;
		WindowSize = S.WindowSize;
	}
	ApplySize();
	ApplyVisibility();
}

void URammsFloatingPanel::SaveState()
{
	if (PanelId.IsNone() || !bStateLoaded)
	{
		return;
	}
	if (PoppedWindow.IsValid())
	{
		WindowPosition = PoppedWindow->GetPositionInScreen();
		WindowSize = PoppedWindow->GetClientSizeInScreen();
	}
	FRammsFloatingPanelState S;
	S.Id = PanelId;
	S.Placement = Placement;
	S.Size = BodySize;
	S.bMinimized = bMinimized;
	S.bHidden = bUserHidden;
	S.bPoppedOut = PoppedWindow.IsValid() || bPopOutPending;
	S.WindowPosition = WindowPosition;
	S.WindowSize = WindowSize;
	URammsHUDLayoutUserSettings::Get()->Store(S);
}

void URammsFloatingPanel::Changed()
{
	OnPanelChanged.Broadcast(this);
}

// ---------------------------------------------------------------------------
// Layout, per tick
// ---------------------------------------------------------------------------

void URammsFloatingPanel::UpdateLayout(const FVector2D& CanvasSize, float Margin, float TopInset, float DeltaSeconds)
{
	if (!bStateLoaded)
	{
		LoadState();
	}
	TickPanel(DeltaSeconds);
	LastCanvasSize = CanvasSize;
	LastMargin = Margin;
	LastTopInset = FMath::Max(TopInset, Margin);
	const float		  Top = LastTopInset;
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
	if (!CanvasSlot || CanvasSize.X <= 0.0 || CanvasSize.Y <= 0.0)
	{
		return;
	}
	// A body the user has not sized keeps within the screen's height (its
	// content scrolls, if it can).
	if (BodyBox && TitleBar)
	{
		const float Room = static_cast<float>(CanvasSize.Y) - Top - Margin - static_cast<float>(TitleBar->GetDesiredSize().Y) - 4.0f;
		if (BodySize.Y > 0.0)
		{
			BodyBox->ClearMaxDesiredHeight();
		}
		else
		{
			BodyBox->SetMaxDesiredHeight(FMath::Max(Room, 60.0f));
		}
	}
	if (Drag != EDrag::None)
	{
		return;
	}
	// A body the user has sized fits the viewport too, so its grip stays in
	// reach: shown smaller while the viewport is (sized on a larger screen, or
	// the window made smaller since).
	if (BodyBox && !bMinimized && IsShownInViewport())
	{
		const FVector2D Chrome = GetDesiredSize() - BodyBox->GetDesiredSize();
		const FVector2D Fit(FMath::Max(CanvasSize.X - 2.0 * Margin - Chrome.X, MinSize.X), FMath::Max(CanvasSize.Y - Top - Margin - Chrome.Y, MinSize.Y));
		if (!Fit.Equals(FitSize, 0.5))
		{
			FitSize = Fit;
			ApplySize();
		}
	}
	const FVector2D Size = GetDesiredSize();
	if (!bMinimized && IsShownInViewport() && Size.Y > 0.0)
	{
		ExpandedSize = Size;
	}
	// The room its placement is a fraction of: around the whole panel, even
	// while minimised (at least a unit, so a minimised title bar keeps its
	// place below a panel that fills the viewport).
	const FVector2D Placed = GetPlacementSize();
	const FVector2D Free(FMath::Max(1.0, CanvasSize.X - Placed.X - 2.0 * Margin), FMath::Max(1.0, CanvasSize.Y - Placed.Y - Top - Margin));
	if (bPlaceFromPosition)
	{
		// Resized where it stood: its placement is where that leaves it.
		bPlaceFromPosition = false;
		const FVector2D At = CanvasSlot->GetPosition();
		Placement = FVector2D(Free.X > 0.0 ? FMath::Clamp((At.X - Margin) / Free.X, 0.0, 1.0) : 0.0,
			Free.Y > 0.0 ? FMath::Clamp((At.Y - Top) / Free.Y, 0.0, 1.0) : 0.0);
		SaveState();
	}
	const FVector2D P = Placement.X < 0.0 || Placement.Y < 0.0 ? DefaultPlacement : Placement;
	FVector2D		Position = FVector2D(Margin, Top) + P * Free;
	// On screen as it is now (a title bar moved lower than the whole panel
	// fits has a placement past 1).
	Position.X = FMath::Clamp(Position.X, static_cast<double>(Margin), FMath::Max(static_cast<double>(Margin), CanvasSize.X - Margin - Size.X));
	Position.Y = FMath::Clamp(Position.Y, static_cast<double>(Top), FMath::Max(static_cast<double>(Top), CanvasSize.Y - Margin - Size.Y));
	if (!CanvasSlot->GetPosition().Equals(Position, 0.25))
	{
		CanvasSlot->SetPosition(Position);
	}
}

void URammsFloatingPanel::TickPanel(float DeltaSeconds)
{
	// New content: tell it it is hosted, and let it fill the body.
	UWidget* Content = GetContent();
	if (Content && Content != HostedContent.Get())
	{
		HostedContent = Content;
		if (IRammsFloatingPanelContent* Hosted = Cast<IRammsFloatingPanelContent>(Content))
		{
			Hosted->SetHostedInFloatingPanel(true);
		}
		if (UOverlaySlot* S = Cast<UOverlaySlot>(Content->Slot))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}
	}
	// A pop-out asked for before there was content to put in the window
	// (restored from last time), or while it was not wanted.
	if (bPopOutPending && Content && bWanted && !bUserHidden && !PoppedWindow.IsValid())
	{
		PopOut();
	}
	// The title bar as wide as the body (kept while minimised).
	if (TitleSize && BodyBox && !bMinimized)
	{
		const float BodyWidth = static_cast<float>(BodyBox->GetDesiredSize().X);
		if (BodyWidth > 1.0f && !FMath::IsNearlyEqual(BodyWidth, TitleWidth, 0.5f))
		{
			TitleWidth = BodyWidth;
			TitleSize->SetWidthOverride(BodyWidth);
		}
	}
	TitleRefreshSeconds -= DeltaSeconds;
	if (TitleRefreshSeconds <= 0.0f)
	{
		TitleRefreshSeconds = 0.5f;
		const FText Shown = GetDisplayTitle();
		if (TitleText && !TitleText->GetText().EqualTo(Shown))
		{
			TitleText->SetText(Shown);
		}
		if (PoppedWindow.IsValid() && !PoppedWindow->GetTitle().EqualTo(Shown))
		{
			PoppedWindow->SetTitle(Shown);
		}
	}
}

void URammsFloatingPanel::NativeConstruct()
{
	Super::NativeConstruct();
	bTearingDown = false;
}

void URammsFloatingPanel::NativeDestruct()
{
	// The HUD is going: its window too, remembered as popped out.
	TearDownWindow();
	Super::NativeDestruct();
}

void URammsFloatingPanel::BeginDestroy()
{
	// Should it ever go without NativeDestruct: its window must not outlive it
	// (the window's border points at this panel's brush).
	TearDownWindow();
	Super::BeginDestroy();
}

void URammsFloatingPanel::TearDownWindow()
{
	if (!PoppedWindow.IsValid())
	{
		return;
	}
	SaveState();
	bTearingDown = true;
	TSharedPtr<SWindow> Window = PoppedWindow;
	PoppedWindow.Reset();
	Window->SetOnWindowClosed(FOnWindowClosed());
	Window->SetRequestDestroyWindowOverride(FRequestDestroyWindowOverride());
	Window->SetContent(SNullWidget::NullWidget);
	DockContent();
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().DestroyWindowImmediately(Window.ToSharedRef());
	}
}

// ---------------------------------------------------------------------------
// Pop-out windows
// ---------------------------------------------------------------------------

bool URammsFloatingPanel::IsPopOutAvailable()
{
#if PLATFORM_DESKTOP
	const URammsControlHUDSettings* Settings = GetDefault<URammsControlHUDSettings>();
	if (Settings && !Settings->bAllowPopOut)
	{
		return false;
	}
	// Pixel Streaming streams the game viewport only: a panel in a window of
	// its own would vanish from the stream.
	if (FCString::Stristr(FCommandLine::Get(), TEXT("PixelStreaming")))
	{
		return false;
	}
	return FSlateApplication::IsInitialized();
#else
	return false;
#endif
}

bool URammsFloatingPanel::IsPoppedOut() const
{
	return PoppedWindow.IsValid();
}

void URammsFloatingPanel::FocusWindow()
{
	if (PoppedWindow.IsValid())
	{
		PoppedWindow->BringToFront(/*bForce=*/true);
	}
}

void URammsFloatingPanel::PopOut()
{
	if (PoppedWindow.IsValid())
	{
		FocusWindow();
		return;
	}
	if (!bAllowPopOut || !IsPopOutAvailable())
	{
		bPopOutPending = false;
		return;
	}
	UWidget* Content = ContentSlot && ContentSlot->GetChildrenCount() > 0 ? ContentSlot->GetChildAt(0) : nullptr;
	if (!Content || !bWanted)
	{
		bPopOutPending = true;
		return;
	}
	bPopOutPending = false;

	// A child of the game's window: it stays above it, and goes with it.
	TSharedPtr<SWindow> Parent;
	if (const UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport())
		{
			Parent = Viewport->GetWindow();
		}
	}
	// The window takes the size it had last time (desktop pixels, as it was
	// measured), else the panel's (Slate units, scaled to pixels here).
	FVector2D Client = WindowSize;
	if (Client.X < 50.0 || Client.Y < 50.0)
	{
		const float		Dpi = Parent.IsValid() ? Parent->GetDPIScaleFactor() : 1.0f;
		const FVector2D Body = BodyBox ? FVector2D(BodyBox->GetCachedGeometry().GetLocalSize()) : FVector2D::ZeroVector;
		Client = FVector2D(FMath::Max(Body.X, 320.0) + 12.0, FMath::Max(Body.Y, 240.0) + 12.0) * Dpi;
	}
	if (!Style)
	{
		WindowBrush = URammsUIStyle::MakeRoundedBoxBrush(FLinearColor(0.06f, 0.06f, 0.08f, 1.0f), 0.0f);
	}

	PoppedContent = Content;
	Content->RemoveFromParent();
	const bool			bPlaced = WindowPosition.X != 0.0 || WindowPosition.Y != 0.0;
	TSharedRef<SWindow> Window = SNew(SWindow)
									 .Title(GetDisplayTitle())
									 .ClientSize(Client)
									 .ScreenPosition(WindowPosition)
									 .AdjustInitialSizeAndPositionForDPIScale(false)
									 .AutoCenter(bPlaced ? EAutoCenter::None : EAutoCenter::PreferredWorkArea)
									 .SizingRule(ESizingRule::UserSized)
									 .SupportsMaximize(true)
									 .SupportsMinimize(true)[MakeWindowContent(Content)];
	// Its own X docks it back; destroyed any other way (with the game's
	// window), it stays popped out for next time.
	Window->SetRequestDestroyWindowOverride(
		FRequestDestroyWindowOverride::CreateUObject(this, &URammsFloatingPanel::HandleWindowCloseRequested));
	Window->SetOnWindowClosed(FOnWindowClosed::CreateUObject(this, &URammsFloatingPanel::HandleWindowClosed));

	PoppedWindow = Parent.IsValid() ? FSlateApplication::Get().AddWindowAsNativeChild(Window, Parent.ToSharedRef())
									: FSlateApplication::Get().AddWindow(Window);
	ApplyVisibility();
	SaveState();
	Changed();
}

void URammsFloatingPanel::DockBack()
{
	bPopOutPending = false;
	if (PoppedWindow.IsValid())
	{
		CloseWindow(/*bKeepPoppedOut=*/false);
		ApplyVisibility();
		SaveState();
		Changed();
	}
}

void URammsFloatingPanel::CloseWindow(bool bKeepPoppedOut)
{
	TSharedPtr<SWindow> Window = PoppedWindow;
	if (!Window.IsValid())
	{
		return;
	}
	WindowPosition = Window->GetPositionInScreen();
	WindowSize = Window->GetClientSizeInScreen();
	// Detach first so neither handler runs as it is destroyed.
	PoppedWindow.Reset();
	Window->SetOnWindowClosed(FOnWindowClosed());
	Window->SetRequestDestroyWindowOverride(FRequestDestroyWindowOverride());
	Window->SetContent(SNullWidget::NullWidget);
	DockContent();
	bPopOutPending = bKeepPoppedOut;
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
	}
}

void URammsFloatingPanel::HandleWindowCloseRequested(const TSharedRef<SWindow>& Window)
{
	if (PoppedWindow == Window)
	{
		DockBack();
	}
	else if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().RequestDestroyWindow(Window);
	}
}

void URammsFloatingPanel::HandleWindowClosed(const TSharedRef<SWindow>& Window)
{
	if (bTearingDown || PoppedWindow != Window)
	{
		return;
	}
	WindowPosition = Window->GetPositionInScreen();
	WindowSize = Window->GetClientSizeInScreen();
	PoppedWindow.Reset();
	Window->SetContent(SNullWidget::NullWidget);
	DockContent();
	bPopOutPending = true;
	SaveState();
	ApplyVisibility();
	Changed();
}

TSharedRef<SWidget> URammsFloatingPanel::MakeWindowContent(UWidget* Content)
{
	return SNew(SBorder).BorderImage(&WindowBrush).Padding(FMargin(6.0f))[Content->TakeWidget()];
}

void URammsFloatingPanel::DockContent()
{
	if (PoppedContent && ContentSlot && ContentSlot->GetChildrenCount() == 0)
	{
		ContentSlot->AddChild(PoppedContent);
		HostedContent = nullptr; // re-applies the fill
	}
	PoppedContent = nullptr;
}

// ---------------------------------------------------------------------------
// Buttons
// ---------------------------------------------------------------------------

void URammsFloatingPanel::HandleMinimizeClicked()
{
	SetMinimized(!bMinimized);
}

void URammsFloatingPanel::HandleCloseClicked()
{
	SetPanelHidden(true);
}

void URammsFloatingPanel::HandlePopOutClicked()
{
	PopOut();
}

// ---------------------------------------------------------------------------
// Moving and resizing
// ---------------------------------------------------------------------------

FReply URammsFloatingPanel::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Any click on it brings it to the front, and goes on to its content.
	BringToFront();
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URammsFloatingPanel::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	const FVector2D	  At = InMouseEvent.GetScreenSpacePosition();
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
	if (bResizable && !bMinimized && Grip && Grip->GetCachedGeometry().IsUnderLocation(At))
	{
		Drag = EDrag::Resize;
		DragStartSize = BodyBox ? FVector2D(BodyBox->GetCachedGeometry().GetLocalSize()) : BodySize;
		DragChrome = FVector2D(GetCachedGeometry().GetLocalSize()) - DragStartSize;
	}
	else if (TitleBar && CanvasSlot && TitleBar->GetCachedGeometry().IsUnderLocation(At))
	{
		Drag = EDrag::Move;
		DragStartPosition = CanvasSlot->GetPosition();
	}
	else
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	DragStartMouse = At;
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply URammsFloatingPanel::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Drag == EDrag::None)
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}
	// Screen pixels to the canvas' Slate units.
	const float		  Scale = InGeometry.Scale > 0.0f ? InGeometry.Scale : 1.0f;
	const FVector2D	  Delta = (InMouseEvent.GetScreenSpacePosition() - DragStartMouse) / Scale;
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
	if (Drag == EDrag::Resize)
	{
		// Kept on screen, like a move: the panel's far corner no further than
		// the viewport's edges, less the margin.
		FVector2D Max(TNumericLimits<double>::Max());
		if (CanvasSlot && LastCanvasSize.X > 0.0 && LastCanvasSize.Y > 0.0)
		{
			Max = LastCanvasSize - FVector2D(LastMargin) - CanvasSlot->GetPosition() - DragChrome;
		}
		BodySize = FVector2D(FMath::Clamp(DragStartSize.X + Delta.X, MinSize.X, FMath::Max(Max.X, MinSize.X)),
			FMath::Clamp(DragStartSize.Y + Delta.Y, MinSize.Y, FMath::Max(Max.Y, MinSize.Y)));
		ApplySize();
		return FReply::Handled();
	}
	if (!CanvasSlot)
	{
		return FReply::Handled();
	}
	// Kept on screen; the placement follows, as a fraction of the room the
	// whole panel leaves (past 1 for a minimised title bar moved lower than
	// the whole panel fits).
	const FVector2D Size = GetDesiredSize();
	const FVector2D Placed = GetPlacementSize();
	const float		M = LastMargin, Top = LastTopInset;
	const FVector2D Free(FMath::Max(0.0, LastCanvasSize.X - Size.X - 2.0 * M), FMath::Max(0.0, LastCanvasSize.Y - Size.Y - Top - M));
	const FVector2D PlacedFree(FMath::Max(1.0, LastCanvasSize.X - Placed.X - 2.0 * M), FMath::Max(1.0, LastCanvasSize.Y - Placed.Y - Top - M));
	FVector2D		Position = DragStartPosition + Delta;
	Position.X = FMath::Clamp(Position.X, static_cast<double>(M), M + Free.X);
	Position.Y = FMath::Clamp(Position.Y, static_cast<double>(Top), Top + Free.Y);
	CanvasSlot->SetPosition(Position);
	Placement = FVector2D(PlacedFree.X > 0.0 ? (Position.X - M) / PlacedFree.X : 0.0, PlacedFree.Y > 0.0 ? (Position.Y - Top) / PlacedFree.Y : 0.0);
	return FReply::Handled();
}

FReply URammsFloatingPanel::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Drag != EDrag::None)
	{
		EndDrag();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void URammsFloatingPanel::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	EndDrag();
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void URammsFloatingPanel::EndDrag()
{
	if (Drag == EDrag::None)
	{
		return;
	}
	bPlaceFromPosition = Drag == EDrag::Resize;
	Drag = EDrag::None;
	SaveState();
}
