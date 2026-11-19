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
//   "position_m": {"X": 0.12, "Y": -0.04, "Z": 1.83},   // the object's CENTRE, METRES
//   "frame": "camera",                                    // or "world"; defaults to "camera"
//   "size_m": {"X": 0.60, "Y": 0.70, "Z": 1.80},          // FULL dimensions, not half-extents
//   "orientation": {"X": 0, "Y": 0, "Z": 0.707, "W": 0.707},   // optional; absent = axis-aligned
//   "yaw_deg": 90.0                                       // shorthand, ignored if orientation is sent
//   "time_since_detection_s": 12.5                        // 0 = being seen now; absent = unknown
//
// time_since_detection_s is how long since the robot last saw the object DIRECTLY -- agreed with
// the bridge owner, not inferred here. A tracker carrying the object forward does NOT reset it: a
// prediction is not a sighting, and if it counted as one the field would measure how long the
// tracker has been running rather than how stale its answer is, which is the opposite of what it
// is for. A tracked object keeps being reported after it leaves the frame or goes behind
// something, and without this field a marker drawn from a forty-second-old memory is
// indistinguishable from one the robot is looking at -- the operator is shown a claim with no way
// to judge how much to trust it. Markers dim as it grows, and say the number once they do.
//
// position_m is the CENTRE of the object, not a corner of its box: the marker is placed there and
// the box drawn around it runs position_m +/- size_m / 2. A corner would put the whole box off by
// half its size on every axis -- which looks like a calibration error, not a convention one, and
// so is the kind of disagreement that gets chased in the wrong place for a week. Centre-vs-corner
// is the same silent ambiguity as half-vs-full on size_m, and gets the same explicit answer.
//
// size_m is what makes a fridge, a microwave or a table readable as the object it is rather than a
// dot floating at its centre, and it is the producer's own understanding of the object's size and
// pose -- which is the thing worth seeing when the two disagree. FULL dimensions because half-vs-
// full is the silent factor-of-two every such field eventually suffers; the key says which.
//
// NOT YET RECONCILED: a DERIVED position is not a centre. It is the object's centroid PIXEL
// unprojected at the depth sampled there, which lands on the surface facing the camera -- half an
// object nearer than its centre. The two agree today only because a derived position comes with no
// size, so it draws a small cube where the difference does not show; but size_m is read whether or
// not position_m was sent, so a producer sending a size WITHOUT a position would get a box centred
// on the front face and sunk half its depth toward the camera. Nothing sends that combination yet.
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
#include "GameFramework/Actor.h"
#include "RammsWorldTappable.h"
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

	/**
	 * Where the object is, in Unreal units. The CENTRE of it when the producer sent position_m --
	 * the marker's box is centred here and runs +/- WorldSize / 2. A derived position is instead a
	 * point on the surface facing the camera; see the note at the top of this file.
	 */
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

	/** Seconds since the robot last saw this object directly. Zero means it is being seen now. */
	UPROPERTY(BlueprintReadOnly, Category = "RAMMS|Detection")
	float SecondsSinceDetection = 0.0f;

	/** True when the producer sent time_since_detection_s. Absent is not zero: one says nothing
	 *  about when the object was seen, the other says it is being seen right now. */
	UPROPERTY(BlueprintReadOnly, Category = "RAMMS|Detection")
	bool bHasSecondsSinceDetection = false;

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
 * One detection, standing in the world where it was resolved, and tappable there.
 *
 * Tappable is the whole reason this is an actor with a collision box rather than a debug box. A
 * drawn box is a picture: it has no hit testing and can never take a tap, which is exactly how the
 * camera overlay's rectangles turned out to be untappable. An operator pointing at the object in
 * the scene has to be pointing at something the engine can trace against.
 *
 * It reports a tap the same way URammsDetectionMarker does -- instance_id with the stream it came
 * from -- so the orchestrator cannot tell which surface the operator used, and should not have to.
 *
 * Taps arrive through IRammsWorldTappable, which URammsWorldTapLayer calls after tracing the view.
 * The engine's own OnClicked and OnInputTouchBegin are bound too and call the same event, but they
 * depend on APlayerController seeing the mouse button, and a full-screen hit-testable widget over
 * the view stops that -- which is exactly what the HUD's background button does. The tap layer is
 * the route that works; those two are a bonus where input is routed normally.
 */
