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

	// Volatile, because everything this draws is read from the sink inside
	// NativePaint rather than pushed in. Under a Slate invalidation root a
	// non-volatile widget can have its first paint cached indefinitely, and
	// the live dot would then sit still while the mechanism moved and the ring
	// would never clear on arrival.
	ForceVolatile(true);
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

	// Retargeting drops whatever was discovered before. Passing null is the
	// documented way back to auto-discovery, and a cache left standing would
	// answer with the previous surface instead -- so a pad pointed at a new
	// robot would go on commanding the old one, which is the worst version of
	// this to debug.
	CachedSink.Reset();
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
	if (UObject* Cached = CachedSink.Get())
	{
		return Cached;
	}
	if (UWorld* World = GetWorld())
	{
		if (URammsControlSurfaceRegistry* Reg = World->GetSubsystem<URammsControlSurfaceRegistry>())
		{
			UObject* Found = Reg->FindControlSurface();
			if (Found && Found->GetClass()->ImplementsInterface(URammsControlSink::StaticClass()))
			{
				CachedSink = Found;
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
	//
	// And then the refused half again, because the sink takes ONE axis at a
	// time. A click is therefore two 1-D commands with an axis-aligned pose in
	// between -- (old across, new along) or the reverse -- and a curved
	// reachable region can exclude that intermediate even when the point
	// clicked sits well inside it. The controller refuses that half, the other
	// half lands, and the endpoint arrives at the new fore/aft with its old
	// height. A second click then works, because by then the first coordinate
	// has already moved: exactly the "click twice" behaviour this collapses.
	//
	// Two refusals are NOT proof that the point is unreachable, and this must
	// not be read as if they were. The region can be non-convex -- the
	// lift-drive's measurably is -- and then both axis-aligned intermediates,
	// (old across, new along) and its mirror, can lie outside it while the
	// point itself lies inside. Two 1-D commands cannot reach such a point at
	// all, whatever order they are tried in.
	//
	// Measured on the lift-drive, across 3192 ordered pairs of points sampled
	// inside its published region, that case arose zero times: the reflex
	// vertices are local to the boundary rather than a notch separating one
	// part of the region from another. So the retry is sufficient there, and
	// the limitation is real rather than hypothetical on some other mechanism.
	//
	// The fix is an atomic pair command -- a 2-D control kind -- since every
	// sink signature today is (FName, float). Until that exists this reports
	// failure for a point it could not command, which is at least honest.
	const bool bHasY = !ControlIdY.IsNone();
	const bool bHasX = !ControlIdX.IsNone();

	bool bY = bHasY && IRammsControlSink::Execute_SetAxis(Sink, ControlIdY, static_cast<float>(Value.Y), Source);
	bool bX = bHasX && IRammsControlSink::Execute_SetAxis(Sink, ControlIdX, static_cast<float>(Value.X), Source);

	if (bHasY && !bY && bX)
	{
		bY = IRammsControlSink::Execute_SetAxis(Sink, ControlIdY, static_cast<float>(Value.Y), Source);
	}
	if (bHasX && !bX && bY)
	{
		bX = IRammsControlSink::Execute_SetAxis(Sink, ControlIdX, static_cast<float>(Value.X), Source);
	}

	// A pair is commanded or it is not. Reporting success because one half
	// landed would call a half-applied pose a success -- and a half-applied
	// pose is exactly the bug this retry exists to remove. A pad configured
	// with only one id is still answered on that one.
	return (bHasX && bHasY) ? (bX && bY) : (bX || bY);
}

bool URammsSurfacePositionPad::CommandAt(const FGeometry& Geometry, const FVector2D& ScreenPosition)
{
	const FVector2D Local = Geometry.AbsoluteToLocal(ScreenPosition);
	FVector2D		Value = LocalToValue(Local, Geometry.GetLocalSize());

	// Clamped, because the pointer is CAPTURED: a drag continues to be
	// delivered here after it leaves the widget, so the mapping runs on
	// coordinates outside the pad and extrapolates past both ranges. The sink
	// does not clamp to the advertised range, so without this a drag off the
	// edge sends targets the axis never offered.
	Value.X = FMath::Clamp(Value.X, FMath::Min(RangeX.X, RangeX.Y), FMath::Max(RangeX.X, RangeX.Y));
	Value.Y = FMath::Clamp(Value.Y, FMath::Min(RangeY.X, RangeY.Y), FMath::Max(RangeY.X, RangeY.Y));

	// And onto the region, because the ranges are a bounding box and the
	// mechanism is not one. Every point in the corners of that box is drawn as
	// unavailable and was still commandable, which sends coordinates the
	// mechanism refuses -- and a refused pair can leave one half applied, since
	// the two axes go through the sink one at a time.
	//
	// Projected rather than rejected: a drag that wanders over the edge should
	// track along the boundary, which is what the mechanism can actually do,
	// instead of freezing or silently doing nothing.
	Value = ProjectIntoRegion(Value);
	return CommandValue(Value);
}

bool URammsSurfacePositionPad::IsInsideRegion(FVector2D Value) const
{
	if (Region.Num() < 3)
	{
		// Nothing published: the pair really is its rectangle, and the caller
		// has already clamped to that.
		return true;
	}
	// Crossing count. The outline is closed implicitly, so the last point pairs
	// with the first.
	bool bInside = false;
	for (int32 i = 0, j = Region.Num() - 1; i < Region.Num(); j = i++)
	{
		const FVector2D& A = Region[i];
		const FVector2D& B = Region[j];
		if (((A.Y > Value.Y) != (B.Y > Value.Y))
			&& (Value.X < (B.X - A.X) * (Value.Y - A.Y) / (B.Y - A.Y != 0.0 ? B.Y - A.Y : UE_DOUBLE_SMALL_NUMBER) + A.X))
		{
			bInside = !bInside;
		}
	}
	return bInside;
}

FVector2D URammsSurfacePositionPad::ProjectIntoRegion(FVector2D Value) const
{
	if (Region.Num() < 3)
	{
		// No region published, so the pair is the rectangle its ranges describe
		// -- which is what this is documented to fall back to. Returning the
		// value untouched let a direct caller of this API hold a point outside
		// even that. The pointer path clamps before it gets here; a script
		// calling the exposed function does not.
		return FVector2D(
			FMath::Clamp(Value.X, FMath::Min(RangeX.X, RangeX.Y), FMath::Max(RangeX.X, RangeX.Y)),
			FMath::Clamp(Value.Y, FMath::Min(RangeY.X, RangeY.Y), FMath::Max(RangeY.X, RangeY.Y)));
	}
	if (IsInsideRegion(Value))
	{
		return Value;
	}

	double ZLo = TNumericLimits<double>::Max();
	double ZHi = -TNumericLimits<double>::Max();
	for (const FVector2D& P : Region)
	{
		ZLo = FMath::Min(ZLo, P.Y);
		ZHi = FMath::Max(ZHi, P.Y);
	}
	// Just inside the extremes. The crossing rule below is half-open in Y so
	// that a vertex shared by two edges counts once; at exactly the lowest or
	// highest point of the polygon that rule yields nothing, and the clamp
	// would then have no interval to work with.
	const double Inward = FMath::Max((ZHi - ZLo) * 1e-6, UE_DOUBLE_SMALL_NUMBER);
	const double Z = FMath::Clamp(Value.Y, ZLo + Inward, ZHi - Inward);

	// Every crossing of the horizontal line at this height, in order. Sorted
	// and paired, they are the intervals reachable at this height -- plural,
	// because the region is not convex, and taking the outermost two would
	// span any notch between them and hand back a point the mechanism cannot
	// reach at all.
	TArray<double> Crossings;
	for (int32 i = 0, j = Region.Num() - 1; i < Region.Num(); j = i++)
	{
		const FVector2D& A = Region[j];
		const FVector2D& B = Region[i];
		const double	 Span = B.Y - A.Y;
		if (FMath::Abs(Span) <= UE_DOUBLE_SMALL_NUMBER)
		{
			// Horizontal edges contribute nothing, deliberately. Adding their
			// two ends looked right and double-counted them: the edges rising
			// from each end already contribute those same points under the rule
			// below, so a horizontal bottom edge produced [x0, x0, x1, x1],
			// which pairs into two zero-width intervals and clamps a centred
			// cursor onto a corner instead of the edge it is pointing at.
			continue;
		}
		// Half-open in Y so a vertex shared by two edges is counted once, which
		// is what keeps the crossings pairing up into intervals.
		const bool bStraddles = (A.Y <= Z && B.Y > Z) || (B.Y <= Z && A.Y > Z);
		if (bStraddles)
		{
			Crossings.Add(A.X + (B.X - A.X) * ((Z - A.Y) / Span));
		}
	}

	if (Crossings.Num() < 2)
	{
		// No interval at this height; leave the value rather than invent a pose.
		return Value;
	}
	Crossings.Sort();

	// The interval the cursor is in, or failing that the one it is nearest to.
	double Best = 0.0;
	double BestDist = TNumericLimits<double>::Max();
	bool   bFound = false;
	for (int32 i = 0; i + 1 < Crossings.Num(); i += 2)
	{
		const double Lo = Crossings[i];
		const double Hi = Crossings[i + 1];
		const double Inset = FMath::Min(static_cast<double>(RegionInset), 0.5 * (Hi - Lo));
		const double Clamped = FMath::Clamp(Value.X, Lo + Inset, Hi - Inset);
		const double Dist = FMath::Abs(Clamped - Value.X);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Clamped;
			bFound = true;
		}
	}
	return bFound ? FVector2D(Best, Z) : Value;
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

	// The ring first, so the live dot draws inside it rather than under it --
	// and only while the two actually differ. A ring drawn around a dot that
	// has arrived says "still moving" about a mechanism that has stopped, and
	// the gap between the two is the only thing the ring is there to show.
	const FVector2D Live = GetLiveValue();
	FVector2D		Target;
	if (GetTargetValue(Target) && !Target.Equals(Live, ArrivedTolerance))
	{
		Ring(Target, TargetColor, DotRadius * 1.8f);
	}
	Dot(Live, LiveColor, DotRadius);

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
