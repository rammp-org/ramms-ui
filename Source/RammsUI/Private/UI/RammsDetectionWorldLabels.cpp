#include "UI/RammsDetectionWorldLabels.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "RammsDetectionWorldMarkers.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/RammsUIStyle.h"

namespace RammsDetectionWorldLabels
{
	/** One label worked out but not yet drawn, so the set can be ordered first. */
	struct FPendingLabel
	{
		FString		 Text;
		FVector2D	 PlatePosition = FVector2D::ZeroVector;
		FVector2D	 PlateSize = FVector2D::ZeroVector;
		FVector2D	 TextSize = FVector2D::ZeroVector;
		FLinearColor PlateColour = FLinearColor::White;
		double		 DistanceSquared = 0.0;
	};
} // namespace RammsDetectionWorldLabels

URammsDetectionWorldLabels::URammsDetectionWorldLabels()
{
	// SelfHitTestInvisible: labels are something to read, never something to press, and this layer
	// sits over URammsWorldTapLayer -- taking a press here would stop every world tap.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// A capsule, the shape the 2D POIs use. HalfHeightRadius lets the brush round itself to
	// whatever the text turns out to be, so changing the font size cannot leave the corners wrong.
	PlateBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	PlateBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
}

TSharedRef<SWidget> URammsDetectionWorldLabels::RebuildWidget()
{
	// A root of its own, so this widget has geometry. A UUserWidget with an empty tree paints
	// nothing and is culled before NativePaint is ever called -- it looks exactly like a drawing
	// bug, which is how the first version of this layer appeared to do nothing at all.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("LabelCanvas"));
		if (Root)
		{
			Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			WidgetTree->RootWidget = Root;
		}
	}
	return Super::RebuildWidget();
}

void URammsDetectionWorldLabels::EnsureLabelFont() const
{
	// Keyed on the style as well as the size. URammsBaseWidget resolves Style from the theme in
	// NativeConstruct and SetStyle can replace it later, so a first paint with no style would
	// otherwise pin the engine fallback typeface for good and ignore every theme change after it.
	if (CachedFontSize == LabelFontSize && CachedStyle == Style)
	{
		return;
	}
	CachedStyle = Style;

	// The theme's typeface at THIS widget's size. Taking the style's font whole, as the camera
	// overlay does, would also take its size -- and a size chosen for labels drawn inside a camera
	// image is not one that can be read across the whole view.
	if (Style && Style->Typography.Body.HasValidFont())
	{
		CachedLabelFont = Style->Typography.Body;
	}
	else
	{
		CachedLabelFont = FCoreStyle::GetDefaultFontStyle("Bold", LabelFontSize);
	}
	CachedLabelFont.Size = LabelFontSize;
	CachedFontSize = LabelFontSize;
}

