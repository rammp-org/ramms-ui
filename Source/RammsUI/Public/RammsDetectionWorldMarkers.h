// RammsDetectionWorldMarkers.h
//
// Places bridge detections in the world, for producers that know where an object IS rather than
// only where it appears.
//
// UpdateDetectedObjects already carries 2D boxes and centroids, which URammsCameraWidget draws over
// the camera image. That stays as it is. This component reads the same push and puts a marker in
// the scene, from whichever of two sources is available:
//
//   1. DERIVED. The object's centroid is unprojected through its camera's intrinsics and the range
//      is read from that camera's depth stream. Needs nothing from the producer beyond the 2D
//      fields it already sends, so it works today. Accurate to what the depth camera saw.
//
//   2. SENT. The producer states the position itself, and optionally the object's size. Better,
//      because perception knows what it measured and a size cannot be derived from one depth
//      sample at all -- a fridge is a box, not a point. Needs the fields below to exist on the
//      wire, which is an agreement with the bridge, not a decision this file can make.
//
// A sent position wins over a derived one. An object with neither is ignored: it is the 2D-only
// case and belongs to the camera widget.
//
//   "position_m": {"X": 0.12, "Y": -0.04, "Z": 1.83},   // METRES, not Unreal units
//   "frame": "camera",                                    // or "world"; defaults to "camera"
//   "size_m": {"X": 0.60, "Y": 0.70, "Z": 1.80},          // FULL dimensions, not half-extents
//   "orientation": {"X": 0, "Y": 0, "Z": 0.707, "W": 0.707},   // optional; absent = axis-aligned
//   "yaw_deg": 90.0                                       // shorthand, ignored if orientation is sent
//
// size_m is what makes a fridge, a microwave or a table readable as the object it is rather than a
// dot floating at its centre, and it is the producer's own understanding of the object's size and
// pose -- which is the thing worth seeing when the two disagree. FULL dimensions because half-vs-
// full is the silent factor-of-two every such field eventually suffers; the key says which.
//
// A QUATERNION rather than roll/pitch/yaw because detectors that give oriented 3D boxes (BOXER and
// its like) give a full rotation, and almost nothing in a kitchen is axis-aligned anyway -- a
// tilted bottle, a chair pushed in at an angle, a table on a ramp. Euler angles would need the
// order and handedness agreed as well as the values, and that convention gets lost: FRotator takes
// (Pitch, Yaw, Roll) while every Python caller writes (roll, pitch, yaw). yaw_deg stays for a
// producer that genuinely only knows a heading.
//
// Both frames are right-handed and Unreal is left-handed, so an orientation is converted by
// conjugating it with the same basis change that moves the position -- B * R * B^T -- rather than
// by negating quaternion components by hand. The matrix form is mechanical and survives the
// handedness flip; the component-flip form is where a mirrored or doubly-rotated box comes from.
//
// Metres because every producer of these is a robot, and the unit is in the key so a 100x error
// has to be deliberate. Unreal units are centimetres, so values are scaled on the way in.
//
// "camera" is the pinhole optical convention (X right, Y down, Z forward) relative to the camera
// the object's `source` names, and is resolved through that stream's extrinsic. It is the mapping
// Shaders/Private/RammsProjection.ush documents for the projector, inverted -- the two have to
// agree or a marker will not sit on the pixels it was detected in.
//
// "world" is the producer's world frame, right-handed Z-up (ROS): X forward, Y left, Z up. Unreal
// is left-handed, so Y is negated. No extrinsic is needed, and the object's `source` is ignored.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "RammsDetectionWorldMarkers.generated.h"

class URammsBridgeSubsystem;

/** One detection resolved to a world position. */
USTRUCT(BlueprintType)
struct RAMMSUI_API FRammsDetectionWorldPoint
{
	GENERATED_BODY()

	/** The producer's id for this object, stable across pushes. */
	UPROPERTY(BlueprintReadOnly, Category = "RAMMS|Detection")
	FName InstanceId;

	/** Free-text class ("cup", "food"); opaque here. */
	UPROPERTY(BlueprintReadOnly, Category = "RAMMS|Detection")
	FString PerceptionClass;

	UPROPERTY(BlueprintReadOnly, Category = "RAMMS|Detection")
	FVector WorldLocation = FVector::ZeroVector;

