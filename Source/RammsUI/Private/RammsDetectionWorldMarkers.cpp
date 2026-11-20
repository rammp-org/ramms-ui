// RammsDetectionWorldMarkers.cpp

#include "RammsDetectionWorldMarkers.h"

#include "Components/BoxComponent.h"
#include "Dom/JsonObject.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IRammsCameraProvider.h"
#include "RammsBridgeSubsystem.h"
#include "RammsCameraStreamSettings.h"
#include "RammsStreamCameraBridge.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogRammsDetectionMarkers, Log, All);

namespace RammsDetectionMarkers
{
	/** Producers send metres; Unreal works in centimetres. */
	constexpr double MetresToUU = 100.0;

	/** Read {"X":..,"Y":..,"Z":..}. Bare arrays are not accepted, matching the box/centroid keys. */
	static bool ReadPoint(const FJsonObject& Object, const TCHAR* Field, FVector& Out)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(Field);
		if (!Value.IsValid())
		{
			return false;
		}
		const TSharedPtr<FJsonObject>* AsObject = nullptr;
		if (!Value->TryGetObject(AsObject) || !AsObject->IsValid())
		{
			return false;
		}
		double X = 0.0, Y = 0.0, Z = 0.0;
		if (!(*AsObject)->TryGetNumberField(TEXT("X"), X) || !(*AsObject)->TryGetNumberField(TEXT("Y"), Y)
			|| !(*AsObject)->TryGetNumberField(TEXT("Z"), Z))
		{
			return false;
		}
		Out = FVector(X, Y, Z);
		return true;
	}
	/** One depth sample, in Unreal units, or a negative number when the pixel carries no range. */
	static float DepthSampleToUU(const uint8* Data, int32 Index, ERammsDepthFormat Format)
	{
		switch (Format)
		{
			case ERammsDepthFormat::Float32CM:
			{
				// Already centimetres, which is what Unreal works in: no conversion, and none wanted.
				const float Value = reinterpret_cast<const float*>(Data)[Index];
				return (FMath::IsFinite(Value) && Value > 0.0f) ? Value : -1.0f;
			}
			case ERammsDepthFormat::Uint16MM:
			{
				const uint16 Value = reinterpret_cast<const uint16*>(Data)[Index];
				// Zero is the usual "no return" in a uint16 depth map, not a surface at the lens.
				return Value > 0 ? static_cast<float>(Value) * 0.1f : -1.0f;
			}
			default:
				return -1.0f;
		}
	}

	static int32 BytesPerDepthPixel(ERammsDepthFormat Format)
	{
		return Format == ERammsDepthFormat::Float32CM ? 4 : (Format == ERammsDepthFormat::Uint16MM ? 2 : 0);
	}

	/**
	 * Median of the valid samples in a square window around a normalized point.
	 *
	 * The median rather than the mean, and valid samples only: a centroid lands near an object's
	 * edge often enough, where a few pixels read the wall behind it, and a mean would place the
	 * marker in between -- inside neither. The median picks the surface that most of the window
	 * agrees on.
	 */
	static bool SampleDepthUU(const TArray<uint8>& Data, ERammsDepthFormat Format, int32 Width, int32 Height,
		const FVector2D& NormalizedPoint, int32 Radius, float& OutUU)
	{
		const int32 Stride = BytesPerDepthPixel(Format);
		if (Stride == 0 || Width <= 0 || Height <= 0)
		{
			return false;
		}
		// EXACTLY the registered size, not merely big enough. Indexing uses the registered Width
		// as the row stride, so a frame that is larger than its registration -- a stream declared
		// 320x240 that later sends 640x480, which nothing re-registers -- would be sampled from
		// the wrong rows and place markers at plausible but wrong positions, reporting nothing.
		if (Data.Num() != Width * Height * Stride)
		{
			return false;
		}

		const int32 CentreX = FMath::Clamp(FMath::FloorToInt32(NormalizedPoint.X * Width), 0, Width - 1);
		const int32 CentreY = FMath::Clamp(FMath::FloorToInt32(NormalizedPoint.Y * Height), 0, Height - 1);
		const int32 R = FMath::Clamp(Radius, 0, 8);

		TArray<float, TInlineAllocator<289>> Samples;
		for (int32 Y = FMath::Max(0, CentreY - R); Y <= FMath::Min(Height - 1, CentreY + R); ++Y)
		{
			for (int32 X = FMath::Max(0, CentreX - R); X <= FMath::Min(Width - 1, CentreX + R); ++X)
			{
				const float Sample = DepthSampleToUU(Data.GetData(), Y * Width + X, Format);
				if (Sample > 0.0f)
				{
					Samples.Add(Sample);
				}
			}
		}
		if (Samples.IsEmpty())
		{
			return false;
		}
		Samples.Sort();
		OutUU = Samples[Samples.Num() / 2];
		return true;
	}
	/** Read {"X":..,"Y":..,"Z":..,"W":..} as a quaternion. */
	static bool ReadQuat(const FJsonObject& Object, const TCHAR* Field, FQuat& Out)
	{
		const TSharedPtr<FJsonValue>   Value = Object.TryGetField(Field);
		const TSharedPtr<FJsonObject>* AsObject = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(AsObject) || !AsObject->IsValid())
		{
			return false;
		}
		double X = 0.0, Y = 0.0, Z = 0.0, W = 0.0;
		if (!(*AsObject)->TryGetNumberField(TEXT("X"), X) || !(*AsObject)->TryGetNumberField(TEXT("Y"), Y)
			|| !(*AsObject)->TryGetNumberField(TEXT("Z"), Z) || !(*AsObject)->TryGetNumberField(TEXT("W"), W))
		{
			return false;
		}
		const FQuat Q(X, Y, Z, W);
		if (Q.SizeSquared() < UE_KINDA_SMALL_NUMBER)
		{
			return false;
		}
		Out = Q.GetNormalized();
		return true;
	}

	/**
	 * The basis that takes a point in the camera's optical frame (X right, Y down, Z forward) to
	 * Unreal-local, matching CameraPointToWorld and so RammsProjection.ush:
	 *   UE_X = Cam_Z,  UE_Y = Cam_X,  UE_Z = -Cam_Y
	 * Its determinant is -1: the optical frame is right-handed and Unreal is not, and that flip is
	 * exactly what has to be carried through a rotation as well as a point.
	 */
	static FMatrix CameraToUnrealBasis()
	{
		return FMatrix(
			FPlane(0, 1, 0, 0),	 // Cam_X -> UE_Y
			FPlane(0, 0, -1, 0), // Cam_Y -> -UE_Z
			FPlane(1, 0, 0, 0),	 // Cam_Z -> UE_X
			FPlane(0, 0, 0, 1));
	}

	/** Right-handed Z-up (X forward, Y left) to Unreal: negate Y. Determinant -1, as above. */
	static FMatrix ProducerWorldToUnrealBasis()
	{
		return FMatrix(
			FPlane(1, 0, 0, 0),
			FPlane(0, -1, 0, 0),
			FPlane(0, 0, 1, 0),
			FPlane(0, 0, 0, 1));
	}

	/**
	 * The object's normalized image point: its centroid, or the centre of its box when it sent only
	 * that. Pixel coordinates are not accepted here -- the bridge normalizes before this component
	 * sees them, and guessing from magnitude is how the overlay once placed pixels as fractions.
	 */
	/**
	 * Read Count numbers from Field: an array, or an object with Keys ({"X": .., "Y": ..}).
	 *
	 * Both, as URammsCameraWidget reads them. The bridge sends the object form -- UE's JSON struct
	 * converter takes no bare arrays into FVector2D/FVector4 -- so reading arrays alone placed no
	 * derived marker for any object the real bridge sent.
	 */
	static bool ReadNumbers(const FJsonObject& Object, const TCHAR* Field, const TCHAR* const* Keys, int32 Count, double* Out)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(Field);
		if (!Value.IsValid())
		{
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		const TSharedPtr<FJsonObject>*		  Fields = nullptr;
		if (Value->TryGetArray(Array))
		{
			if (Array->Num() < Count)
			{
				return false;
			}
			for (int32 i = 0; i < Count; ++i)
			{
				if (!(*Array)[i].IsValid() || !(*Array)[i]->TryGetNumber(Out[i]))
				{
					return false;
				}
			}
			return true;
		}
		if (Value->TryGetObject(Fields) && Fields && Fields->IsValid())
		{
			for (int32 i = 0; i < Count; ++i)
			{
				if (!(*Fields)->TryGetNumberField(Keys[i], Out[i]))
				{
					return false;
				}
			}
			return true;
		}
		return false;
	}

	/**
	 * The object's point in its image -- the centroid, else the centre of the box -- and whether it is
	 * in pixels of that image rather than normalized.
	 *
	 * The same reading URammsCameraWidget gives the same payload, so a marker in the world and the
	 * box on the camera agree on what was sent:
	 *  - an all-zero box (x_min == x_max, y_min == y_max) is no box, and a centroid of exactly (0, 0)
	 *    with no box is no point -- the orchestrator sends those for an object it has no box for,
	 *    not for an object in the corner;
	 *  - normalized when every coordinate sent is within 0-1 (with slack for rounding), otherwise
	 *    pixels: the bridge's own units (GetDetectionCoordinateScale).
	 */
	static bool ReadImagePoint(const FJsonObject& Object, FVector2D& OutPoint, bool& bOutPixels)
	{
		static const TCHAR* const Keys[] = { TEXT("X"), TEXT("Y"), TEXT("Z"), TEXT("W") };
		double					  Box[4] = { 0.0, 0.0, 0.0, 0.0 };
		double					  Centroid[2] = { 0.0, 0.0 };
		const bool				  bHasBox = ReadNumbers(Object, TEXT("box"), Keys, 4, Box) && (Box[0] != Box[2] || Box[1] != Box[3]);
		const bool				  bHasCentroid = ReadNumbers(Object, TEXT("centroid"), Keys, 2, Centroid);
		if (!bHasBox && (!bHasCentroid || (Centroid[0] == 0.0 && Centroid[1] == 0.0)))
		{
			return false;
		}
		OutPoint = bHasCentroid ? FVector2D(Centroid[0], Centroid[1])
								: FVector2D((Box[0] + Box[2]) * 0.5, (Box[1] + Box[3]) * 0.5);
		const auto IsUnit = [](double Value) { return Value >= -0.001 && Value <= 1.001; };
		bool	   bUnit = !bHasCentroid || (IsUnit(Centroid[0]) && IsUnit(Centroid[1]));
		if (bHasBox)
		{
			bUnit = bUnit && IsUnit(Box[0]) && IsUnit(Box[1]) && IsUnit(Box[2]) && IsUnit(Box[3]);
		}
		bOutPixels = !bUnit;
		return true;
	}
} // namespace RammsDetectionMarkers

