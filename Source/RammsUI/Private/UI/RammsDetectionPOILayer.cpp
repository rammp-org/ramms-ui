// RammsDetectionPOILayer.cpp

#include "UI/RammsDetectionPOILayer.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Engine/Engine.h"
#include "Interfaces/IRammsCameraProvider.h"
#include "Engine/Texture.h"
#include "EngineUtils.h"
#include "RammsCameraProviderComponent.h"
#include "RammsDetectionWorldMarkers.h"
#include "UI/RammsDetectionPOI.h"

DEFINE_LOG_CATEGORY_STATIC(LogRammsDetectionPOI, Log, All);

URammsDetectionPOILayer::URammsDetectionPOILayer()
{
	// SelfHitTestInvisible, not HitTestInvisible: the layer itself must not swallow touches meant
	// for the view underneath, but its POIs have to receive them, and HitTestInvisible would stop
	// the children too. A designer can still override this per instance.
	Visibility = ESlateVisibility::SelfHitTestInvisible;
}

void URammsDetectionPOILayer::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	POICanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("POICanvas"));
	POICanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = POICanvas;
}

void URammsDetectionPOILayer::ResetCachedWidgets()
{
	// The tree is being rebuilt: the POIs belong to the old canvas
	for (const TPair<FName, TObjectPtr<UUserWidget>>& Pair : POIsById)
	{
		ReleasePOI(Pair.Value);
	}
	for (const TObjectPtr<UUserWidget>& POI : UnkeyedPOIs)
	{
		ReleasePOI(POI);
	}
	POIsById.Reset();
	UnkeyedPOIs.Reset();
	POICanvas = nullptr;
	CachedProvider = nullptr; // another world's provider is not ours
}

UWidget* URammsDetectionPOILayer::GetRootWidgetForValidation()
{
	return POICanvas;
}

void URammsDetectionPOILayer::NativeConstruct()
{
	Super::NativeConstruct();

	// Take the chosen camera's sources, so this layer and that camera agree about whose objects
	// these are without the list being typed twice.
	if (StreamSet != ERammsCameraStreamSet::Custom)
	{
		AcceptedSources = URammsCameraStreamSettings::GetStreamSet(StreamSet).DetectionSources;
	}

	if (StreamSet == ERammsCameraStreamSet::Custom && AcceptedSources.IsEmpty())
	{
		UE_LOG(LogRammsDetectionPOI, Warning,
			TEXT("[%s] accepts every source. With more than one POI layer each shows every camera's ")
				TEXT("objects, placed against its own rectangle -- a detection is normalized to the image ")
					TEXT("it was found in. Set Stream Set to the camera this layer covers, or list AcceptedSources."),
			*GetName());
	}

	if (bReceiveBridgeDetections)
	{
		if (URammsBridgeSubsystem* Bridge = GEngine ? GEngine->GetEngineSubsystem<URammsBridgeSubsystem>() : nullptr)
		{
			Bridge->OnDetectedObjects.AddUniqueDynamic(this, &URammsDetectionPOILayer::HandleDetectedObjects);

			// A push may already have arrived before this layer was constructed, and the next one is
			// on the producer's schedule: show what is current rather than nothing until then.
			SetDetectedObjects(Bridge->GetLastDetectedObjects());
		}
	}
}

void URammsDetectionPOILayer::NativeDestruct()
{
	if (URammsBridgeSubsystem* Bridge = GEngine ? GEngine->GetEngineSubsystem<URammsBridgeSubsystem>() : nullptr)
	{
		Bridge->OnDetectedObjects.RemoveDynamic(this, &URammsDetectionPOILayer::HandleDetectedObjects);
	}

	Super::NativeDestruct();
}