	/** True when the push named this one as selected_instance_id. */
	UPROPERTY(BlueprintReadOnly, Category = "RAMMS|Detection")
	bool bSelected = false;

	/** The object's `source`, i.e. which camera reported it. Empty for world-frame positions. */
	UPROPERTY(BlueprintReadOnly, Category = "RAMMS|Detection")
	FString Source;

	/** Full size in Unreal units, when the producer sent one. Zero when it did not. */
	UPROPERTY(BlueprintReadOnly, Category = "RAMMS|Detection")
	FVector WorldSize = FVector::ZeroVector;

	/** True when WorldSize is the producer's, so a marker can draw a box instead of a point. */
	UPROPERTY(BlueprintReadOnly, Category = "RAMMS|Detection")
	bool bHasSize = false;

	/** The producer's orientation for the box, in Unreal's frame. Identity when none was sent. */
	UPROPERTY(BlueprintReadOnly, Category = "RAMMS|Detection")
	FRotator WorldRotation = FRotator::ZeroRotator;

	/** True when WorldRotation is the producer's rather than an assumed axis alignment. */
	UPROPERTY(BlueprintReadOnly, Category = "RAMMS|Detection")
	bool bHasRotation = false;

	/**
	 * False when this position was derived here from the depth stream, true when the producer sent
	 * it. Worth showing while the two are being brought into agreement: a derived point that sits
	 * somewhere a sent one does not is the interesting case, and nothing else distinguishes them.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RAMMS|Detection")
	bool bPositionFromProducer = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnRammsDetectionWorldPoints, const TArray<FRammsDetectionWorldPoint>&, Points);

/**
 * Resolves the 3D positions in a detections push and, optionally, keeps an actor at each one.
 *
 * Place on any actor in the level. Objects with no `position_m` are ignored entirely -- they are
 * the 2D-only case and belong to the camera widget.
 */
UCLASS(ClassGroup = (RAMMS), meta = (BlueprintSpawnableComponent))
class RAMMSUI_API URammsDetectionWorldMarkers : public UActorComponent
{
	GENERATED_BODY()

public:
	URammsDetectionWorldMarkers();

	/** Spawned once per object with a 3D position, and destroyed when it stops being reported. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	TSubclassOf<AActor> MarkerClass;

	/**
	 * Keep an actor at each position. Leave this off to drive your own visuals from
	 * OnDetectionWorldPoints -- the positions are resolved either way.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	bool bSpawnMarkers = true;

	/**
	 * Unproject centroids through the depth stream for objects the producer sent no position for.
	 * Turn off to show only what the producer states, which is how you see whether it is sending
	 * anything at all.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	bool bDeriveFromDepth = true;

	/**
	 * Half-width in depth pixels of the window sampled around a centroid; the median of the valid
	 * samples is used. One pixel of a real depth image is readily a dropout or an edge between the
	 * object and whatever is behind it, and the median of a small window costs nothing and does not
	 * take a hole for a surface. 0 samples the single pixel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection", meta = (ClampMin = "0", UIMin = "0", UIMax = "8"))
	int32 DepthSampleRadius = 2;

	/**
	 * Draw each resolved detection with the debug renderer: an oriented box where a size was sent,
	 * a cross where only a point is known, and the class name beside it.
	 *
	 * Colour says where the position came from, which is the thing being validated while the two
	 * sources are brought into agreement -- green for a position the producer sent, cyan for one
	 * derived here from depth, yellow for the selected object. A derived marker sitting somewhere a
	 * sent one does not is the case worth seeing, and nothing else distinguishes them on screen.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	bool bDrawDebugMarkers = true;

	/**
	 * Only accept objects whose `source` is listed; empty accepts every camera. Mirrors
	 * URammsCameraWidget::BridgeDetectionSources, so one camera's markers can be separated from
	 * another's.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	TArray<FString> AcceptedSources;

	/** Fires on every push, with the objects that carried a position. Empty is a valid result. */
	UPROPERTY(BlueprintAssignable, Category = "RAMMS|Detection")
	FOnRammsDetectionWorldPoints OnDetectionWorldPoints;