ARammsDetectionWorldMarker::ARammsDetectionWorldMarker()
{
	// Ticks to draw its own outline; it has no mesh, so nothing else would show it.
	PrimaryActorTick.bCanEverTick = true;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	SetRootComponent(Bounds);

	// Query only, and see TapChannel for why the response is Overlap rather than Block: a marker
	// is a target to aim at, not an obstacle, and on this project's channels "obstacle" would mean
	// an invisible one in front of the sonar. Overlap events stay off -- the layer queries the box
	// when it needs it; nothing wants a begin/end overlap every time the robot drives past.
	Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Bounds->SetGenerateOverlapEvents(false);
	Bounds->SetBoxExtent(DefaultExtent);
	ApplyCollisionResponse();

	Bounds->OnClicked.AddDynamic(this, &ARammsDetectionWorldMarker::HandleClicked);
	Bounds->OnInputTouchBegin.AddDynamic(this, &ARammsDetectionWorldMarker::HandleTouchBegin);
}

void ARammsDetectionWorldMarker::ApplyCollisionResponse()
{
	if (!Bounds)
	{
		return;
	}
	Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	Bounds->SetCollisionResponseToChannel(TapChannel, bBlockTapChannel ? ECR_Block : ECR_Overlap);
}

TArray<TWeakObjectPtr<ARammsDetectionWorldMarker>> ARammsDetectionWorldMarker::LiveMarkers;

void ARammsDetectionWorldMarker::BeginPlay()
{
	Super::BeginPlay();

	LiveMarkers.AddUnique(this);

	// A Blueprint subclass's TapChannel is not applied until after the constructor has run.
	ApplyCollisionResponse();

#if !ENABLE_DRAW_DEBUG
	// Everything this marker shows is a debug draw, and those compile out of Shipping and Test.
	// Once per run, not once per marker: markers are spawned as objects come and go, so a
	// per-instance log would be the same complaint for the life of the build.
	static bool bWarnedNoDebugDraw = false;
	if (!bWarnedNoDebugDraw)
	{
		bWarnedNoDebugDraw = true;
		UE_LOG(LogRammsDetectionMarkers, Warning,
			TEXT("Debug drawing is compiled out of this build, so world markers are invisible. "
				 "Give MarkerClass a Blueprint subclass with a mesh, or ship Development."));
	}
#endif
}

void ARammsDetectionWorldMarker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	LiveMarkers.RemoveAllSwap([this](const TWeakObjectPtr<ARammsDetectionWorldMarker>& Entry) {
		return !Entry.IsValid() || Entry.Get() == this;
	});
	Super::EndPlay(EndPlayReason);
}

void ARammsDetectionWorldMarker::SetDetection(const FRammsDetectionWorldPoint& Point)
{
	Detection = Point;
	if (const UWorld* World = GetWorld())
	{
		DetectionReceivedTime = World->GetTimeSeconds();
	}
	SetActorLocationAndRotation(Point.WorldLocation, Point.WorldRotation.Quaternion());
	if (Bounds)
	{
		// Half of the full size the producer sent, or a default worth aiming at -- but never
		// thinner than that default on any one axis. A flat detection (a floor sign, a mat, a
		// tabletop sent with size_m.Z = 0) would otherwise get a zero-thickness box, and a line
		// trace against a degenerate box is not reliable: the marker would draw and be untappable,
		// which is the exact failure DefaultExtent exists to prevent. The drawn box takes the same
		// floor, so what is seen stays what can be aimed at.
		const FVector Half = Point.bHasSize ? Point.WorldSize * 0.5 : DefaultExtent;
		const FVector Extent(FMath::Max(Half.X, DefaultExtent.X), FMath::Max(Half.Y, DefaultExtent.Y),
			FMath::Max(Half.Z, DefaultExtent.Z));
		// Only when it changed: SetBoxExtent destroys and recreates the physics state every time
		// it is called, and SetDetection runs for every tracked object on every push -- so a
		// stationary fridge of unchanging size would churn its Chaos body at the push rate.
		if (!Extent.Equals(Bounds->GetUnscaledBoxExtent()))
		{
			Bounds->SetBoxExtent(Extent);
		}
	}
}