void URammsDetectionPOILayer::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// A POI places itself from its parent's size, so a size change invalidates every placement, and
	// pushes arrive on the producer's cadence. Without this the first placement -- made before the
	// canvas has any geometry, so against (0,0) -- and anything after a resize stay wrong until the
	// next push, or forever if the producer sent once and stopped.
	const FVector2D Size = MyGeometry.GetLocalSize();
	if (!Size.Equals(LastLayoutSize, 0.5))
	{
		LastLayoutSize = Size;
		if (Size.X > 0.0 && Size.Y > 0.0 && LastObjects.Objects.Num() > 0)
		{
			SetDetectedObjects(LastObjects);
			return;
		}
	}

	// World markers come and go on their own schedule: the marker component resolves the same push in
	// its own handler, in no fixed order with this one, and a depth-derived marker can appear a few
	// frames later, once the depth it waited for arrives. So whether an object has a marker is looked at
	// again every tick, not only when a push arrives.
	if (bHideObjectsWithWorldMarkers && LastObjects.Objects.Num() > 0)
	{
		const TSet<FName> Marked = CollectWorldMarkedIds();
		if (Marked.Num() != LastMarkedIds.Num() || !Marked.Includes(LastMarkedIds))
		{
			SetDetectedObjects(LastObjects);
		}
	}
}

TSet<FName> URammsDetectionPOILayer::CollectWorldMarkedIds() const
{
	TSet<FName>	  Ids;
	const UWorld* World = GetWorld();
	for (const TWeakObjectPtr<ARammsDetectionWorldMarker>& Marker : ARammsDetectionWorldMarker::GetLiveMarkers())
	{
		if (Marker.IsValid() && Marker->GetWorld() == World && !Marker->GetDetection().InstanceId.IsNone())
		{
			Ids.Add(Marker->GetDetection().InstanceId);
		}
	}
	return Ids;
}

void URammsDetectionPOILayer::HandleDetectedObjects(const FRammsBridgeDetectedObjects& InObjects)
{
	SetDetectedObjects(InObjects);
}

IRammsCameraProvider* URammsDetectionPOILayer::FindStreamProvider(const FString& StreamID)
{
	if (UObject* Cached = CachedProvider.Get())
	{
		if (IRammsCameraProvider* Provider = Cast<IRammsCameraProvider>(Cached))
		{
			return Provider;
		}
	}

	UWorld* World = GetWorld();
	if (!World || StreamID.IsEmpty())
	{
		return nullptr;
	}

	// Both shapes, as URammsCameraWidget's auto-find does: ARammsCameraProviderBase is an AActor,
	// so a component-only sweep misses every level built on the actor form, and an actor can carry
	// a provider component as well as implementing the interface itself.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<IRammsCameraProvider*, TInlineAllocator<2>> Candidates;
		if (It->GetClass()->ImplementsInterface(URammsCameraProvider::StaticClass()))
		{
			Candidates.Add(Cast<IRammsCameraProvider>(*It));
		}
		if (URammsCameraProviderComponent* Comp = It->FindComponentByClass<URammsCameraProviderComponent>())
		{
			Candidates.Add(Comp);
		}

		for (IRammsCameraProvider* Candidate : Candidates)
		{
			FRammsCameraStreamInfo Info;
			if (Candidate && Candidate->GetStreamInfo(StreamID, Info))
			{
				CachedProvider = Cast<UObject>(Candidate->_getUObject());
				return Candidate;
			}
		}
	}
	return nullptr;
}

FVector2D URammsDetectionPOILayer::ResolvePixelDivisor()
{
	const FVector2D Fallback(
		FMath::Max(FallbackDetectionImageSize.X, 1.0), FMath::Max(FallbackDetectionImageSize.Y, 1.0));

	// Key on the stream THIS LAYER covers, never on the object's "source". They are not the same
	// thing: source is a detection alias, and the shipped scene set accepts "" and "scene" for a
	// stream registered as "camera/scene/color" (RammsCameraStreamSettings.cpp). Asking a provider
	// for a stream called "scene" never matches, which is how an earlier version of this silently
	// used the fallback every time while claiming to use the camera's own size.
	if (StreamSet == ERammsCameraStreamSet::Custom)
	{
		if (!bWarnedAboutCustomStreamSet)
		{
			bWarnedAboutCustomStreamSet = true;
			UE_LOG(LogRammsDetectionPOI, Warning,
				TEXT("[%s] has pixel coordinates to place but Stream Set is Custom, so there is no camera to ")
					TEXT("ask for a frame size: falling back to %s. Set Stream Set to the camera this layer covers."),
				*GetName(), *Fallback.ToString());
		}
		return Fallback;
	}

	const FString		  StreamID = URammsCameraStreamSettings::GetStreamSet(StreamSet).ColorStreamID;
	IRammsCameraProvider* Provider = FindStreamProvider(StreamID);
	if (!Provider)
	{
		// Not remembered: the stream registers on its first frame, and this layer can be built
		// before that, so the next pixel object should try again rather than keep the fallback.
		return Fallback;
	}

	// The live surface, which is what URammsCameraWidget::GetImageSize reads. The registered
	// Width/Height is only captured when the stream id changes, so a producer that switches
	// resolution under one id would leave this layer and the camera widget disagreeing forever.
	if (const UTexture* Texture = Provider->GetStreamTexture(StreamID))
	{
		const FVector2D Live(Texture->GetSurfaceWidth(), Texture->GetSurfaceHeight());
		if (Live.X > 0.0 && Live.Y > 0.0)
		{
			return Live;
		}
	}

	FRammsCameraStreamInfo Info;
	if (Provider->GetStreamInfo(StreamID, Info) && Info.Width > 0 && Info.Height > 0)
	{
		return FVector2D(Info.Width, Info.Height);
	}
	return Fallback;
}

