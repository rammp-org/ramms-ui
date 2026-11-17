// RammsDetectionPOILayer.cpp

#include "UI/RammsDetectionPOILayer.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
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
		return;
	}

	TSet<FName>						Seen;
	TArray<TObjectPtr<UUserWidget>> KeptUnkeyed;
	int32							NextUnkeyed = 0;

	for (const FRammsBridgeDetectedObject& Object : InObjects.Objects)
	{
		if (!IsAcceptedSource(Object.Source))
		{
			continue;
		}
		// No box and a centroid of exactly (0, 0) is the orchestrator saying it has no position for
		// this object, not an object in the corner -- the same rule the camera widget applies.
		if (!Object.bHasBox && Object.Centroid.IsNearlyZero())
		{
			continue;
		}

		const FName	 Id = Object.InstanceId.IsEmpty() ? NAME_None : FName(*Object.InstanceId);
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
				break; // No usable class: nothing to place, and CreatePOI has said why
			}
			// Added before the object is handed over: the POI places itself through this slot
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

		POI->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
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

	UE_LOG(LogRammsDetectionPOI, Verbose, TEXT("[%s] stream_id '%s': %d POI(s) from %d object(s)"),
		*GetName(), *InObjects.StreamId, GetPOICount(), InObjects.Objects.Num());
}

void URammsDetectionPOILayer::ClearPOIs()
{
	SetDetectedObjects(FRammsBridgeDetectedObjects());
}

int32 URammsDetectionPOILayer::GetPOICount() const
{
	return POIsById.Num() + UnkeyedPOIs.Num();
}