UCLASS(ClassGroup = (RAMMS))
class RAMMSUI_API ARammsDetectionWorldMarker : public AActor, public IRammsWorldTappable
{
	GENERATED_BODY()

public:
	ARammsDetectionWorldMarker();

	/** Place and size this marker, and remember what it stands for. */
	UFUNCTION(BlueprintCallable, Category = "RAMMS|Detection")
	void SetDetection(const FRammsDetectionWorldPoint& Point);

	UFUNCTION(BlueprintPure, Category = "RAMMS|Detection")
	const FRammsDetectionWorldPoint& GetDetection() const { return Detection; }

	/** Tell the robot this object was tapped. Public so a Blueprint marker can report the same way. */
	UFUNCTION(BlueprintCallable, Category = "RAMMS|Detection")
	void ReportTapped() const;

	/** The box a tap has to hit. Sized from the detection, or DefaultExtent when no size was sent. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RAMMS|Detection")
	TObjectPtr<class UBoxComponent> Bounds;

	/**
	 * The channel this marker answers queries on, and the only one it answers at all. It must be
	 * the channel URammsWorldTapLayer traces, or taps find nothing.
	 *
	 * It OVERLAPS that channel rather than blocking it, which is the important part. Visibility is
	 * not just the click channel in this project: RammsCore's ToF and sonar components line-trace
	 * on it (their TraceChannel defaults to ECC_Visibility) whenever the GPU path is off or
	 * unavailable, and the curb controllers ground-probe on it. A marker is bHiddenInGame, so a
	 * blocking one would be an INVISIBLE wall that shortens a sonar range or raises a measured
	 * curb -- a sensor fault with no visible cause, arriving only when a detection happens to land
	 * in front of the robot. An overlap response never stops a single-trace, so the sensors cannot
	 * see it, while the layer's multi-trace still collects it.
	 *
	 * The cost is that APlayerController's click trace is a single-trace and so cannot see it
	 * either: the engine's OnClicked route is dormant unless a project changes this to block.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RAMMS|Detection")
	TEnumAsByte<ECollisionChannel> TapChannel = ECC_Visibility;

	/** Block that channel instead of overlapping it. Read the warning on TapChannel first. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RAMMS|Detection")
	bool bBlockTapChannel = false;

	/**
	 * Half-extent used when the producer sent no size. Big enough to be worth aiming at: a point
	 * detection still has to be tappable, and a cup's centroid is not a target a finger can hit.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RAMMS|Detection")
	FVector DefaultExtent = FVector(6.0, 6.0, 6.0);

	/**
	 * Draw this marker: a translucent filled box with its outline over the top, coloured by where
	 * its position came from.
	 *
	 * On by default because the collision box renders nothing: a marker that can be tapped and not
	 * seen is not a marker, and an operator cannot aim at it. Turn it off in a Blueprint subclass
	 * that brings its own mesh, which is what should eventually replace this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	bool bDrawMarker = true;

	/** The producer stated this object's position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection|Appearance")
	FColor SentColour = FColor(72, 220, 120);

	/** The position was derived here from the depth stream. Distinct from sent on purpose: which of
	 * the two a marker came from is the interesting thing while they are being brought to agree. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection|Appearance")
	FColor DerivedColour = FColor(72, 200, 235);

	/** The push named this one as selected_instance_id. Overrides both colours above. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection|Appearance")
	FColor SelectedColour = FColor(255, 206, 64);

	/**
	 * Fill opacity of the selected marker, and of every other one.
	 *
	 * Filled rather than wireframe because a wireframe box over a busy scene reads as a few stray
	 * lines, not as an object with a front and a back. Low for the rest, because a room's worth of
	 * solid boxes hides the room; the gap between the two is what makes the selection findable at a
	 * glance without hunting for a thicker line.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection|Appearance",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SelectedFillOpacity = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection|Appearance",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FillOpacity = 0.18f;

	/** Outline thickness, selected and not. Zero takes the renderer's thinnest line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection|Appearance",
		meta = (ClampMin = "0.0"))
	float SelectedLineThickness = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection|Appearance",
		meta = (ClampMin = "0.0"))
	float LineThickness = 1.5f;

	/**
	 * How much of its colour an unselected marker keeps, 0-1. Dims the colour as well as the fill:
	 * opacity decides how much of the scene shows through a box, not how loud it is, and a
	 * saturated green still shouts at 0.18 against a pale room. Dimming is what makes the
	 * unselected ones recede far enough for the selected one to be findable without looking.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection|Appearance",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UnselectedBrightness = 0.72f;

	/**
	 * Dim a marker as the time since the robot last saw the object grows.
	 *
	 * Inert unless the producer sends time_since_detection_s, so turning it on costs nothing until
	 * there is something to show. The selected marker dims too -- an old selection is exactly the
	 * one worth doubting -- but keeps its colour and thicker outline, so it stays findable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection|Appearance")
	bool bDimWhenStale = true;

	/** Age at which dimming starts. Below this a marker is drawn as a live detection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection|Appearance",
		meta = (ClampMin = "0.0", EditCondition = "bDimWhenStale"))
	float StaleAfterSeconds = 3.0f;

	/** Age at which dimming reaches StaleBrightness and stops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection|Appearance",
		meta = (ClampMin = "0.0", EditCondition = "bDimWhenStale"))
	float FullyStaleAfterSeconds = 60.0f;

	/**
	 * What a fully stale marker keeps of its colour and fill, 0-1. Not zero: an object the robot
	 * has not seen for a minute is still the best guess anyone has about where it is, and a marker
	 * that fades to nothing is one the operator cannot tap to ask about.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection|Appearance",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bDimWhenStale"))
	float StaleBrightness = 0.35f;

	/** Put the age on the label once a marker is stale, so "dim" reads as a number not a mood. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection|Appearance")
	bool bShowAgeWhenStale = true;

	/**
	 * Draw the label as plain debug text beside the box.
	 *
	 * OFF, because URammsDetectionWorldLabels draws it properly: the 2D POIs' pill, in screen
	 * space, at a size meant to be read. This is the fallback for a world with no RammsUI HUD in
	 * it, and it is the old unreadable version -- bare text whose colour happens to be the colour
	 * of the scene behind it often enough.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection|Appearance")
	bool bDrawDebugLabel = false;

	/**
	 * Seconds since the robot last saw this object, or a negative number when the producer never
	 * said. It is the age the producer sent PLUS the time since that push arrived, so a marker
	 * goes on ageing when the pushes stop -- which is the honest reading, since losing the
	 * producer is also not seeing the object.
	 */
	UFUNCTION(BlueprintPure, Category = "RAMMS|Detection")
	float GetSecondsSinceDetection() const;