namespace RammsDetectionMarkers
{
	/** The same colour at a given brightness and opacity. Both, because they do different work:
	 *  opacity decides how much of the scene shows through the fill, brightness how much the
	 *  marker recedes against it. */
	static FColor Shade(const FColor Base, const float Brightness, const float Opacity)
	{
		const float B = FMath::Clamp(Brightness, 0.0f, 1.0f);
		FColor		Out = Base;
		Out.R = static_cast<uint8>(FMath::RoundToInt(Base.R * B));
		Out.G = static_cast<uint8>(FMath::RoundToInt(Base.G * B));
		Out.B = static_cast<uint8>(FMath::RoundToInt(Base.B * B));
		Out.A = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Clamp(Opacity, 0.0f, 1.0f) * 255.0f), 0, 255));
		return Out;
	}
} // namespace RammsDetectionMarkers

float ARammsDetectionWorldMarker::GetSecondsSinceDetection() const
{
	if (!Detection.bHasSecondsSinceDetection)
	{
		return -1.0f;
	}
	const UWorld* World = GetWorld();
	const double  SincePush = World ? FMath::Max(0.0, World->GetTimeSeconds() - DetectionReceivedTime) : 0.0;
	return Detection.SecondsSinceDetection + static_cast<float>(SincePush);
}

float ARammsDetectionWorldMarker::GetStaleness() const
{
	// Not gated on bDimWhenStale: how old this is and whether the box fades for it are separate
	// questions, and the two properties read as independent in the editor. Gating here made
	// turning dimming off also delete the age from the label.
	const float Age = GetSecondsSinceDetection();
	if (Age < 0.0f)
	{
		return 0.0f;
	}
	const float FullyStaleAt = FMath::Max(FullyStaleAfterSeconds, StaleAfterSeconds + KINDA_SMALL_NUMBER);
	return FMath::Clamp(FMath::GetRangePct(StaleAfterSeconds, FullyStaleAt, Age), 0.0f, 1.0f);
}

FColor ARammsDetectionWorldMarker::GetBaseColour() const
{
	return Detection.bSelected
		? SelectedColour
		: (Detection.bPositionFromProducer ? SentColour : DerivedColour);
}

FLinearColor ARammsDetectionWorldMarker::GetLabelColour() const
{
	return FLinearColor(GetBaseColour());
}

FString ARammsDetectionWorldMarker::GetLabelText() const
{
	return Detection.PerceptionClass.IsEmpty() ? Detection.InstanceId.ToString() : Detection.PerceptionClass;
}

FString ARammsDetectionWorldMarker::GetAgeText() const
{
	if (!bShowAgeWhenStale || GetStaleness() <= 0.0f)
	{
		return FString();
	}
	// A number, because "dimmer than the others" is a comparison and there may be nothing to
	// compare against -- one stale marker alone looks like a marker.
	return FString::Printf(TEXT("%.0fs"), GetSecondsSinceDetection());
}

FVector ARammsDetectionWorldMarker::GetLabelAnchorLocation() const
{
	const double Top = Bounds ? Bounds->GetUnscaledBoxExtent().Z : 0.0;
	return GetActorLocation() + FVector(0, 0, Top + 10.0);
}

void ARammsDetectionWorldMarker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bDrawMarker || !Bounds)
	{
		return;
	}
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FColor Base = GetBaseColour();
	const bool	 bSelected = Detection.bSelected;

	// How far from live this marker is, 0 to 1. Both the colour and the fill fade with it, so a
	// memory recedes while a live detection stays forward; what it must NOT do is disappear, or
	// the operator loses the only handle they have for asking about the object.
	const float Staleness = bDimWhenStale ? GetStaleness() : 0.0f;
	const float StaleScale = FMath::Lerp(1.0f, FMath::Clamp(StaleBrightness, 0.0f, 1.0f), Staleness);

	const float	 Brightness = (bSelected ? 1.0f : UnselectedBrightness) * StaleScale;
	const FColor FillColour = RammsDetectionMarkers::Shade(
		Base, Brightness, (bSelected ? SelectedFillOpacity : FillOpacity) * StaleScale);
	const FColor LineColour = RammsDetectionMarkers::Shade(Base, Brightness, 1.0f);

	// The box the tap actually hits, so what is drawn is what can be aimed at. Drawing a different
	// shape from the collision is how a marker comes to look like it missed a tap it never had.
	const FVector Centre = GetActorLocation();
	const FVector Extent = Bounds->GetUnscaledBoxExtent();
	const FQuat	  Rotation = GetActorQuat();

	// Fill first, outline over it: the outline is what gives the box its edges once several of them
	// overlap, and a fill drawn afterwards would wash the near ones out.
	DrawDebugSolidBox(World, Centre, Extent, Rotation, FillColour, false, -1.0f, 0);
	DrawDebugBox(World, Centre, Extent, Rotation, LineColour, false, -1.0f, 0,
		bSelected ? SelectedLineThickness : LineThickness);

	if (bDrawDebugLabel)
	{
		// Bare text with no plate behind it: legible over a pale wall, invisible over a bright
		// one. URammsDetectionWorldLabels is the version meant to be read.
		FString		  Age = GetAgeText();
		const FString Label = Age.IsEmpty() ? GetLabelText() : GetLabelText() + TEXT("  ") + Age;
		DrawDebugString(World, GetLabelAnchorLocation(), Label, nullptr, LineColour, 0.0f, true,
			bSelected ? 1.25f : 1.0f);
	}
}

void ARammsDetectionWorldMarker::ReportTapped() const
{
	if (Detection.InstanceId.IsNone())
	{
		// Nothing for the orchestrator to match on, the rule PostObjectTapped's own callers follow.
		return;
	}
	if (URammsBridgeSubsystem* Bridge = GEngine ? GEngine->GetEngineSubsystem<URammsBridgeSubsystem>() : nullptr)
	{
		Bridge->PostObjectTapped(Detection.InstanceId.ToString(), Detection.Source);
	}
}

void ARammsDetectionWorldMarker::OnRammsWorldTapped_Implementation(const FHitResult& /*Hit*/)
{
	ReportTapped();
}

namespace RammsDetectionMarkers
{
	/** What the engine's click and touch routes can honestly say about where the tap landed: which
	 *  thing was touched, and nothing about a ray, because there was no trace behind these. */
	static FHitResult TouchedHit(AActor* Actor, UPrimitiveComponent* Component)
	{
		FHitResult Hit;
		Hit.HitObjectHandle = FActorInstanceHandle(Actor);
		Hit.Component = Component;
		if (Actor)
		{
			Hit.Location = Hit.ImpactPoint = Actor->GetActorLocation();
		}
		return Hit;
	}
} // namespace RammsDetectionMarkers

// Both of the engine's own routes go through the interface rather than straight to ReportTapped, so
// a Blueprint subclass that overrides On Ramms World Tapped overrides all three at once. They fire
// only where APlayerController's click trace can see this marker, which with the default overlap
// response it cannot -- see TapChannel.
void ARammsDetectionWorldMarker::HandleClicked(UPrimitiveComponent* TouchedComponent, FKey /*ButtonPressed*/)
{
	IRammsWorldTappable::Execute_OnRammsWorldTapped(this, RammsDetectionMarkers::TouchedHit(this, TouchedComponent));
}

void ARammsDetectionWorldMarker::HandleTouchBegin(ETouchIndex::Type /*FingerIndex*/, UPrimitiveComponent* TouchedComponent)
{
	IRammsWorldTappable::Execute_OnRammsWorldTapped(this, RammsDetectionMarkers::TouchedHit(this, TouchedComponent));
}