bool URammsDetectionPOILayer::IsAcceptedSource(const FString& Source) const
{
	if (AcceptedSources.IsEmpty())
	{
		return true;
	}
	return AcceptedSources.ContainsByPredicate(
		[&Source](const FString& Candidate) { return Candidate.Equals(Source, ESearchCase::IgnoreCase); });
}

UUserWidget* URammsDetectionPOILayer::CreatePOI()
{
	if (!POIWidgetClass)
	{
		return nullptr;
	}
	if (!POIWidgetClass->ImplementsInterface(URammsDetectionPOI::StaticClass()))
	{
		if (!bWarnedAboutClass)
		{
			bWarnedAboutClass = true;
			UE_LOG(LogRammsDetectionPOI, Warning,
				TEXT("[%s] POIWidgetClass '%s' does not implement Ramms Detection POI, so it would never be ")
					TEXT("given an object: add the interface in its Class Settings. No POIs will be placed."),
				*GetName(), *POIWidgetClass->GetName());
		}
		return nullptr;
	}

	APlayerController* PC = GetOwningPlayer();
	return PC
		? CreateWidget<UUserWidget>(PC, POIWidgetClass)
		: CreateWidget<UUserWidget>(this, POIWidgetClass);
}

void URammsDetectionPOILayer::SetDetectedObjects(const FRammsBridgeDetectedObjects& InObjects)
{
	if (!POICanvas)
	{
		// BuildWidgetTree only builds the canvas when the widget tree is empty, so a Blueprint
		// subclass with anything in its designer never gets one and this layer can never show
		// anything. Silence would cost someone an afternoon.
		if (!bWarnedAboutCanvas)
		{
			bWarnedAboutCanvas = true;
			UE_LOG(LogRammsDetectionPOI, Warning,
				TEXT("[%s] has no POI canvas, so no POI can be placed. Use Ramms Detection POI Layer ")
					TEXT("directly rather than a Blueprint subclass with its own widget tree."),
				*GetName());
		}
		return;
	}

	// A copy: the argument is the subsystem's own member (OnDetectedObjects broadcasts
	// LastDetectedObjects, and NativeConstruct passes GetLastDetectedObjects), and the loop below
	// calls into Blueprint, which could push again and reallocate it underneath us.
	const FRammsBridgeDetectedObjects Objects = InObjects;

	// Objects shown in the world by a marker get no POI (bHideObjectsWithWorldMarkers).
	LastMarkedIds = bHideObjectsWithWorldMarkers ? CollectWorldMarkedIds() : TSet<FName>();

	// A different POI class: start the pool again rather than show the old class beside the new.
	if (POIWidgetClass != PooledClass)
	{
		for (const TPair<FName, TObjectPtr<UUserWidget>>& Pair : POIsById)
		{
			ReleasePOI(Pair.Value);
		}
		for (const TObjectPtr<UUserWidget>& POI : UnkeyedPOIs)
		{
			ReleasePOI(POI);
		}
		POIsById.Reset();
		UnkeyedPOIs.Reset();
		PooledClass = POIWidgetClass;
		bWarnedAboutClass = false; // a new class deserves its own say about the interface
	}

	// FRammsBridgeDetectedObject documents its coordinates as "normalized 0-1, or pixels when any
	// value is > 1", and the two can arrive in ONE push, so the choice is per object, exactly as
	// URammsCameraWidget::GetDetectionCoordinateScale makes it. A POI multiplies by its parent's
	// size, so a pixel centroid of 154.5 lands 154 screens away while a normalized one beside it
	// looks fine -- which reads as a POI bug rather than a coordinate one.
	const auto IsUnit = [](double Value) { return Value >= -0.001 && Value <= 1.001; };

	// Resolved at most once per push, and only if a pixel object actually turns up: while no
	// provider is serving the stream yet the lookup sweeps the world, and doing that per object
	// put a full actor scan into the inner loop.
	bool	  bDivisorResolved = false;
	FVector2D Divisor = FVector2D::ZeroVector;

	TSet<FName>						Seen;
	TArray<TObjectPtr<UUserWidget>> KeptUnkeyed;
	int32							NextUnkeyed = 0;

	for (const FRammsBridgeDetectedObject& Object : Objects.Objects)
	{
		if (!IsAcceptedSource(Object.Source))
		{
			continue;
		}
		// No box and a centroid of exactly (0, 0) is the orchestrator saying it has no position for
		// this object, not an object in the corner -- the same rule the camera widget applies.
		if (!Object.bHasBox && Object.Centroid.X == 0.0 && Object.Centroid.Y == 0.0)
		{
			continue;
		}

		// Normalized when every component is within 0-1; otherwise pixels of the source's frame.
		// The box decides with the centroid, and an object with no box is judged on the centroid
		// alone -- the same test the camera widget applies to the same payload.
		FRammsBridgeDetectedObject Placed = Object;
		const bool				   bNormalized = IsUnit(Object.Centroid.X) && IsUnit(Object.Centroid.Y)
			&& (!Object.bHasBox
				|| (IsUnit(Object.Box.X) && IsUnit(Object.Box.Y) && IsUnit(Object.Box.Z) && IsUnit(Object.Box.W)));
		if (!bNormalized)
		{
			if (!bDivisorResolved)
			{
				bDivisorResolved = true;
				Divisor = ResolvePixelDivisor();
			}
			Placed.Centroid = FVector2D(Object.Centroid.X / Divisor.X, Object.Centroid.Y / Divisor.Y);
			// URammsBridgeSubsystem fills Box whether or not bHasBox, so scaling it only when the
			// flag is set hands Blueprint a struct with a normalized centroid and a pixel box.
			Placed.Box = FVector4(
				Object.Box.X / Divisor.X, Object.Box.Y / Divisor.Y,
				Object.Box.Z / Divisor.X, Object.Box.W / Divisor.Y);
			// Said again whenever the divisor changes: a provider registering after the first push
			// moves every pixel POI, and a line printed once per layer cannot show that.
			if (!Divisor.Equals(LastReportedDivisor, 0.5))
			{
				LastReportedDivisor = Divisor;
				UE_LOG(LogRammsDetectionPOI, Log,
					TEXT("[%s] object '%s' (source '%s') is in pixels (centroid %s); dividing by %s. A wrong ")
						TEXT("divisor puts every such POI in the wrong place, so check it against the real frame."),
					*GetName(), *Object.InstanceId, *Object.Source, *Object.Centroid.ToString(), *Divisor.ToString());
			}
		}

		const FName Id = Object.InstanceId.IsEmpty() ? NAME_None : FName(*Object.InstanceId);
		if (!Id.IsNone() && LastMarkedIds.Contains(Id))
		{
			continue; // its world marker shows it; left out of Seen, so a POI it had is released below
		}
		if (!Id.IsNone() && Seen.Contains(Id))
		{
			// Two objects with one id: keep the first rather than thrash one widget between them.
			if (!bWarnedAboutDuplicateId)
			{
				bWarnedAboutDuplicateId = true;
				UE_LOG(LogRammsDetectionPOI, Warning,
					TEXT("[%s] instance_id '%s' appears more than once in one push; showing the first only."),
					*GetName(), *Object.InstanceId);
			}
			continue;
		}
		UUserWidget* POI = nullptr;
		if (!Id.IsNone())
		{
			if (TObjectPtr<UUserWidget>* Found = POIsById.Find(Id))
			{
				POI = *Found;
			}
		}
		else
		{
			if (!bShowObjectsWithoutIds)
			{
				continue;
			}
			if (NextUnkeyed < UnkeyedPOIs.Num())
			{
				POI = UnkeyedPOIs[NextUnkeyed++];
			}
		}

		if (!POI)
		{
			POI = CreatePOI();
			if (!POI)
			{
				// Creation can fail for reasons that have nothing to do with this object -- a
				// Blueprint class mid-reinstance after a recompile, no local player yet. Skipping
				// one object is recoverable; breaking out would leave every later object missing
				// from Seen and the removal pass below would delete POIs that are still current.
				continue;
			}
			// Added before the object is handed over: the POI places itself through this slot
			POICanvas->AddChildToCanvas(POI);
			POI->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			BindPOI(POI);
		}
		else if (!POI->GetParent())
		{
			// Something took it off the canvas (its own dismiss, a transition). Tracked means shown,
			// so put it back rather than keep feeding a widget nobody can see.
			POICanvas->AddChildToCanvas(POI);
		}

		if (Id.IsNone())
		{
			KeptUnkeyed.Add(POI);
		}
		else
		{
			POIsById.Add(Id, POI);
			Seen.Add(Id);
		}

		// Before the call, so a POI that fires its dispatcher from SetDetectedObject reports this object
		if (TObjectPtr<URammsDetectionPOITapListener>* Listener = TapListeners.Find(POI))
		{
			(*Listener)->Object = Placed;
		}
		IRammsDetectionPOI::Execute_SetDetectedObject(POI, Placed);
	}

	// An object this push leaves out is gone, so its POI goes with it: left on screen it is a ghost,
	// and handed to another object a press on it would tap the wrong thing.
	for (TMap<FName, TObjectPtr<UUserWidget>>::TIterator It = POIsById.CreateIterator(); It; ++It)
	{
		if (!Seen.Contains(It.Key()))
		{
			ReleasePOI(It.Value());
			It.RemoveCurrent();
		}
	}
	for (int32 i = NextUnkeyed; i < UnkeyedPOIs.Num(); ++i)
	{
		ReleasePOI(UnkeyedPOIs[i]);
	}
	UnkeyedPOIs = MoveTemp(KeptUnkeyed);
	LastObjects = Objects;
	ApplySelectionOpacity();

	UE_LOG(LogRammsDetectionPOI, Verbose, TEXT("[%s] stream_id '%s': %d POI(s) from %d object(s)"),
		*GetName(), *Objects.StreamId, GetPOICount(), Objects.Objects.Num());
}