	/**
	 * Resolve a detections payload directly, instead of waiting for the bridge to push one.
	 *
	 * The same entry the subscription uses, so what it produces is what a real push produces.
	 * Mirrors URammsDetectionPOILayer::SetDetectedObjects: useful for replaying a recorded payload,
	 * for driving the markers from a Blueprint with no bridge attached, and for testing this
	 * component without a producer.
	 */
	UFUNCTION(BlueprintCallable, Category = "RAMMS|Detection")
	void SetDetectedObjectsJson(const FString& DetectedObjectsJson);

	/**
	 * Resolve one normalized image point against a camera's depth stream, as the derived path does
	 * per object. Exposed because it is the half that depends on live stream data, so it is the
	 * half worth checking against a camera that is actually running.
	 */
	UFUNCTION(BlueprintCallable, Category = "RAMMS|Detection")
	bool DeriveWorldLocation(const FString& Source, const FVector2D& NormalizedPoint, FVector& OutWorldLocation) const;

	/** The most recent resolved set, for anything that starts after a push. */
	UFUNCTION(BlueprintPure, Category = "RAMMS|Detection")
	const TArray<FRammsDetectionWorldPoint>& GetWorldPoints() const { return WorldPoints; }

	/**
	 * Camera-frame point (metres, pinhole optical: X right, Y down, Z forward) to world.
	 *
	 * Exposed because it is the half worth testing and the half worth reusing: anything else
	 * holding a camera-frame point and that camera's extrinsic can place it the same way.
	 */
	UFUNCTION(BlueprintPure, Category = "RAMMS|Detection")
	static FVector CameraPointToWorld(const FVector& PointMetres, const FTransform& CameraExtrinsic);

	/** Producer world frame (metres, right-handed Z-up) to Unreal world. */
	UFUNCTION(BlueprintPure, Category = "RAMMS|Detection")
	static FVector ProducerWorldPointToWorld(const FVector& PointMetres);

	/**
	 * A normalized image point plus a range to the camera-local point it came from, in Unreal
	 * units.
	 *
	 * This is RammsProjection.ush line 94 solved for the point instead of the pixel:
	 *   u = fx * CamX / Depth + cx   ->   CamX = (u - cx) * Depth / fx
	 * so it is the projector's own model run backwards, and a marker placed with it lands on the
	 * pixels the object was detected in. Depth is along the optical axis, not a radial distance,
	 * which is the shader's convention (Depth = CameraLocalPos.x) and is the one thing here that a
	 * depth producer could reasonably disagree about; a radial source makes markers splay outward
	 * toward the frame edges while looking correct in the centre.
	 *
	 * Intrinsics are (fx, fy, cx, cy) in pixels of an image ImageSize wide, so a depth stream of a
	 * different resolution is fine: the normalized point carries across.
	 */
	/**
	 * A producer rotation (quaternion in its own right-handed frame) to Unreal's frame.
	 *
	 * bCameraFrame picks which basis the rotation is expressed in: the optical frame of the camera
	 * the object was seen by, or the producer's world frame. Pass the camera's extrinsic for the
	 * former so the result is a world rotation either way.
	 */
	UFUNCTION(BlueprintPure, Category = "RAMMS|Detection")
	static FRotator ProducerRotationToWorld(const FQuat& ProducerRotation, bool bCameraFrame, const FTransform& CameraExtrinsic);

	UFUNCTION(BlueprintPure, Category = "RAMMS|Detection")
	static FVector UnprojectToCameraLocal(const FVector2D& NormalizedPoint, float DepthUU, const FVector4& Intrinsics, const FVector2D& ImageSize);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleDetectedObjectsJson(const FString& DetectedObjectsJson);

	/** Re-run the last push: a camera that moved invalidates every camera-frame position from it. */
	void Reresolve();

	bool ResolveCameraExtrinsic(const FString& Source, FTransform& OutExtrinsic) const;

	/** The depth stream sharing a GroupID with this colour stream, and the numbers to read it. */
	bool FindDepthSibling(const FString& ColourStreamID, FString& OutDepthStreamID, TScriptInterface<class IRammsCameraProvider>& OutProvider) const;
	bool IsAcceptedSource(const FString& Source) const;
	void ApplyMarkers();

	/** Kept so an extrinsic update can be re-applied without waiting for the next push. */
	FString LastPayload;

	UPROPERTY(Transient)
	TArray<FRammsDetectionWorldPoint> WorldPoints;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<AActor>> Markers;

	TArray<FDelegateHandle> ExtrinsicSubscriptions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> SubscribedProviders;
};