	/**
	 * The name that belongs beside this marker: its perception class, or its id when it has no
	 * class.
	 *
	 * On the marker rather than in the widget that draws it, because the marker owns the detection
	 * -- two places deciding what a label says is two places to change when it changes. Kept apart
	 * from GetAgeText so a caller can upper-case the name without turning "120s" into "120S".
	 */
	UFUNCTION(BlueprintPure, Category = "RAMMS|Detection")
	FString GetLabelText() const;

	/** The age to show beside the name, or empty while this marker is still fresh. */
	UFUNCTION(BlueprintPure, Category = "RAMMS|Detection")
	FString GetAgeText() const;

	/**
	 * The colour its label is drawn in: the marker's own colour, NOT dimmed by staleness.
	 *
	 * The box fades and the label states the age in words; dimming the text as well would make the
	 * oldest markers -- the ones whose age you most want to read -- the hardest to read, which is
	 * the whole reason the label needed a plate behind it.
	 */
	UFUNCTION(BlueprintPure, Category = "RAMMS|Detection")
	FLinearColor GetLabelColour() const;

	/** Where its label belongs, in world space: just above the box. */
	UFUNCTION(BlueprintPure, Category = "RAMMS|Detection")
	FVector GetLabelAnchorLocation() const;

	/** 0 fresh, 1 fully stale. Zero when the producer sends no age or dimming is off. */
	UFUNCTION(BlueprintPure, Category = "RAMMS|Detection")
	float GetStaleness() const;