void URammsDetectionPOILayer::ClearPOIs()
{
	SetDetectedObjects(FRammsBridgeDetectedObjects());
}

int32 URammsDetectionPOILayer::GetPOICount() const
{
	return POIsById.Num() + UnkeyedPOIs.Num();
}

void URammsDetectionPOILayer::BindPOI(UUserWidget* POI)
{
	if (!POI || POISelectedDispatcherName.IsNone() || TapListeners.Contains(POI))
	{
		return;
	}

	// Found by name because the dispatcher is a Blueprint variable: C++ has no type to bind it by,
	// and asking every POI to implement something extra would mean editing the widget for a feature
	// it already has. The property is looked up on the POI's own class, so a subclass that renames
	// nothing still works.
	const FMulticastDelegateProperty* Dispatcher =
		FindFProperty<FMulticastDelegateProperty>(POI->GetClass(), POISelectedDispatcherName);
	if (!Dispatcher)
	{
		if (!bWarnedAboutDispatcher)
		{
			bWarnedAboutDispatcher = true;
			UE_LOG(LogRammsDetectionPOI, Warning,
				TEXT("[%s] POI class '%s' has no event dispatcher called '%s', so taps on its POIs are not ")
					TEXT("tracked. Add one, or set POI Selected Dispatcher Name to the one it has (None to stop this)."),
				*GetName(), *POI->GetClass()->GetName(), *POISelectedDispatcherName.ToString());
		}
		return;
	}

	URammsDetectionPOITapListener* Listener = NewObject<URammsDetectionPOITapListener>(this);
	Listener->Layer = this;
	Listener->POI = POI;

	FScriptDelegate Delegate;
	Delegate.BindUFunction(Listener, GET_FUNCTION_NAME_CHECKED(URammsDetectionPOITapListener, HandleSelected));
	Dispatcher->AddDelegate(MoveTemp(Delegate), POI);
	TapListeners.Add(POI, Listener);
}