URammsDetectionWorldMarkers::URammsDetectionWorldMarkers()
{
	// Ticks only to draw the debug markers; the resolving itself is driven by the bridge push.
	PrimaryComponentTick.bCanEverTick = true;
	MarkerClass = ARammsDetectionWorldMarker::StaticClass();
}

FVector URammsDetectionWorldMarkers::CameraPointToWorld(
	const FVector& PointMetres, const FTransform& CameraExtrinsic)
{
	// Shaders/Private/RammsProjection.ush maps UE-local to the pinhole model as
	//   CamX = UE_Y,  CamY = -UE_Z,  CamZ = UE_X
	// so the inverse, which is what a detection needs, is
	//   UE_X = CamZ,  UE_Y = CamX,  UE_Z = -CamY
	// Keeping these two in step is the whole point: a marker that disagrees with the projector
	// lands off the pixels the object was detected in, and nothing reports that.
	const FVector Local(PointMetres.Z, PointMetres.X, -PointMetres.Y);
	return CameraExtrinsic.TransformPosition(Local * RammsDetectionMarkers::MetresToUU);
}

FVector URammsDetectionWorldMarkers::ProducerWorldPointToWorld(const FVector& PointMetres)
{
	// Right-handed Z-up (X forward, Y left) to Unreal's left-handed Z-up: negate Y.
	return FVector(PointMetres.X, -PointMetres.Y, PointMetres.Z) * RammsDetectionMarkers::MetresToUU;
}

FRotator URammsDetectionWorldMarkers::ProducerRotationToWorld(
	const FQuat& ProducerRotation, bool bCameraFrame, const FTransform& CameraExtrinsic)
{
	// B^T * R * B, where B converts a POINT from the producer's frame to Unreal's.
	//
	// The direction matters and is easy to invert. Unreal applies a matrix to a row vector, so
	// requiring that converting a rotated point equals rotating a converted point,
	//     (v * R) * B == (v * B) * R_ue   for every v
	// gives R * B == B * R_ue, hence R_ue = B^T * R * B. The other order passes a symmetric basis
	// -- which the world one is, being diagonal -- and silently fails for the camera one, which is
	// not. B is orthogonal so its transpose is its inverse, and although B mirrors (determinant
	// -1) the conjugation yields a proper rotation with the handedness change accounted for.
	// Doing it this way rather than negating quaternion components leaves no sign to get wrong.
	const FMatrix Basis = bCameraFrame
		? RammsDetectionMarkers::CameraToUnrealBasis()
		: RammsDetectionMarkers::ProducerWorldToUnrealBasis();
	const FMatrix Converted = Basis.GetTransposed() * FQuatRotationMatrix(ProducerRotation) * Basis;

	// A camera-frame rotation is relative to the camera, so the camera's own pose still applies.
	const FQuat Local = Converted.ToQuat().GetNormalized();
	return (bCameraFrame ? (CameraExtrinsic.GetRotation() * Local) : Local).Rotator();
}

FVector URammsDetectionWorldMarkers::UnprojectToCameraLocal(
	const FVector2D& NormalizedPoint, float DepthUU, const FVector4& Intrinsics, const FVector2D& ImageSize)
{
	// RammsProjection.ush:94 solved for the point:  CamX = (u - cx) * Depth / fx
	const double FocalX = Intrinsics.X, FocalY = Intrinsics.Y;
	if (FMath::IsNearlyZero(FocalX) || FMath::IsNearlyZero(FocalY))
	{
		return FVector::ZeroVector;
	}
	const double U = NormalizedPoint.X * ImageSize.X;
	const double V = NormalizedPoint.Y * ImageSize.Y;
	const double CamX = (U - Intrinsics.Z) * DepthUU / FocalX;
	const double CamY = (V - Intrinsics.W) * DepthUU / FocalY;

	// Same axis mapping as CameraPointToWorld, which is the shader's inverted: UE_X is the optical
	// axis, so the range goes there.
	return FVector(DepthUU, CamX, -CamY);
}

void URammsDetectionWorldMarkers::ForEachCameraProvider(
	TFunctionRef<bool(UObject*, IRammsCameraProvider*)> Visit) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Actors first, then components, which is the order and the pair of passes
	// URammsCameraProjectionManager::DiscoverProviders uses.
	TArray<AActor*> ProviderActors;
	UGameplayStatics::GetAllActorsWithInterface(World, URammsCameraProvider::StaticClass(), ProviderActors);
	for (AActor* Actor : ProviderActors)
	{
		if (IRammsCameraProvider* Provider = Cast<IRammsCameraProvider>(Actor))
		{
			if (!Visit(Actor, Provider))
			{
				return;
			}
		}
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		for (UActorComponent* Component : (*It)->GetComponents())
		{
			if (!IsValid(Component))
			{
				continue;
			}
			if (IRammsCameraProvider* Provider = Cast<IRammsCameraProvider>(Component))
			{
				if (!Visit(Component, Provider))
				{
					return;
				}
			}
		}
	}
}

UObject* URammsDetectionWorldMarkers::FindProviderForStream(const FString& StreamID) const
{
	if (StreamID.IsEmpty())
	{
		return nullptr;
	}

	if (const TWeakObjectPtr<UObject>* Cached = ProviderByStream.Find(StreamID))
	{
		UObject* Object = Cached->Get();
		// Still alive AND still owning that stream: a provider can drop a stream without dying.
		if (IRammsCameraProvider* Provider = Cast<IRammsCameraProvider>(Object))
		{
			FRammsCameraStreamInfo Info;
			if (Provider->GetStreamInfo(StreamID, Info))
			{
				return Object;
			}
		}
		ProviderByStream.Remove(StreamID);
	}

	UObject* Found = nullptr;
	ForEachCameraProvider([&StreamID, &Found](UObject* Object, IRammsCameraProvider* Provider) {
		FRammsCameraStreamInfo Info;
		if (Provider->GetStreamInfo(StreamID, Info))
		{
			Found = Object;
			return false;
		}
		return true;
	});
	if (Found)
	{
		ProviderByStream.Add(StreamID, Found);
	}
	return Found;
}

bool URammsDetectionWorldMarkers::FindDepthSibling(
	const FString& ColourStreamID, FString& OutDepthStreamID, TScriptInterface<IRammsCameraProvider>& OutProvider) const
{
	UObject*			   Object = FindProviderForStream(ColourStreamID);
	IRammsCameraProvider*  Provider = Cast<IRammsCameraProvider>(Object);
	FRammsCameraStreamInfo Colour;
	if (Provider && Provider->GetStreamInfo(ColourStreamID, Colour))
	{
		// Streams sharing a GroupID are the same physical camera, which is the pairing the
		// projection manager already relies on to drive PGM.
		if (!Colour.GroupID.IsEmpty())
		{
			for (const FRammsCameraStreamInfo& Candidate : Provider->GetAvailableStreams())
			{
				if (Candidate.GroupID == Colour.GroupID
					&& (Candidate.StreamRole == ERammsStreamRole::Depth || Candidate.bIsDepth))
				{
					OutDepthStreamID = Candidate.StreamID;
					OutProvider = TScriptInterface<IRammsCameraProvider>(Object);
					return true;
				}
			}
		}
	}
	return false;
}

