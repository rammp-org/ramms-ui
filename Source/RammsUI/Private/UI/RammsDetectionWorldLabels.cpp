#include "UI/RammsDetectionWorldLabels.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "RammsDetectionWorldMarkers.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/RammsUIStyle.h"

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
	if (CachedFontSize == LabelFontSize)
	{
		return;
	}

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

	int32 Layer = LayerId;

	// Every marker in the world, every frame. It is a class-filtered actor iteration, which costs
	// a pass over the level's actor list -- tens of microseconds on the maps this runs on. If a
	// level ever makes that matter, the fix is a registry the markers add themselves to, not a
	// cached list that goes stale the first time one is pooled away.
	for (TActorIterator<ARammsDetectionWorldMarker> It(World); It; ++It)
	{
		const ARammsDetectionWorldMarker* Marker = *It;
		if (!IsValid(Marker))
		{
			continue;
		}

		const FVector Anchor = Marker->GetLabelAnchorLocation();
		if (MaxLabelDistance > 0.0f && FVector::Dist(Anchor, CameraLocation) > MaxLabelDistance)
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

		// Centred on the marker and sitting above it. The camera overlay anchors its plate to the
		// left edge of a 2D box; a 3D box projects to an irregular shape with no left edge, so the
		// centre of the top is the honest equivalent.
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

		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(PlateSize, FSlateLayoutTransform(PlatePosition)), &PlateBrush,
			ESlateDrawEffect::None, PlateColour);

		FSlateDrawElement::MakeText(OutDrawElements, Layer + 1,
			AllottedGeometry.ToPaintGeometry(
				TextSize, FSlateLayoutTransform(PlatePosition + LabelPadding)),
			Text, CachedLabelFont, ESlateDrawEffect::None, LabelTextColour);

		// Each label gets its own pair of layers, so a nearer plate covers a further label's text
		// rather than interleaving with it.
		Layer += 2;
	}

	return FMath::Max(Result, Layer);
}