	/** IRammsWorldTappable: the operator tapped this marker in the 3D view. */
	virtual void OnRammsWorldTapped_Implementation(const FHitResult& Hit) override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Every marker currently in play, in any world. Filter by GetWorld() at the call site.
	 *
	 * A registry rather than TActorIterator: URammsDetectionWorldLabels needs this list in
	 * NativePaint, which can run more than once a frame, and an actor iteration walks every actor
	 * in every loaded level -- tens of thousands of them on a generated street map, to find the
	 * handful of markers. Entries can be stale; the weak pointers say which.
	 */
	static const TArray<TWeakObjectPtr<ARammsDetectionWorldMarker>>& GetLiveMarkers() { return LiveMarkers; }

protected:
	/** Put TapChannel / bBlockTapChannel on the box. Here rather than in the constructor because a
	 *  Blueprint subclass's defaults are not applied until after it. */
	void ApplyCollisionResponse();

	UFUNCTION()
	void HandleClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed);

	UFUNCTION()
	void HandleTouchBegin(ETouchIndex::Type FingerIndex, UPrimitiveComponent* TouchedComponent);

	/** Colour before any dimming: selected, or by where the position came from. */
	FColor GetBaseColour() const;

private:
	static TArray<TWeakObjectPtr<ARammsDetectionWorldMarker>> LiveMarkers;

	FRammsDetectionWorldPoint Detection;

	/** World time the current Detection arrived, so its age can keep running between pushes. */
	double DetectionReceivedTime = 0.0;
};

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

	/**
	 * Spawned once per object with a 3D position, and destroyed when it stops being reported.
	 * Defaults to ARammsDetectionWorldMarker, which is tappable; a Blueprint subclass of it keeps
	 * that and can look like anything.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	TSubclassOf<AActor> MarkerClass;

	// Taps are URammsWorldTapLayer's job, not this component's: it traces the view from Slate and
	// calls IRammsWorldTappable. This used to switch on APlayerController's click and touch events
	// on every push -- global state changed as a side effect of data arriving, never restored, and
	// pointless now that markers overlap the click channel rather than block it.

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
	 *
	 * Off by default because a spawned ARammsDetectionWorldMarker draws itself the same way, and
	 * both at once double every line. This is for running with bSpawnMarkers off, where the
	 * positions are resolved for something else to use and there is no actor to do the drawing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	bool bDrawDebugMarkers = false;

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

	/** Destroy every pooled marker and forget them. */
	void DestroyMarkers();

	/**
	 * Visit every camera provider in this world until Visit returns false.
	 *
	 * BOTH kinds: ARammsCameraProviderBase is an actor implementing IRammsCameraProvider and is a
	 * shipped extension point (RemoteControl, Socket.IO, ROS 2 implementations are named in the
	 * interface's own documentation), while URammsCameraProviderComponent is a component.
	 * URammsCameraProjectionManager::DiscoverProviders deliberately does both passes; a sweep of
	 * components alone leaves a project whose camera is an actor with no world markers at all and
	 * nothing but "no extrinsic for stream" in the log to say why.
	 */
	void ForEachCameraProvider(TFunctionRef<bool(UObject*, IRammsCameraProvider*)> Visit) const;

	/**
	 * The provider that owns a stream, remembered between lookups.
	 *
	 * Without it every object of every push walks the whole object table twice, and the extrinsic
	 * subscription below fires on every frame that carries camera metadata -- three streams at
	 * 30 fps with fifteen detections is thousands of full scans a second, on the game thread.
	 */
	UObject* FindProviderForStream(const FString& StreamID) const;

	mutable TMap<FString, TWeakObjectPtr<UObject>> ProviderByStream;

	/** Sources the last resolve actually used, so an unrelated camera moving costs nothing. */
	TSet<FString> UsedSources;

	/** A camera moved: re-resolve once on the next tick rather than once per broadcast. */
	bool bReresolvePending = false;

	/** Streams already complained about, so a camera with no pose does not fill the log. */
	mutable TSet<FString> WarnedStreams;

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