FString URammsDetectionWorldMarkers::ResolveStreamID(const FString& Source) const
{
	// The bridge tags an object with the camera it was found in as "scene" or "overlay" -- and the
	// orchestrator's own objects with "" -- not with a stream id, so looking `source` up as one found
	// nothing for any real object. The settings that tell each camera widget which objects are its own
	// also say which stream each of those names means.
	// A stream that is registered names itself, even when it is also on an alias list (the scene list
	// carries "camera/scene/depth"): it may carry its own pose, as the bridge's depth stream does.
	if (!Source.IsEmpty() && FindProviderForStream(Source))
	{
		return Source;
	}
	const URammsCameraStreamSettings* Settings = GetDefault<URammsCameraStreamSettings>();
	const auto						  Lists = [&Source](const TArray<FString>& Names) {
		   return Names.ContainsByPredicate([&Source](const FString& Name) { return Name.Equals(Source, ESearchCase::IgnoreCase); });
	};
	if (Settings && !Settings->SceneColorStreamID.IsEmpty() && Lists(Settings->SceneDetectionSources))
	{
		return Settings->SceneColorStreamID;
	}
	if (Settings && !Settings->OverlayColorStreamID.IsEmpty() && Lists(Settings->OverlayDetectionSources))
	{
		return Settings->OverlayColorStreamID;
	}
	return Source;
}

bool URammsDetectionWorldMarkers::EnsureDepthForwarding(UObject* ProviderObject) const
{
	// Depth bytes reach a provider only while a consumer asks the stream bridge for them. Only the
	// projection manager did, and only for CPU projection, so with GPU projection -- the default --
	// nothing was ever derived and nothing said why. Depth only: the colour streams' bytes are the
	// expensive ones, and nothing here reads them.
	AActor* Owner = Cast<AActor>(ProviderObject);
	if (!Owner)
	{
		if (const UActorComponent* Component = Cast<UActorComponent>(ProviderObject))
		{
			Owner = Component->GetOwner();
		}
	}
	URammsStreamCameraBridge* Bridge = Owner ? Owner->FindComponentByClass<URammsStreamCameraBridge>() : nullptr;
	if (!Bridge
		|| DepthForwardingBridges.ContainsByPredicate(
			[Bridge](const TWeakObjectPtr<URammsStreamCameraBridge>& Held) { return Held.Get() == Bridge; }))
	{
		return false;
	}
	Bridge->RequestDepthRawDataForwarding();
	DepthForwardingBridges.Add(Bridge);
	return true;
}

void URammsDetectionWorldMarkers::ReleaseDepthForwarding()
{
	for (const TWeakObjectPtr<URammsStreamCameraBridge>& Bridge : DepthForwardingBridges)
	{
		if (Bridge.IsValid())
		{
			Bridge->ReleaseDepthRawDataForwarding();
		}
	}
	DepthForwardingBridges.Reset();
}

bool URammsDetectionWorldMarkers::DeriveWorldLocation(
	const FString& Source, const FVector2D& NormalizedPoint, FVector& OutWorldLocation) const
{
	bool bNoDepthYet = false, bDepthDropout = false;
	return DeriveWorldLocationFor(Source, NormalizedPoint, OutWorldLocation, bNoDepthYet, bDepthDropout);
}

bool URammsDetectionWorldMarkers::DeriveWorldLocationFor(
	const FString& Source, const FVector2D& NormalizedPoint, FVector& OutWorldLocation, bool& bOutNoDepthYet, bool& bOutDepthDropout) const
{
	bOutNoDepthYet = false;
	bOutDepthDropout = false;

	// A point outside the image has no depth of its own. Reading the clamped edge pixel and
	// unprojecting anyway placed a pixel centroid taken for a normalized one hundreds of metres off,
	// as a plausible marker; outside the image is nowhere.
	const auto IsUnit = [](double Value) { return Value >= -0.001 && Value <= 1.001; };
	if (!IsUnit(NormalizedPoint.X) || !IsUnit(NormalizedPoint.Y))
	{
		return false;
	}
	const FVector2D Point(FMath::Clamp(NormalizedPoint.X, 0.0, 1.0), FMath::Clamp(NormalizedPoint.Y, 0.0, 1.0));

	const FString						   StreamID = ResolveStreamID(Source);
	FString								   DepthStreamID;
	TScriptInterface<IRammsCameraProvider> ProviderInterface;
	if (!FindDepthSibling(StreamID, DepthStreamID, ProviderInterface))
	{
		return false;
	}
	IRammsCameraProvider* Provider = ProviderInterface.GetInterface();
	if (!Provider)
	{
		return false;
	}

	FRammsCameraStreamInfo Colour, Depth;
	if (!Provider->GetStreamInfo(StreamID, Colour) || !Provider->GetStreamInfo(DepthStreamID, Depth))
	{
		return false;
	}
	// The pose and intrinsics come from whichever of the camera's streams carries both. The UI bridge
	// puts them on the depth frames only; another producer may put them on the colour. Colour first,
	// being the image the object's coordinates are in. Requiring them on the colour placed nothing for
	// the real bridge, and said so only at Verbose.
	const auto					  Complete = [](const FRammsCameraStreamInfo& Info) { return Info.bHasExtrinsic && Info.Intrinsics.Num() >= 4; };
	const FRammsCameraStreamInfo* Geometry = Complete(Colour) ? &Colour : (Complete(Depth) ? &Depth : nullptr);
	if (!Geometry)
	{
		// Once per stream: this is reached per object, per push, at the push rate.
		const FString Key = TEXT("geometry:") + StreamID;
		if (!WarnedStreams.Contains(Key))
		{
			WarnedStreams.Add(Key);
			UE_LOG(LogRammsDetectionMarkers, Warning,
				TEXT("Neither '%s' (%d intrinsics, %s) nor its depth stream '%s' (%d intrinsics, %s) carries both intrinsics and a pose, so its centroids cannot be unprojected. Send them in the frames' metadata, or send position_m."),
				*StreamID, Colour.Intrinsics.Num(), Colour.bHasExtrinsic ? TEXT("a pose") : TEXT("no pose"),
				*DepthStreamID, Depth.Intrinsics.Num(), Depth.bHasExtrinsic ? TEXT("a pose") : TEXT("no pose"));
		}
		return false;
	}

	// The depth is sampled at the colour image's normalized point, which is only the same point of the
	// scene when the depth image is registered to the colour one (aligned_depth_to_color). A raw depth
	// image of another field of view puts every derived marker off by an amount that grows towards the
	// edges -- plausible, and wrong. Say so once.
	const FString AlignKey = TEXT("aligned:") + DepthStreamID;
	if (!WarnedStreams.Contains(AlignKey) && Colour.Width > 0 && Colour.Height > 0 && Depth.Width > 0 && Depth.Height > 0)
	{
		const double ColourAspect = double(Colour.Width) / Colour.Height, DepthAspect = double(Depth.Width) / Depth.Height;
		bool		 bMisaligned = FMath::Abs(ColourAspect - DepthAspect) > 0.01 * ColourAspect;
		if (!bMisaligned && Colour.Intrinsics.Num() >= 4 && Depth.Intrinsics.Num() >= 4)
		{
			// The same field of view means the same intrinsics once each is divided by its image size.
			const double Scale[4] = { double(Colour.Width), double(Colour.Height), double(Colour.Width), double(Colour.Height) };
			const double DepthScale[4] = { double(Depth.Width), double(Depth.Height), double(Depth.Width), double(Depth.Height) };
			for (int32 i = 0; i < 4 && !bMisaligned; ++i)
			{
				const double C = Colour.Intrinsics[i] / Scale[i], D = Depth.Intrinsics[i] / DepthScale[i];
				bMisaligned = FMath::Abs(C - D) > 0.02 * FMath::Max(FMath::Abs(C), 1e-6);
			}
		}
		if (bMisaligned)
		{
			WarnedStreams.Add(AlignKey);
			UE_LOG(LogRammsDetectionMarkers, Warning,
				TEXT("Depth stream '%s' (%dx%d) does not look registered to colour '%s' (%dx%d): derived positions sample depth at the colour image's point and will be off. Send depth aligned to the colour image."),
				*DepthStreamID, Depth.Width, Depth.Height, *StreamID, Colour.Width, Colour.Height);
		}
	}

	const TArray<uint8>* Raw = nullptr;
	EPixelFormat		 RawFormat = PF_Unknown;
	if (!Provider->GetStreamRawData(DepthStreamID, Raw, RawFormat) || !Raw)
	{
		bOutNoDepthYet = true;
		if (EnsureDepthForwarding(ProviderInterface.GetObject()))
		{
			// The bytes come with the next depth frame: retry for a moment rather than leave the
			// objects that needed them unplaced until the next push.
			DepthRetryTicks = 60;
		}
		return false;
	}

	ERammsDepthFormat DepthFormat = Depth.DepthFormat;
	if (DepthFormat == ERammsDepthFormat::Unknown)
	{
		DepthFormat = Provider->GetStreamDepthFormat(DepthStreamID);
	}

	float DepthUU = 0.0f;
	// A size that disagrees with the registration is not a dropout, it is a misconfiguration, and
	// it would otherwise look exactly like one. Once per stream, as above.
	const int32 ExpectedBytes =
		Depth.Width * Depth.Height * RammsDetectionMarkers::BytesPerDepthPixel(DepthFormat);
	if (ExpectedBytes > 0 && Raw->Num() != ExpectedBytes && !WarnedStreams.Contains(DepthStreamID))
	{
		WarnedStreams.Add(DepthStreamID);
		UE_LOG(LogRammsDetectionMarkers, Warning,
			TEXT("Depth stream '%s' sent %d bytes but is registered %dx%d (%d bytes). No positions "
				 "will be derived from it until the two agree."),
			*DepthStreamID, Raw->Num(), Depth.Width, Depth.Height, ExpectedBytes);
	}

	if (!RammsDetectionMarkers::SampleDepthUU(
			*Raw, DepthFormat, Depth.Width, Depth.Height, Point, DepthSampleRadius, DepthUU))
	{
		// A dropout is ordinary -- a dark, shiny or distant surface returns nothing -- so this is
		// not a warning. The object keeps its 2D marker; a world one it already has is held for a
		// moment (DerivedDropoutHoldSeconds), and otherwise it gains none.
		bOutDepthDropout = true;
		return false;
	}

	const FVector4 Intrinsics(Geometry->Intrinsics[0], Geometry->Intrinsics[1], Geometry->Intrinsics[2], Geometry->Intrinsics[3]);
	const FVector  Local = UnprojectToCameraLocal(Point, DepthUU, Intrinsics, FVector2D(Geometry->Width, Geometry->Height));
	OutWorldLocation = Geometry->Extrinsic.TransformPosition(Local);
	return true;
}