void URammsDetectionPOILayer::ReleasePOI(UUserWidget* POI)
{
	if (!POI)
	{
		return;
	}

	TObjectPtr<URammsDetectionPOITapListener> Listener;
	if (TapListeners.RemoveAndCopyValue(POI, Listener) && Listener)
	{
		// Unbound rather than left to go stale: a released POI is garbage, but until it is collected
		// a dispatcher it fires (its own dismiss animation, say) would still report a tap.
		if (const FMulticastDelegateProperty* Dispatcher =
				FindFProperty<FMulticastDelegateProperty>(POI->GetClass(), POISelectedDispatcherName))
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(Listener, GET_FUNCTION_NAME_CHECKED(URammsDetectionPOITapListener, HandleSelected));
			Dispatcher->RemoveDelegate(Delegate, POI);
		}
		Listener->Layer = nullptr;
	}

	if (LastTappedPOI == POI)
	{
		LastTappedPOI = nullptr; // the object it showed stays in LastTappedObject
	}
	POI->RemoveFromParent();
}

void URammsDetectionPOILayer::NotifyPOITapped(UUserWidget* POI, const FRammsBridgeDetectedObject& Object)
{
	LastTappedPOI = POI;
	LastTappedObject = Object;
	bHasLastTapped = true;

	UE_LOG(LogRammsDetectionPOI, Log, TEXT("[%s] POI tapped: '%s' (%s, source '%s')"), *GetName(),
		*Object.InstanceId, *Object.PerceptionClass, *Object.Source);
	ApplySelectionOpacity();
	OnPOITapped.Broadcast(POI, Object);
}

