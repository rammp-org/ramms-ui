// RammsDetectionWorldMarkers.cpp

#include "RammsDetectionWorldMarkers.h"

#include "Components/BoxComponent.h"
#include "Dom/JsonObject.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IRammsCameraProvider.h"
#include "RammsBridgeSubsystem.h"
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
		if (Stride == 0 || Width <= 0 || Height <= 0 || Data.Num() < Width * Height * Stride)
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
	static bool ReadCentroid(const FJsonObject& Object, FVector2D& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Centroid = nullptr;
		if (Object.TryGetArrayField(TEXT("centroid"), Centroid) && Centroid->Num() >= 2)
		{
			Out = FVector2D((*Centroid)[0]->AsNumber(), (*Centroid)[1]->AsNumber());
			return true;
		}
		const TArray<TSharedPtr<FJsonValue>>* Box = nullptr;
		if (Object.TryGetArrayField(TEXT("box"), Box) && Box->Num() >= 4)
		{
			Out = FVector2D(
				((*Box)[0]->AsNumber() + (*Box)[2]->AsNumber()) * 0.5,
				((*Box)[1]->AsNumber() + (*Box)[3]->AsNumber()) * 0.5);
			return true;
		}
		return false;
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

void ARammsDetectionWorldMarker::BeginPlay()
{
	Super::BeginPlay();

	// A Blueprint subclass's TapChannel is not applied until after the constructor has run.
	ApplyCollisionResponse();

#if !ENABLE_DRAW_DEBUG
	// Everything this marker shows is a debug draw, and those compile out of Shipping and Test.
	// Say so once rather than ship a build whose markers can be tapped and cannot be seen -- the
	// state the class comment calls not a marker at all.
	UE_LOG(LogRammsDetectionMarkers, Warning,
		TEXT("%s: debug drawing is compiled out of this build, so world markers are invisible. "
			 "Give MarkerClass a Blueprint subclass with a mesh, or ship Development."),
		*GetName());
#endif
}

void ARammsDetectionWorldMarker::SetDetection(const FRammsDetectionWorldPoint& Point)
{
	Detection = Point;
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
		Bounds->SetBoxExtent(FVector(FMath::Max(Half.X, DefaultExtent.X),
			FMath::Max(Half.Y, DefaultExtent.Y), FMath::Max(Half.Z, DefaultExtent.Z)));
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

	const FColor Base = Detection.bSelected
		? SelectedColour
		: (Detection.bPositionFromProducer ? SentColour : DerivedColour);

	const bool	 bSelected = Detection.bSelected;
	const float	 Brightness = bSelected ? 1.0f : UnselectedBrightness;
	const FColor FillColour = RammsDetectionMarkers::Shade(Base, Brightness, bSelected ? SelectedFillOpacity : FillOpacity);
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

	const FString Label = Detection.PerceptionClass.IsEmpty()
		? Detection.InstanceId.ToString()
		: Detection.PerceptionClass;
	DrawDebugString(World, Centre + FVector(0, 0, Extent.Z + 10.0), Label, nullptr, LineColour, 0.0f, true,
		bSelected ? 1.25f : 1.0f);
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

bool URammsDetectionWorldMarkers::FindDepthSibling(
	const FString& ColourStreamID, FString& OutDepthStreamID, TScriptInterface<IRammsCameraProvider>& OutProvider) const
{
	const UWorld* World = GetWorld();
	if (!World || ColourStreamID.IsEmpty())
	{
		return false;
	}

	for (TObjectIterator<UActorComponent> It; It; ++It)
	{
		UActorComponent* Component = *It;
		if (!IsValid(Component) || Component->GetWorld() != World)
		{
			continue;
		}
		IRammsCameraProvider*  Provider = Cast<IRammsCameraProvider>(Component);
		FRammsCameraStreamInfo Colour;
		if (!Provider || !Provider->GetStreamInfo(ColourStreamID, Colour))
		{
			continue;
		}
		// Streams sharing a GroupID are the same physical camera, which is the pairing the
		// projection manager already relies on to drive PGM.
		if (Colour.GroupID.IsEmpty())
		{
			continue;
		}
		for (const FRammsCameraStreamInfo& Candidate : Provider->GetAvailableStreams())
		{
			if (Candidate.GroupID == Colour.GroupID
				&& (Candidate.StreamRole == ERammsStreamRole::Depth || Candidate.bIsDepth))
			{
				OutDepthStreamID = Candidate.StreamID;
				OutProvider = TScriptInterface<IRammsCameraProvider>(Component);
				return true;
			}
		}
	}
	return false;
}

bool URammsDetectionWorldMarkers::DeriveWorldLocation(
	const FString& Source, const FVector2D& NormalizedPoint, FVector& OutWorldLocation) const
{
	FString								   DepthStreamID;
	TScriptInterface<IRammsCameraProvider> ProviderInterface;
	if (!FindDepthSibling(Source, DepthStreamID, ProviderInterface))
	{
		return false;
	}
	IRammsCameraProvider* Provider = ProviderInterface.GetInterface();
	if (!Provider)
	{
		return false;
	}

	FRammsCameraStreamInfo Colour, Depth;
	if (!Provider->GetStreamInfo(Source, Colour) || !Provider->GetStreamInfo(DepthStreamID, Depth))
	{
		return false;
	}
	if (!Colour.bHasExtrinsic)
	{
		UE_LOG(LogRammsDetectionMarkers, Verbose,
			TEXT("No extrinsic for '%s' yet; cannot place a derived position"), *Source);
		return false;
	}
	if (Colour.Intrinsics.Num() < 4)
	{
		UE_LOG(LogRammsDetectionMarkers, Warning,
			TEXT("Stream '%s' reports no intrinsics, so a centroid cannot be unprojected; the producer must send position_m for this camera"),
			*Source);
		return false;
	}

	const TArray<uint8>* Raw = nullptr;
	EPixelFormat		 RawFormat = PF_Unknown;
	if (!Provider->GetStreamRawData(DepthStreamID, Raw, RawFormat) || !Raw)
	{
		return false;
	}

	ERammsDepthFormat DepthFormat = Depth.DepthFormat;
	if (DepthFormat == ERammsDepthFormat::Unknown)
	{
		DepthFormat = Provider->GetStreamDepthFormat(DepthStreamID);
	}

	float DepthUU = 0.0f;
	if (!RammsDetectionMarkers::SampleDepthUU(
			*Raw, DepthFormat, Depth.Width, Depth.Height, NormalizedPoint, DepthSampleRadius, DepthUU))
	{
		// A dropout is ordinary -- a dark, shiny or distant surface returns nothing -- so this is
		// not a warning. The object keeps its 2D marker and gains no world one.
		return false;
	}

	const FVector4 Intrinsics(Colour.Intrinsics[0], Colour.Intrinsics[1], Colour.Intrinsics[2], Colour.Intrinsics[3]);
	const FVector  Local = UnprojectToCameraLocal(NormalizedPoint, DepthUU, Intrinsics, FVector2D(Colour.Width, Colour.Height));
	OutWorldLocation = Colour.Extrinsic.TransformPosition(Local);
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
	if (const UWorld* World = GetWorld())
	{
		for (TObjectIterator<UActorComponent> It; It; ++It)
		{
			UActorComponent* Component = *It;
			if (!IsValid(Component) || Component->GetWorld() != World)
			{
				continue;
			}
			if (IRammsCameraProvider* Provider = Cast<IRammsCameraProvider>(Component))
			{
				ExtrinsicSubscriptions.Add(Provider->OnCameraExtrinsicUpdated().AddLambda(
					[this](const FString&, const FTransform&) { Reresolve(); }));
				SubscribedProviders.Add(Component);
			}
		}
	}
}

void URammsDetectionWorldMarkers::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

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

	Super::EndPlay(EndPlayReason);
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
	const UWorld* World = GetWorld();
	if (!World || Source.IsEmpty())
	{
		return false;
	}

	for (TObjectIterator<UActorComponent> It; It; ++It)
	{
		UActorComponent* Component = *It;
		if (!IsValid(Component) || Component->GetWorld() != World)
		{
			continue;
		}
		IRammsCameraProvider* Provider = Cast<IRammsCameraProvider>(Component);
		if (!Provider)
		{
			continue;
		}
		FRammsCameraStreamInfo Info;
		// `source` is the stream the object was detected on, so it names the camera directly.
		if (Provider->GetStreamInfo(Source, Info) && Info.bHasExtrinsic)
		{
			OutExtrinsic = Info.Extrinsic;
			return true;
		}
	}
	return false;
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
				// Nothing sent, so unproject the object's own centroid through its depth stream.
				// Falls back to the box centre, since a producer may send one without the other.
				FVector2D Centroid;
				if (!RammsDetectionMarkers::ReadCentroid(**Object, Centroid))
				{
					continue;
				}
				if (!DeriveWorldLocation(Source, Centroid, Point.WorldLocation))
				{
					continue;
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
				if (!ResolveCameraExtrinsic(Source, Extrinsic))
				{
					// Without the camera's pose a camera-frame point cannot be placed at all.
					// Say so: silently dropping it looks identical to the producer not sending it.
					UE_LOG(LogRammsDetectionMarkers, Warning,
						TEXT("No extrinsic for stream '%s'; cannot place camera-frame position for '%s'"),
						*Source, *(*Object)->GetStringField(TEXT("instance_id")));
					continue;
				}
				Point.WorldLocation = CameraPointToWorld(PointMetres, Extrinsic);
				Point.Source = Source;
			}
			else
			{
				UE_LOG(LogRammsDetectionMarkers, Warning,
					TEXT("Unknown frame '%s' (expected 'camera' or 'world')"), *Frame);
				continue;
			}

			FString InstanceId;
			(*Object)->TryGetStringField(TEXT("instance_id"), InstanceId);
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
					// A heading about the producer's +Z, through the same conversion as a full
					// rotation so the two cannot disagree.
					const FQuat YawOnly(FVector::ZAxisVector, FMath::DegreesToRadians(Yaw));
					Point.WorldRotation = ProducerRotationToWorld(YawOnly, bCameraFrame, Extrinsic);
					Point.bHasRotation = true;
				}
			}

			Point.bSelected = !Selected.IsEmpty() && Selected == InstanceId;
			WorldPoints.Add(MoveTemp(Point));
		}
	}

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