int32 URammsDetectionWorldLabels::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Result = Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	UWorld*			   World = GetWorld();
	APlayerController* Controller = GetOwningPlayer();
	if (!Controller && World)
	{
		// A widget created without an owning player still has a world with one in it.
		Controller = UGameplayStatics::GetPlayerController(World, 0);
	}
	if (!bShowLabels || !World || !Controller || !FSlateApplication::IsInitialized())
	{
		return Result;
	}

	FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();
	if (!Renderer)
	{
		return Result;
	}

	EnsureLabelFont();
	const TSharedRef<FSlateFontMeasure> FontMeasure = Renderer->GetFontMeasureService();
	const FVector2D						LocalSize = AllottedGeometry.GetLocalSize();

	FVector CameraLocation = FVector::ZeroVector;
	if (const APlayerCameraManager* Camera = Controller->PlayerCameraManager)
	{
		CameraLocation = Camera->GetCameraLocation();
	}

	TArray<RammsDetectionWorldLabels::FPendingLabel> Pending;

	// The markers' own registry, not an actor iteration: this runs in NativePaint, which can run
	// more than once a frame, and iterating actors would walk every actor in every loaded level --
	// tens of thousands on a generated street map -- to find a handful of markers.
	for (const TWeakObjectPtr<ARammsDetectionWorldMarker>& Entry : ARammsDetectionWorldMarker::GetLiveMarkers())
	{
		const ARammsDetectionWorldMarker* Marker = Entry.Get();
		// The registry spans worlds: an editor world's markers must not be labelled over PIE's.
		if (!IsValid(Marker) || Marker->GetWorld() != World)
		{
			continue;
		}

		const FVector Anchor = Marker->GetLabelAnchorLocation();
		const double  DistanceSquared = FVector::DistSquared(Anchor, CameraLocation);
		if (MaxLabelDistance > 0.0f && DistanceSquared > static_cast<double>(MaxLabelDistance) * MaxLabelDistance)
		{
			continue;
		}

		// ProjectWorldLocationToWidgetPosition, not ProjectWorldToScreen: it divides by the
		// viewport's DPI scale, so the result is already in the slate units a full-screen widget
		// is laid out in. The raw pixel position is a different space -- here the two differ by
		// 1.9x -- and mixing them puts every label off the side of the screen, where the cull
		// below silently eats it and the layer looks like it is simply not drawing.
		//
		// False when the anchor is behind the camera, which is the case that would otherwise put a
		// label on the opposite side of the screen from the thing it names.
		FVector2D LocalPosition = FVector2D::ZeroVector;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller, Anchor, LocalPosition, false))
		{
			continue;
		}

		FString Text = Marker->GetLabelText();
		if (Text.IsEmpty())
		{
			continue;
		}
		if (bUppercaseLabels)
		{
			Text = Text.ToUpper();
		}
		// The age joins AFTER the upper-casing, or "120s" becomes "120S" and the unit reads as
		// part of the number.
		const FString Age = Marker->GetAgeText();
		if (!Age.IsEmpty())
		{
			Text += TEXT("  ") + Age;
		}

		const FVector2D TextSize = FontMeasure->Measure(Text, CachedLabelFont);
		const FVector2D PlateSize(TextSize.X + LabelPadding.X * 2.0f, TextSize.Y + LabelPadding.Y * 2.0f);

		// Centred on the marker and sitting above it. A 2D POI anchors to the left edge of its
		// box; a 3D box projects to an irregular shape with no left edge, so the centre of the top
		// is the honest equivalent.
		const FVector2D PlatePosition(LocalPosition.X - PlateSize.X * 0.5f, LocalPosition.Y - PlateSize.Y);

		// Cull off-screen labels rather than hand Slate elements it will clip anyway. Only when
		// the size is known: a zero-sized geometry would otherwise cull every label and look like
		// the layer is broken rather than unsized.
		if (LocalSize.X > 0.0f && LocalSize.Y > 0.0f
			&& (PlatePosition.X + PlateSize.X < 0.0f || PlatePosition.X > LocalSize.X
				|| PlatePosition.Y + PlateSize.Y < 0.0f || PlatePosition.Y > LocalSize.Y))
		{
			continue;
		}

		// The pill takes the marker's colour and fades with its age; the ink does not, so an old
		// label recedes without becoming the hardest one to read.
		FLinearColor PlateColour = Marker->GetLabelColour();
		PlateColour.A = FMath::Lerp(1.0f, FMath::Clamp(StaleLabelOpacity, 0.0f, 1.0f), Marker->GetStaleness());

		RammsDetectionWorldLabels::FPendingLabel& Label = Pending.AddDefaulted_GetRef();
		Label.Text = MoveTemp(Text);
		Label.PlatePosition = PlatePosition;
		Label.PlateSize = PlateSize;
		Label.TextSize = TextSize;
		Label.PlateColour = PlateColour;
		Label.DistanceSquared = DistanceSquared;
	}

	// Furthest first, so the nearest label is drawn last and lands on top of the ones behind it.
	// Slate has no depth here -- paint order is the only ordering there is -- and TActorIterator
	// hands actors over in whatever order the level holds them, which is why a far object's name
	// could cover a near one's and look like a bug in the projection rather than in the sequence.
	Pending.Sort([](const RammsDetectionWorldLabels::FPendingLabel&	 A,
					 const RammsDetectionWorldLabels::FPendingLabel& B) {
		return A.DistanceSquared > B.DistanceSquared;
	});

	int32 Layer = LayerId;
	for (const RammsDetectionWorldLabels::FPendingLabel& Label : Pending)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(Label.PlateSize, FSlateLayoutTransform(Label.PlatePosition)),
			&PlateBrush, ESlateDrawEffect::None, Label.PlateColour);

		FSlateDrawElement::MakeText(OutDrawElements, Layer + 1,
			AllottedGeometry.ToPaintGeometry(
				Label.TextSize, FSlateLayoutTransform(Label.PlatePosition + LabelPadding)),
			Label.Text, CachedLabelFont, ESlateDrawEffect::None, LabelTextColour);

		// A pair of layers each, so a nearer pill covers the text of the one behind it rather
		// than interleaving with it.
		Layer += 2;
	}

	return FMath::Max(Result, Layer);
}