bool URammsDetectionPOILayer::GetLastTappedObject(FRammsBridgeDetectedObject& OutObject) const
{
	if (bHasLastTapped)
	{
		OutObject = LastTappedObject;
	}
	return bHasLastTapped;
}

void URammsDetectionPOILayer::ClearLastTapped()
{
	LastTappedPOI = nullptr;
	LastTappedObject = FRammsBridgeDetectedObject();
	bHasLastTapped = false;
	ApplySelectionOpacity();
}

void URammsDetectionPOILayer::ApplySelectionOpacity()
{
	// Matched by instance_id, not by widget: the id is what identifies the object, and a POI with no
	// id cannot be told apart from the next one, so tapping it selects nothing. A tapped object that
	// is no longer on this layer leaves nothing to highlight, so everything goes back to opaque rather
	// than every POI sitting dimmed.
	const FName SelectedId = bHasLastTapped && !LastTappedObject.InstanceId.IsEmpty()
		? FName(*LastTappedObject.InstanceId)
		: NAME_None;
	const bool	bAnySelected = !SelectedId.IsNone() && POIsById.Contains(SelectedId);

	const auto SetAlpha = [](UUserWidget* POI, float Alpha) {
		if (!POI)
		{
			return;
		}
		FLinearColor Color = POI->GetColorAndOpacity();
		if (!FMath::IsNearlyEqual(Color.A, Alpha))
		{
			Color.A = Alpha;
			POI->SetColorAndOpacity(Color);
		}
	};

	const float Dimmed = FMath::Clamp(UnselectedPOIOpacity, 0.0f, 1.0f);
	for (const TPair<FName, TObjectPtr<UUserWidget>>& Pair : POIsById)
	{
		SetAlpha(Pair.Value, !bAnySelected || Pair.Key == SelectedId ? 1.0f : Dimmed);
	}
	for (const TObjectPtr<UUserWidget>& POI : UnkeyedPOIs)
	{
		SetAlpha(POI, bAnySelected ? Dimmed : 1.0f);
	}
}

void URammsDetectionPOITapListener::HandleSelected()
{
	URammsDetectionPOILayer* L = Layer.Get();
	UUserWidget*			 P = POI.Get();
	if (L && P)
	{
		L->NotifyPOITapped(P, Object);
	}
}
