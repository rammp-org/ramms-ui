// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsSurfacePositionPad.h"
#include "RammsControlSink.h"
#include "RammsControlSurfaceRegistry.h"
#include "Styling/CoreStyle.h"

namespace
{
	/** Slate's plain filled box, for the dots. */
	const FSlateBrush* WhiteBox()
	{
		static const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("GenericWhiteBox");
		return Brush;
	}

	/** A span's width, guarded: a zero-width range would map every value onto one
	 *  pixel and divide by nothing doing it. */
	double SpanOf(const FVector2D& Range)
	{
		const double Width = Range.Y - Range.X;
		return FMath::Abs(Width) > UE_DOUBLE_SMALL_NUMBER ? Width : 1.0;
	}
} // namespace

URammsSurfacePositionPad::URammsSurfacePositionPad(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Visible, not the UUserWidget default of SelfHitTestInvisible.
	//
	// SelfHitTestInvisible means "hit-test my children, not me", and this pad
	// has no children: everything it shows is drawn in NativePaint. So it
	// painted perfectly and accepted no pointer at all -- the region, the live
	// dot and the reset button all worked, and the pad itself could not be
	// clicked or dragged.
	SetVisibility(ESlateVisibility::Visible);
}

void URammsSurfacePositionPad::NativeConstruct()
{
	Super::NativeConstruct();
	// Again here: a Blueprint subclass or an archetype can carry its own
	// serialized Visibility that overrides what the constructor set.
	SetVisibility(ESlateVisibility::Visible);
}

void URammsSurfacePositionPad::SetTarget(UObject* Sink, FName IdX, FName IdY)
{
	TargetSink = Sink;
	ControlIdX = IdX;
	ControlIdY = IdY;
}

void URammsSurfacePositionPad::SetRegion(FVector2D InRangeX, FVector2D InRangeY, const TArray<FVector2D>& InRegion)
{
	RangeX = InRangeX;
	RangeY = InRangeY;
	Region = InRegion;
}

UObject* URammsSurfacePositionPad::ResolveSink() const
{
	if (TargetSink && TargetSink->GetClass()->ImplementsInterface(URammsControlSink::StaticClass()))
	{
		return TargetSink;
	}
	if (!bAutoFindSink)
	{
		return nullptr;
	}
	if (UWorld* World = GetWorld())
	{
		if (URammsControlSurfaceRegistry* Reg = World->GetSubsystem<URammsControlSurfaceRegistry>())
		{
			UObject* Found = Reg->FindControlSurface();
			if (Found && Found->GetClass()->ImplementsInterface(URammsControlSink::StaticClass()))
			{
				return Found;
			}
		}
	}
	return nullptr;
}

FVector2D URammsSurfacePositionPad::ValueToLocal(FVector2D Value, FVector2D WidgetSize) const
{
	// Inset so a point on the boundary is drawn inside the widget rather than
	// half-clipped by its edge.
	const double W = FMath::Max(1.0, WidgetSize.X - 2.0 * EdgeInset);
	const double H = FMath::Max(1.0, WidgetSize.Y - 2.0 * EdgeInset);

	const double Tx = (Value.X - RangeX.X) / SpanOf(RangeX);
	const double Ty = (Value.Y - RangeY.X) / SpanOf(RangeY);

	// Widget space runs down; height runs up. The flip is here and nowhere
	// else, so everything downstream can think in widget coordinates.
	return FVector2D(EdgeInset + Tx * W, EdgeInset + (1.0 - Ty) * H);
}

FVector2D URammsSurfacePositionPad::LocalToValue(FVector2D Local, FVector2D WidgetSize) const
{
	const double W = FMath::Max(1.0, WidgetSize.X - 2.0 * EdgeInset);
	const double H = FMath::Max(1.0, WidgetSize.Y - 2.0 * EdgeInset);

	const double Tx = (Local.X - EdgeInset) / W;
	const double Ty = 1.0 - (Local.Y - EdgeInset) / H;

	return FVector2D(RangeX.X + Tx * SpanOf(RangeX), RangeY.X + Ty * SpanOf(RangeY));
}

TArray<FVector2D> URammsSurfacePositionPad::ShapeToDraw() const
{
	if (Region.Num() >= 3)
	{
		return Region;
	}

	// No region published: the pair really is the box its ranges describe, and
	// drawing that is honest. Drawing nothing would suggest there is nowhere to
	// go.
	TArray<FVector2D> Box;
	Box.Emplace(RangeX.X, RangeY.X);
	Box.Emplace(RangeX.Y, RangeY.X);
	Box.Emplace(RangeX.Y, RangeY.Y);
	Box.Emplace(RangeX.X, RangeY.Y);
	return Box;
}

FVector2D URammsSurfacePositionPad::GetLiveValue() const
{
	UObject* Sink = ResolveSink();
	if (!Sink)
	{
		return FVector2D::ZeroVector;
	}
	return FVector2D(
		ControlIdX.IsNone() ? 0.0f : IRammsControlSink::Execute_GetAxisValue(Sink, ControlIdX),
		ControlIdY.IsNone() ? 0.0f : IRammsControlSink::Execute_GetAxisValue(Sink, ControlIdY));
}

bool URammsSurfacePositionPad::GetTargetValue(FVector2D& OutValue) const
{
	UObject* Sink = ResolveSink();
	if (!Sink || ControlIdX.IsNone() || ControlIdY.IsNone())
	{
		return false;
	}
	float X = 0.0f;
	float Y = 0.0f;
	// Both halves or neither: half a target is not a place, and drawing one
	// would put the ring somewhere the mechanism was never asked to go.
	if (!IRammsControlSink::Execute_GetAxisTarget(Sink, ControlIdX, X)
		|| !IRammsControlSink::Execute_GetAxisTarget(Sink, ControlIdY, Y))
	{
		return false;
	}
	OutValue = FVector2D(X, Y);
	return true;
}

