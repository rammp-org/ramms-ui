// RammsDetectionMarkerSync.cpp

#include "UI/RammsDetectionMarkerSync.h"

#include "Components/PanelWidget.h"
#include "RammsBridgeDetectionParse.h"

DEFINE_LOG_CATEGORY_STATIC(LogRammsDetectionMarkerSync, Log, All);

int32 URammsDetectionMarkerSync::SyncDetectionMarkersFromBridge(UPanelWidget* Container, TSubclassOf<URammsDetectionMarker> MarkerClass,
	const FString& DetectedObjectsJson, const TArray<FString>& AcceptedSources, FVector2D ImageSize)
{
	if (!Container || !MarkerClass)
	{
		UE_LOG(LogRammsDetectionMarkerSync, Warning,
			TEXT("SyncDetectionMarkersFromBridge: %s; the markers are left as they were"),
			!Container ? TEXT("no container") : TEXT("no marker class"));
		return -1;
	}

	const FVector2D PixelDivisor(
		ImageSize.X > 0.0 ? ImageSize.X : 640.0,
		ImageSize.Y > 0.0 ? ImageSize.Y : 480.0);

	// Empty list: every camera's objects. A POI layer is not one camera's, so there is no stream of
	// its own to fall back to.
	const auto IsMine = [&AcceptedSources](const FString& Source) {
		return AcceptedSources.IsEmpty()
			|| AcceptedSources.ContainsByPredicate(
				[&Source](const FString& Candidate) { return Candidate.Equals(Source, ESearchCase::IgnoreCase); });
	};

	RammsBridgeDetectionParse::FResult Parsed;

	const int32 Count = RammsBridgeDetectionParse::Parse(DetectedObjectsJson, IsMine, PixelDivisor, Parsed);
	if (Count < 0)
	{
		UE_LOG(LogRammsDetectionMarkerSync, Warning, TEXT("Bridge detections: not a JSON object: %s"), *DetectedObjectsJson.Left(256));
		return -1;
	}

	// The pool is the container's own marker children, so there is no state to keep between pushes
	// and no way for it to disagree with what is on screen.
	TMap<FName, URammsDetectionMarker*> ByID;
	TArray<URammsDetectionMarker*>		Unkeyed;
	TArray<URammsDetectionMarker*>		Stale;
	for (int32 i = 0; i < Container->GetChildrenCount(); ++i)
	{
		URammsDetectionMarker* Marker = Cast<URammsDetectionMarker>(Container->GetChildAt(i));
		if (!Marker)
		{
			continue; // Someone else's widget: not ours to manage
		}
		if (Marker->GetClass() != MarkerClass.Get())
		{
			Stale.Add(Marker); // A different marker class: start again rather than show the old one
			continue;
		}
		const FName ID = Marker->Detection.ID;
		if (ID.IsNone())
		{
			Unkeyed.Add(Marker);
		}
		else if (!ByID.Contains(ID))
		{
			ByID.Add(ID, Marker);
		}
		else
		{
			Stale.Add(Marker); // Two markers for one id: keep the first
		}
	}
	for (URammsDetectionMarker* Marker : Stale)
	{
		Marker->RemoveFromParent();
	}

	const FName Selected = Parsed.SelectedInstanceId.IsEmpty() ? NAME_None : FName(*Parsed.SelectedInstanceId);

	int32 NextUnkeyed = 0;
	for (int32 i = 0; i < Parsed.Boxes.Num(); ++i)
	{
		const FRammsBoundingBox& Box = Parsed.Boxes[i];
		URammsDetectionMarker*	 Marker = nullptr;
		if (!Box.ID.IsNone())
		{
			if (URammsDetectionMarker** Found = ByID.Find(Box.ID))
			{
				Marker = *Found;
				ByID.Remove(Box.ID);
			}
		}
		else if (NextUnkeyed < Unkeyed.Num())
		{
			// Without an id there is nothing to key a marker on, so these are reused in order
			Marker = Unkeyed[NextUnkeyed++];
		}
		if (!Marker)
		{
			APlayerController* PC = Container->GetOwningPlayer();
			Marker = PC
				? CreateWidget<URammsDetectionMarker>(PC, MarkerClass)
				: CreateWidget<URammsDetectionMarker>(Container, MarkerClass);
			if (!Marker)
			{
				break;
			}
			// Before SetDetection: the widget places itself through this slot in OnDetectionSet
			Container->AddChild(Marker);
		}

		Marker->StreamID = Parsed.Sources.IsValidIndex(i) ? Parsed.Sources[i] : Parsed.PushSource;
		Marker->bSelected = !Box.ID.IsNone() && Box.ID == Selected;
		Marker->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Marker->SetDetection(Box);
	}

	// An object the push no longer lists is gone: its marker goes with it, rather than being left on
	// screen or handed to a different object, where a press on it would tap the wrong thing.
	for (const TPair<FName, URammsDetectionMarker*>& Gone : ByID)
	{
		Gone.Value->RemoveFromParent();
	}
	for (int32 i = NextUnkeyed; i < Unkeyed.Num(); ++i)
	{
		Unkeyed[i]->RemoveFromParent();
	}

	UE_LOG(LogRammsDetectionMarkerSync, Verbose, TEXT("Bridge detections (stream_id '%s'): %d marker(s) on %s"),
		*Parsed.PushSource, Count, *Container->GetName());
	return Count;
}