void URammsDetectionWorldMarkers::BeginPlay()
{
	Super::BeginPlay();

	if (URammsBridgeSubsystem* Bridge = GEngine ? GEngine->GetEngineSubsystem<URammsBridgeSubsystem>() : nullptr)
	{
		Bridge->OnDetectedObjectsJson.AddDynamic(this, &URammsDetectionWorldMarkers::HandleDetectedObjectsJson);
	}

	// A camera that moves invalidates every camera-frame position it reported, and detections are
	// pushed on their own cadence -- without this a marker would sit at the old pose until the
	// producer happened to send again.
	ForEachCameraProvider([this](UObject* Object, IRammsCameraProvider* Provider) {
		ExtrinsicSubscriptions.Add(
			Provider->OnCameraExtrinsicUpdated().AddLambda([this](const FString& StreamID, const FTransform&) {
				// RammsStreamCameraBridge pushes an extrinsic for EVERY frame that carries camera
				// metadata, so this fires at frame rate per stream. Re-resolving there would
				// re-parse the whole payload each time; instead note it and do the work once, on
				// the next tick, and only when the camera that moved is one the current points
				// were actually placed against (or tried to be: a derived object waiting on this camera's
				// depth or pose is in UsedSources too).
				if (UsedSources.Contains(StreamID))
				{
					bReresolvePending = true;
				}
			}));
		SubscribedProviders.Add(Object);
		return true;
	});
}

void URammsDetectionWorldMarkers::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Depth was asked for a moment ago and has not arrived yet: try again each tick until it does,
	// for about a second, rather than leave the objects that need it unplaced until the next push.
	if (DepthRetryTicks > 0)
	{
		--DepthRetryTicks;
		bReresolvePending = true;
	}

	// At most one re-resolve a frame, however many cameras moved. Ahead of the debug-draw
	// early-out below, because markers have to follow a moving camera whether or not anything is
	// drawing boxes for them.
	if (bReresolvePending)
	{
		bReresolvePending = false;
		Reresolve();
	}

	if (!bDrawDebugMarkers || WorldPoints.IsEmpty())
	{
		return;
	}
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const FRammsDetectionWorldPoint& Point : WorldPoints)
	{
		// Green sent, cyan derived, yellow selected: which source placed it is the thing being
		// checked, so it is what the colour says.
		const FColor Colour = Point.bSelected
			? FColor::Yellow
			: (Point.bPositionFromProducer ? FColor::Green : FColor::Cyan);

		if (Point.bHasSize)
		{
			DrawDebugBox(World, Point.WorldLocation, Point.WorldSize * 0.5f,
				Point.WorldRotation.Quaternion(), Colour, false, -1.0f, 0, 1.5f);
		}
		else
		{
			// No size, so draw a cross rather than a box: a guessed box would look like knowledge.
			const float Arm = 8.0f;
			DrawDebugLine(World, Point.WorldLocation - FVector(Arm, 0, 0), Point.WorldLocation + FVector(Arm, 0, 0), Colour, false, -1.0f, 0, 1.5f);
			DrawDebugLine(World, Point.WorldLocation - FVector(0, Arm, 0), Point.WorldLocation + FVector(0, Arm, 0), Colour, false, -1.0f, 0, 1.5f);
			DrawDebugLine(World, Point.WorldLocation - FVector(0, 0, Arm), Point.WorldLocation + FVector(0, 0, Arm), Colour, false, -1.0f, 0, 1.5f);
		}

		const FString Label = Point.PerceptionClass.IsEmpty() ? Point.InstanceId.ToString() : Point.PerceptionClass;
		DrawDebugString(World, Point.WorldLocation + FVector(0, 0, 12.0f), Label, nullptr, Colour, 0.0f, true, 1.0f);
	}
}

void URammsDetectionWorldMarkers::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URammsBridgeSubsystem* Bridge = GEngine ? GEngine->GetEngineSubsystem<URammsBridgeSubsystem>() : nullptr)
	{
		Bridge->OnDetectedObjectsJson.RemoveAll(this);
	}

	for (int32 i = 0; i < SubscribedProviders.Num() && i < ExtrinsicSubscriptions.Num(); ++i)
	{
		if (IRammsCameraProvider* Provider = Cast<IRammsCameraProvider>(SubscribedProviders[i]))
		{
			Provider->OnCameraExtrinsicUpdated().Remove(ExtrinsicSubscriptions[i]);
		}
	}
	ExtrinsicSubscriptions.Reset();
	SubscribedProviders.Reset();

	DestroyMarkers();
	ReleaseDepthForwarding();

	Super::EndPlay(EndPlayReason);
}

void URammsDetectionWorldMarkers::BeginDestroy()
{
	// A component that never began play (made at runtime and never registered) gets no EndPlay.
	ReleaseDepthForwarding();
	Super::BeginDestroy();
}