bool URammsSurfacePositionPad::CommandValue(FVector2D Value)
{
	UObject* Sink = ResolveSink();
	if (!Sink)
	{
		return false;
	}

	// Both axes, every time. A 5-bar reaches a point with both motors, so
	// commanding one coordinate and leaving the other to whatever it was is how
	// a pad ends up describing a pose nobody pointed at.
	bool bAny = false;
	if (!ControlIdY.IsNone())
	{
		bAny |= IRammsControlSink::Execute_SetAxis(Sink, ControlIdY, static_cast<float>(Value.Y), Source);
	}
	if (!ControlIdX.IsNone())
	{
		bAny |= IRammsControlSink::Execute_SetAxis(Sink, ControlIdX, static_cast<float>(Value.X), Source);
	}
	return bAny;
}

bool URammsSurfacePositionPad::CommandAt(const FGeometry& Geometry, const FVector2D& ScreenPosition)
{
	const FVector2D Local = Geometry.AbsoluteToLocal(ScreenPosition);
	return CommandValue(LocalToValue(Local, Geometry.GetLocalSize()));
}

int32 URammsSurfacePositionPad::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X <= 0.0 || Size.Y <= 0.0)
	{
		return LayerId;
	}

	const int32			 Layer = LayerId + 1;
	const FPaintGeometry Paint = AllottedGeometry.ToPaintGeometry();

	// The region, closed by repeating the first point rather than by asking the
	// renderer to close it -- MakeLines draws a polyline and nothing else.
	const TArray<FVector2D> Shape = ShapeToDraw();
	if (Shape.Num() >= 2)
	{
		TArray<FVector2D> Points;
		Points.Reserve(Shape.Num() + 1);
		for (const FVector2D& Value : Shape)
		{
			Points.Add(ValueToLocal(Value, Size));
		}
		// Copied out first. Adding an element that lives inside the array being
		// added to is a checked error in UE -- the growth would invalidate the
		// reference mid-call -- and it fires on the very first paint of a real
		// region, taking the editor with it.
		const FVector2D First = Points[0];
		Points.Add(First);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Paint, Points, ESlateDrawEffect::None, RegionColor,
			/*bAntialias=*/true, 1.0f);
	}

	const auto Dot = [&](FVector2D Value, const FLinearColor& Color, float Radius) {
		const FVector2D At = ValueToLocal(Value, Size);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(Radius * 2.0f, Radius * 2.0f),
				FSlateLayoutTransform(At - FVector2D(Radius, Radius))),
			WhiteBox(), ESlateDrawEffect::None, Color);
	};

	// The target is a ring, not a second filled marker. Drawn as a box it sat
	// behind the live dot as a larger square, so "commanded" and "arrived" read
	// as one blob rather than as a dot inside a ring closing on it.
	const auto Ring = [&](FVector2D Value, const FLinearColor& Color, float Radius) {
		constexpr int32	  Segments = 16;
		const FVector2D	  At = ValueToLocal(Value, Size);
		TArray<FVector2D> Points;
		Points.Reserve(Segments + 1);
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float Angle = 2.0f * PI * static_cast<float>(i) / static_cast<float>(Segments);
			Points.Emplace(At.X + Radius * FMath::Cos(Angle), At.Y + Radius * FMath::Sin(Angle));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, Layer + 1, Paint, Points, ESlateDrawEffect::None, Color,
			/*bAntialias=*/true, 1.5f);
	};

	// The ring first, so the live dot draws inside it rather than under it.
	FVector2D Target;
	if (GetTargetValue(Target))
	{
		Ring(Target, TargetColor, DotRadius * 1.8f);
	}
	Dot(GetLiveValue(), LiveColor, DotRadius);

	return Layer + 2;
}

FReply URammsSurfacePositionPad::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Left only. A right- or middle-click would otherwise command a position
	// and take the mouse capture with it, which is how a context menu turns
	// into a move order.
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	bDragging = true;
	CommandAt(InGeometry, InMouseEvent.GetScreenSpacePosition());
	// Captured, or a drag that leaves the widget would silently stop steering
	// while the button is still down.
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply URammsSurfacePositionPad::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging)
	{
		return FReply::Unhandled();
	}
	CommandAt(InGeometry, InMouseEvent.GetScreenSpacePosition());
	return FReply::Handled();
}

FReply URammsSurfacePositionPad::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging)
	{
		return FReply::Unhandled();
	}
	bDragging = false;

	// Nothing is released. These are Position controls: letting go of the pad
	// means "stop steering it", not "stop holding the pose" -- a lift that
	// dropped when you lifted your finger would be a very different machine.
	return FReply::Handled().ReleaseMouseCapture();
}

FReply URammsSurfacePositionPad::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InEvent)
{
	bDragging = true;
	CommandAt(InGeometry, InEvent.GetScreenSpacePosition());
	// Captured, like the mouse path: without it a finger that slides off the
	// pad stops delivering moves here, so the drag dies mid-gesture and the
	// end event cannot reliably clear the interaction either.
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply URammsSurfacePositionPad::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InEvent)
{
	if (!bDragging)
	{
		return FReply::Unhandled();
	}
	CommandAt(InGeometry, InEvent.GetScreenSpacePosition());
	return FReply::Handled();
}

FReply URammsSurfacePositionPad::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InEvent)
{
	bDragging = false;
	return FReply::Handled().ReleaseMouseCapture();
}
