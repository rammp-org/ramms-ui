// RammsDetectionPOILayer.cpp

#include "UI/RammsDetectionPOILayer.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Engine/Engine.h"
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
		if (Pair.Value)
		{
			Pair.Value->RemoveFromParent();
		}
	}
	for (const TObjectPtr<UUserWidget>& POI : UnkeyedPOIs)
	{
		if (POI)
		{
			POI->RemoveFromParent();
		}
	}
	POIsById.Reset();
	UnkeyedPOIs.Reset();
	POICanvas = nullptr;
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
		}
	}
}

void URammsDetectionPOILayer::HandleDetectedObjects(const FRammsBridgeDetectedObjects& InObjects)
{
	SetDetectedObjects(InObjects);
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

	// A different POI class: start the pool again rather than show the old class beside the new.
	if (POIWidgetClass != PooledClass)
	{
		for (const TPair<FName, TObjectPtr<UUserWidget>>& Pair : POIsById)
		{
			if (Pair.Value)
			{
				Pair.Value->RemoveFromParent();
			}
		}
		for (const TObjectPtr<UUserWidget>& POI : UnkeyedPOIs)
		{
			if (POI)
			{
				POI->RemoveFromParent();
			}
		}
		POIsById.Reset();
		UnkeyedPOIs.Reset();
		PooledClass = POIWidgetClass;
		bWarnedAboutClass = false; // a new class deserves its own say about the interface
	}

	// The bridge struct documents its coordinates as "normalized 0-1, or pixels when any value is
	// > 1", and URammsCameraWidget divides the pixel case by the frame size. Nothing here can: the
	// layer has no frame. A POI multiplying pixels by its parent's size lands far off screen, and
	// the boxes over the camera image stay correct, so it reads as a POI-widget bug. Say it once.
	const auto IsUnit = [](double Value) { return Value >= -0.001 && Value <= 1.001; };

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

		if (!bWarnedAboutPixels
			&& (!IsUnit(Object.Centroid.X) || !IsUnit(Object.Centroid.Y)
				|| (Object.bHasBox
					&& (!IsUnit(Object.Box.X) || !IsUnit(Object.Box.Y) || !IsUnit(Object.Box.Z) || !IsUnit(Object.Box.W)))))
		{
			bWarnedAboutPixels = true;
			UE_LOG(LogRammsDetectionPOI, Warning,
				TEXT("[%s] object '%s' on stream '%s' has coordinates outside 0-1 (centroid %s). This layer ")
					TEXT("cannot scale pixels -- it has no frame size -- so its POI will be placed far off. Send ")
						TEXT("normalized 0-1 coordinates."),
				*GetName(), *Object.InstanceId, *Objects.StreamId, *Object.Centroid.ToString());
		}

		const FName Id = Object.InstanceId.IsEmpty() ? NAME_None : FName(*Object.InstanceId);
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

		IRammsDetectionPOI::Execute_SetDetectedObject(POI, Object);
	}

	// An object this push leaves out is gone, so its POI goes with it: left on screen it is a ghost,
	// and handed to another object a press on it would tap the wrong thing.
	for (TMap<FName, TObjectPtr<UUserWidget>>::TIterator It = POIsById.CreateIterator(); It; ++It)
	{
		if (!Seen.Contains(It.Key()))
		{
			if (It.Value())
			{
				It.Value()->RemoveFromParent();
			}
			It.RemoveCurrent();
		}
	}
	for (int32 i = NextUnkeyed; i < UnkeyedPOIs.Num(); ++i)
	{
		if (UnkeyedPOIs[i])
		{
			UnkeyedPOIs[i]->RemoveFromParent();
		}
	}
	UnkeyedPOIs = MoveTemp(KeptUnkeyed);
	LastObjects = Objects;

	UE_LOG(LogRammsDetectionPOI, Verbose, TEXT("[%s] stream_id '%s': %d POI(s) from %d object(s)"),
		*GetName(), *Objects.StreamId, GetPOICount(), Objects.Objects.Num());
}

void URammsDetectionPOILayer::ClearPOIs()
{
	SetDetectedObjects(FRammsBridgeDetectedObjects());
}

void URammsDetectionPOILayer::PostObjectTapped(const FRammsBridgeDetectedObject& DetectedObject)
{
	if (DetectedObject.InstanceId.IsEmpty())
	{
		// The producer sent no id for this object, so there is nothing the bridge could act on.
		UE_LOG(LogRammsDetectionPOI, Verbose, TEXT("Tap on an object with no instance_id; not reported"));
		return;
	}
	if (URammsBridgeSubsystem* Bridge = GEngine ? GEngine->GetEngineSubsystem<URammsBridgeSubsystem>() : nullptr)
	{
		Bridge->PostObjectTapped(DetectedObject.InstanceId, DetectedObject.Source);
	}
}

int32 URammsDetectionPOILayer::GetPOICount() const
{
	return POIsById.Num() + UnkeyedPOIs.Num();
}