void URammsDetectionWorldMarkers::DestroyMarkers()
{
	for (const TPair<FName, TObjectPtr<AActor>>& Pair : Markers)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Destroy();
		}
	}
	Markers.Reset();
}

bool URammsDetectionWorldMarkers::IsAcceptedSource(const FString& Source) const
{
	if (AcceptedSources.IsEmpty())
	{
		return true;
	}
	return AcceptedSources.ContainsByPredicate(
		[&Source](const FString& Candidate) { return Candidate.Equals(Source, ESearchCase::IgnoreCase); });
}

bool URammsDetectionWorldMarkers::ResolveCameraExtrinsic(const FString& Source, FTransform& OutExtrinsic) const
{
	// `source` names the camera the object was detected on: a stream id, or one of the bridge's names
	// for a camera ("scene", "overlay").
	const FString		   StreamID = ResolveStreamID(Source);
	IRammsCameraProvider*  Provider = Cast<IRammsCameraProvider>(FindProviderForStream(StreamID));
	FRammsCameraStreamInfo Info;
	if (Provider && Provider->GetStreamInfo(StreamID, Info) && Info.bHasExtrinsic)
	{
		OutExtrinsic = Info.Extrinsic;
		return true;
	}
	// The UI bridge sends a camera's pose on its depth frames only: the same camera, so the same pose
	// (to within the colour/depth baseline, which an aligned depth image does not have).
	FString								   DepthStreamID;
	TScriptInterface<IRammsCameraProvider> DepthProvider;
	if (FindDepthSibling(StreamID, DepthStreamID, DepthProvider) && DepthProvider.GetInterface()
		&& DepthProvider.GetInterface()->GetStreamInfo(DepthStreamID, Info) && Info.bHasExtrinsic)
	{
		OutExtrinsic = Info.Extrinsic;
		return true;
	}
	return false;
}

void URammsDetectionWorldMarkers::AddCameraStreams(const FString& StreamID)
{
	// The camera's colour stream and its depth sibling: the pose updates come from whichever carries
	// the pose -- the depth, from the UI bridge -- and either moving invalidates the positions.
	UsedSources.Add(StreamID);
	FString								   DepthStreamID;
	TScriptInterface<IRammsCameraProvider> Provider;
	if (FindDepthSibling(StreamID, DepthStreamID, Provider))
	{
		UsedSources.Add(DepthStreamID);
	}
}

void URammsDetectionWorldMarkers::SetDetectedObjectsJson(const FString& DetectedObjectsJson)
{
	LastPayload = DetectedObjectsJson;
	Reresolve();
}

void URammsDetectionWorldMarkers::HandleDetectedObjectsJson(const FString& DetectedObjectsJson)
{
	SetDetectedObjectsJson(DetectedObjectsJson);
}

void URammsDetectionWorldMarkers::Reresolve()
{
	WorldPoints.Reset();
	// Rebuilt from what this resolve leans on, so only those cameras moving brings it back.
	UsedSources.Reset();
	bool bMissingDepth = false;
	// This resolve's derived locations, replacing LastDerived at the end: an object that has left
	// the push holds nothing.
	const double						Now = FPlatformTime::Seconds();
	TMap<FName, TPair<FVector, double>> Derived;

	TSharedPtr<FJsonObject>			Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(LastPayload);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		// The camera widget already warns about a malformed payload; do not warn twice.
		return;
	}

	FString PushSource;
	Root->TryGetStringField(TEXT("stream_id"), PushSource);
	FString Selected;
	Root->TryGetStringField(TEXT("selected_instance_id"), Selected);

	const TArray<TSharedPtr<FJsonValue>>* Objects = nullptr;
	if (Root->TryGetArrayField(TEXT("objects"), Objects))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Objects)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(Object))
			{
				continue;
			}

			FVector	   PointMetres;
			const bool bSentPosition = RammsDetectionMarkers::ReadPoint(**Object, TEXT("position_m"), PointMetres);
			if (!bSentPosition && !bDeriveFromDepth)
			{
				continue;
			}

			// Parsed up here so the warnings below can name the object without GetStringField,
			// which logs a LogJson warning AND an error of its own when the field is absent.
			FString InstanceId;
			(*Object)->TryGetStringField(TEXT("instance_id"), InstanceId);

			FString Source;
			if (!(*Object)->TryGetStringField(TEXT("source"), Source) || Source.IsEmpty())
			{
				Source = PushSource;
			}

			FString Frame;
			if (!(*Object)->TryGetStringField(TEXT("frame"), Frame) || Frame.IsEmpty())
			{
				Frame = TEXT("camera");
			}

			if (!IsAcceptedSource(Source))
			{
				continue;
			}

			FRammsDetectionWorldPoint Point;
			if (!bSentPosition)
			{
				// An object still in the push that cannot be placed this time keeps its last derived
				// location for DerivedDropoutHoldSeconds: its depth dropped out, or the producer sent it
				// without a 2D position for a frame or two (occluded). Holding does not renew the hold.
				const FName Id = InstanceId.IsEmpty() ? NAME_None : FName(*InstanceId);
				const auto	Hold = [&]() {
					 // A second copy of an id this resolve already placed is dropped, as it always was: holding
					 // it would put the marker back at the old location, the last copy winning in ApplyMarkers.
					 const TPair<FVector, double>* Held = (Id.IsNone() || Derived.Contains(Id)) ? nullptr : LastDerived.Find(Id);
					 if (!Held || Now - Held->Value > DerivedDropoutHoldSeconds)
					 {
						 return false;
					 }
					 Point.WorldLocation = Held->Key;
					 Derived.Add(Id, *Held);
					 return true;
				};

				// Nothing sent, so unproject the object's own centroid through its depth stream.
				// Falls back to the box centre, since a producer may send one without the other.
				FVector2D ImagePoint;
				bool	  bPixels = false;
				if (!RammsDetectionMarkers::ReadImagePoint(**Object, ImagePoint, bPixels))
				{
					if (!Hold())
					{
						continue;
					}
				}
				else
				{
					const FString StreamID = ResolveStreamID(Source);
					AddCameraStreams(StreamID);
					if (bPixels)
					{
						// Pixels of the source's frame, as the bridge sends them, so normalized by that
						// stream's size -- the same frame the depth is sampled against.
						IRammsCameraProvider*  Provider = Cast<IRammsCameraProvider>(FindProviderForStream(StreamID));
						FRammsCameraStreamInfo Info;
						if (!Provider || !Provider->GetStreamInfo(StreamID, Info) || Info.Width <= 0 || Info.Height <= 0)
						{
							continue;
						}
						ImagePoint = FVector2D(ImagePoint.X / Info.Width, ImagePoint.Y / Info.Height);
					}
					bool bNoDepthYet = false, bDepthDropout = false;
					if (DeriveWorldLocationFor(Source, ImagePoint, Point.WorldLocation, bNoDepthYet, bDepthDropout))
					{
						if (!Id.IsNone())
						{
							Derived.Add(Id, TPair<FVector, double>(Point.WorldLocation, Now));
						}
					}
					else
					{
						bMissingDepth |= bNoDepthYet;
						// Only a dropout holds: a point outside the image, or a camera without geometry,
						// is not going to come back next frame.
						if (!bDepthDropout || !Hold())
						{
							continue;
						}
					}
				}
				Point.Source = Source;
			}
			else if (Frame.Equals(TEXT("world"), ESearchCase::IgnoreCase))
			{
				// A world-frame position stands on its own: `source` decided above only whether
				// this component is the one that should show it.
				Point.WorldLocation = ProducerWorldPointToWorld(PointMetres);
				Point.Source = Source;
			}
			else if (Frame.Equals(TEXT("camera"), ESearchCase::IgnoreCase))
			{
				FTransform Extrinsic;
				AddCameraStreams(ResolveStreamID(Source));
				if (!ResolveCameraExtrinsic(Source, Extrinsic))
				{
					// Without the camera's pose a camera-frame point cannot be placed at all.
					// Say so: silently dropping it looks identical to the producer not sending it.
					// Once per stream, though -- this sits in a per-object loop inside a push that
					// arrives at frame rate, and a camera that never sends a pose would otherwise
					// put hundreds of identical lines a second into the robot's log.
					if (!WarnedStreams.Contains(Source))
					{
						WarnedStreams.Add(Source);
						UE_LOG(LogRammsDetectionMarkers, Warning,
							TEXT("No extrinsic for stream '%s'; cannot place camera-frame positions "
								 "for it (first was '%s'). Further objects on this stream are silent."),
							*Source, *InstanceId);
					}
					continue;
				}
				Point.WorldLocation = CameraPointToWorld(PointMetres, Extrinsic);
				Point.Source = Source;
			}
			else
			{
				// Keyed by the frame rather than the stream: a producer sending one bad spelling
				// sends it for every object, every push.
				if (!WarnedStreams.Contains(Frame))
				{
					WarnedStreams.Add(Frame);
					UE_LOG(LogRammsDetectionMarkers, Warning,
						TEXT("Unknown frame '%s' (expected 'camera' or 'world')"), *Frame);
				}
				continue;
			}

			(*Object)->TryGetStringField(TEXT("perception_class"), Point.PerceptionClass);
			Point.InstanceId = InstanceId.IsEmpty() ? NAME_None : FName(*InstanceId);
			Point.bPositionFromProducer = bSentPosition;

			// Size is the producer's alone: one depth sample says where a surface is, never how
			// big the thing behind it is.
			const bool bCameraFrame = !Frame.Equals(TEXT("world"), ESearchCase::IgnoreCase);
			FVector	   SizeMetres;
			if (RammsDetectionMarkers::ReadPoint(**Object, TEXT("size_m"), SizeMetres)
				&& !SizeMetres.IsNearlyZero())
			{
				// Extents follow the SAME axis map as a point, or the box is built on the wrong
				// axes. In the optical frame X is right, Y is DOWN and Z is the range, while
				// Unreal's are forward, right and up -- so without this remap a fridge sent as
				// 1.80 m tall in camera coordinates comes back 1.80 m WIDE and 0.70 m tall: a box
				// of the right dimensions in the wrong orientation, which reads as a detector
				// error rather than a frame one.
				//
				// The producer's world frame needs no remap: its axes are Unreal's up to the sign
				// of Y, and an extent has no sign to flip, which is what the abs is for. That is
				// also why this went unnoticed -- the only frame under test was the one that
				// happens not to need it.
				const FVector SizeUU = SizeMetres.GetAbs() * RammsDetectionMarkers::MetresToUU;
				Point.WorldSize = bCameraFrame ? FVector(SizeUU.Z, SizeUU.X, SizeUU.Y) : SizeUU;
				Point.bHasSize = true;
			}

			// Orientation is read whether or not a size came with it. It used to be nested inside
			// the size block, so a detector that sent position_m and orientation but no size_m had
			// its pose silently dropped -- bHasRotation false, WorldRotation identity -- while
			// those two struct members are advertised independently of bHasSize, and a Blueprint
			// marker with a mesh or an approach vector reads them on their own.
			{
				FTransform Extrinsic = FTransform::Identity;
				if (bCameraFrame)
				{
					ResolveCameraExtrinsic(Source, Extrinsic);
				}

				FQuat  Orientation;
				double Yaw = 0.0;
				if (RammsDetectionMarkers::ReadQuat(**Object, TEXT("orientation"), Orientation))
				{
					Point.WorldRotation = ProducerRotationToWorld(Orientation, bCameraFrame, Extrinsic);
					Point.bHasRotation = true;
				}
				else if ((*Object)->TryGetNumberField(TEXT("yaw_deg"), Yaw))
				{
					// A heading turns about UP, and up is not the same axis in the two frames: it
					// is +Z in the producer's world frame but -Y in the optical one, where +Z is
					// the way the camera is looking. Taking +Z in both would have rolled a
					// camera-frame box about the view direction and called it a heading.
					const FVector Up = bCameraFrame ? -FVector::YAxisVector : FVector::ZAxisVector;
					const FQuat	  YawOnly(Up, FMath::DegreesToRadians(Yaw));
					Point.WorldRotation = ProducerRotationToWorld(YawOnly, bCameraFrame, Extrinsic);
					Point.bHasRotation = true;
				}
			}

			// Negative is taken as "not sent": a producer with no answer should leave the field
			// out, but a sentinel is the obvious way to get that wrong and must not read as a time.
			double SecondsSinceDetection = 0.0;
			if ((*Object)->TryGetNumberField(TEXT("time_since_detection_s"), SecondsSinceDetection)
				&& SecondsSinceDetection >= 0.0)
			{
				Point.SecondsSinceDetection = static_cast<float>(SecondsSinceDetection);
				Point.bHasSecondsSinceDetection = true;
			}

			Point.bSelected = !Selected.IsEmpty() && Selected == InstanceId;
			WorldPoints.Add(MoveTemp(Point));
		}
	}

	if (!bMissingDepth)
	{
		DepthRetryTicks = 0;
	}
	LastDerived = MoveTemp(Derived);
	ApplyMarkers();
	OnDetectionWorldPoints.Broadcast(WorldPoints);
}

void URammsDetectionWorldMarkers::ApplyMarkers()
{
	if (!bSpawnMarkers || !MarkerClass)
	{
		// A deliberate opt-out (MarkerClass defaults to the tappable marker), but the ones already
		// spawned have to go with it: left alone they freeze at their last positions until EndPlay,
		// still answering tap traces for detections that may no longer exist.
		DestroyMarkers();
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Every push is the full list, so anything absent from it is gone.
	TSet<FName> Seen;
	for (const FRammsDetectionWorldPoint& Point : WorldPoints)
	{
		if (Point.InstanceId.IsNone())
		{
			continue; // Without an id there is nothing to key a persistent marker on.
		}
		Seen.Add(Point.InstanceId);

		// Pooled by instance id: the same object keeps the same actor across pushes, so a marker
		// being looked at or tapped does not blink out from under the operator every frame.
		TObjectPtr<AActor>* Existing = Markers.Find(Point.InstanceId);
		AActor*				Marker = (Existing && IsValid(*Existing)) ? Existing->Get() : nullptr;
		if (!Marker)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Marker = World->SpawnActor<AActor>(MarkerClass, Point.WorldLocation, Point.WorldRotation, Params);
			if (!Marker)
			{
				continue;
			}
			Markers.Add(Point.InstanceId, Marker);
		}

		// Size, orientation and selection all move between pushes, not just position. A marker
		// told only where to stand would keep the extent of whatever it was first seen as.
		if (ARammsDetectionWorldMarker* Typed = Cast<ARammsDetectionWorldMarker>(Marker))
		{
			Typed->SetDetection(Point);
		}
		else
		{
			Marker->SetActorLocationAndRotation(Point.WorldLocation, Point.WorldRotation.Quaternion());
		}
	}

	for (auto It = Markers.CreateIterator(); It; ++It)
	{
		if (!Seen.Contains(It.Key()))
		{
			if (IsValid(It.Value()))
			{
				It.Value()->Destroy();
			}
			It.RemoveCurrent();
		}
	}
}
