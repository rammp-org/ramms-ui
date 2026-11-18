// RammsBridgeSubsystem.cpp

#include "RammsBridgeSubsystem.h"

#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

namespace
{
	FString ToCondensedJson(const TSharedRef<FJsonObject>& Object)
	{
		FString															 Out;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Object, Writer);
		return Out;
	}
} // namespace

void URammsBridgeSubsystem::Deinitialize()
{
	OnPanelJson.Clear();
	OnDetectedObjectsJson.Clear();
	OnDetectedObjects.Clear();
	OnCameraFeedRequested.Clear();
	OnUIEventsJsonChanged.Clear();
	RecentEvents.Empty();
	Super::Deinitialize();
}

// â”€â”€ Inbound â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

bool URammsBridgeSubsystem::PanelJsonHasProgress(const FString& PanelJson)
{
	TSharedPtr<FJsonObject>			Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PanelJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}
	const TSharedPtr<FJsonObject>* Progress = nullptr;
	return Root->TryGetObjectField(TEXT("progress"), Progress) && Progress && Progress->IsValid();
}

void URammsBridgeSubsystem::BroadcastPanelJson(const FString& PanelJson)
{
	OnPanelJson.Broadcast(PanelJson);
}

void URammsBridgeSubsystem::BroadcastDetectedObjectsJson(const FString& DetectedObjectsJson)
{
	OnDetectedObjectsJson.Broadcast(DetectedObjectsJson);

	FRammsBridgeDetectedObjects Parsed;
	if (ParseDetectedObjectsJson(DetectedObjectsJson, Parsed))
	{
		LastDetectedObjects = Parsed;
		OnDetectedObjects.Broadcast(LastDetectedObjects);
	}
}

namespace
{
	/** Reads Count numbers from Field: a bare array, or an object with Keys ({"X": .., "Y": ..}). */
	bool ReadBridgeNumbers(const FJsonObject& Object, const TCHAR* Field, int32 Count, double* Out)
	{
		static const TCHAR* const	 Keys[] = { TEXT("X"), TEXT("Y"), TEXT("Z"), TEXT("W") };
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
		if (Value->TryGetObject(Fields))
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
} // namespace

bool URammsBridgeSubsystem::ParseDetectedObjectsJson(const FString& DetectedObjectsJson, FRammsBridgeDetectedObjects& OutObjects)
{
	OutObjects = FRammsBridgeDetectedObjects();

	TSharedPtr<FJsonObject>			Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DetectedObjectsJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	Root->TryGetStringField(TEXT("stream_id"), OutObjects.StreamId);
	Root->TryGetStringField(TEXT("selected_instance_id"), OutObjects.SelectedInstanceId);

	const TArray<TSharedPtr<FJsonValue>>* Objects = nullptr;
	if (!Root->TryGetArrayField(TEXT("objects"), Objects))
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Objects)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object))
		{
			continue;
		}

		FRammsBridgeDetectedObject& Out = OutObjects.Objects.AddDefaulted_GetRef();
		(*Object)->TryGetStringField(TEXT("instance_id"), Out.InstanceId);
		(*Object)->TryGetStringField(TEXT("perception_class"), Out.PerceptionClass);
		if (!(*Object)->TryGetStringField(TEXT("source"), Out.Source) || Out.Source.IsEmpty())
		{
			Out.Source = OutObjects.StreamId;
		}

		double BoxValues[4] = { 0.0, 0.0, 0.0, 0.0 };
		Out.bHasBox = ReadBridgeNumbers(**Object, TEXT("box"), 4, BoxValues)
			&& (BoxValues[0] != BoxValues[2] || BoxValues[1] != BoxValues[3]);
		Out.Box = FVector4(BoxValues[0], BoxValues[1], BoxValues[2], BoxValues[3]);

		double CentroidValues[2] = { 0.0, 0.0 };
		if (ReadBridgeNumbers(**Object, TEXT("centroid"), 2, CentroidValues))
		{
			Out.Centroid = FVector2D(CentroidValues[0], CentroidValues[1]);
		}
		else if (Out.bHasBox)
		{
			Out.Centroid = FVector2D((BoxValues[0] + BoxValues[2]) * 0.5, (BoxValues[1] + BoxValues[3]) * 0.5);
		}

		Out.bSelected = !Out.InstanceId.IsEmpty() && Out.InstanceId == OutObjects.SelectedInstanceId;
	}
	return true;
}

void URammsBridgeSubsystem::SetCameraFeedOpen(bool bOpen)
{
	bCameraFeedOpen = bOpen;
	OnCameraFeedRequested.Broadcast(bOpen);
}

// â”€â”€ Outbound â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void URammsBridgeSubsystem::PostButtonPress(const FString& ButtonId)
{
	TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
	Event->SetStringField(TEXT("event_type"), TEXT("button_press"));
	Event->SetStringField(TEXT("button_id"), ButtonId);
	PushEvent(Event);
}

void URammsBridgeSubsystem::PostObjectTapped(const FString& InstanceId, const FString& StreamId)
{
	TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
	Event->SetStringField(TEXT("event_type"), TEXT("object_tapped"));
	Event->SetStringField(TEXT("instance_id"), InstanceId);
	if (!StreamId.IsEmpty())
	{
		Event->SetStringField(TEXT("stream_id"), StreamId);
	}
	PushEvent(Event);
}

void URammsBridgeSubsystem::PushEvent(const TSharedRef<FJsonObject>& Event)
{
	Event->SetNumberField(TEXT("seq"), ++Sequence);

	FBufferedEvent& Buffered = RecentEvents.AddDefaulted_GetRef();
	Buffered.Seq = Sequence;
	Buffered.Object = Event;
	Buffered.Json = ToCondensedJson(Event);

	if (RecentEvents.Num() > MaxRecentEvents)
	{
		RecentEvents.RemoveAt(0);
	}

	TArray<TSharedPtr<FJsonValue>> EventValues;
	for (const FBufferedEvent& E : RecentEvents)
	{
		EventValues.Add(MakeShared<FJsonValueObject>(E.Object));
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("latest_seq"), Sequence);
	Root->SetArrayField(TEXT("events"), EventValues);

	CurrentUIEventsJson = ToCondensedJson(Root);

	UE_LOG(LogTemp, Log, TEXT("[Bridge] UI event: %s"), *CurrentUIEventsJson);
	OnUIEventsJsonChanged.Broadcast(CurrentUIEventsJson);
}

void URammsBridgeSubsystem::GetEventsSince(int32 SinceSequence, int32& OutLatestSequence, TArray<FString>& OutEvents) const
{
	OutLatestSequence = Sequence;
	for (const FBufferedEvent& E : RecentEvents)
	{
		if (E.Seq > SinceSequence)
		{
			OutEvents.Add(E.Json);
		}
	}
}

void URammsBridgeSubsystem::NotifyPropertyChanged(UObject* Object, FName PropertyName)
{
	// Remote Control preset subscriptions run in -game and packaged builds too. The editor learns of the change
	// from OnObjectPropertyChanged, which exists only WITH_EDITOR; game builds have no such delegate, and Remote
	// Control polls watched properties there instead (RemoteControl.FramesBetweenPropertyWatch), so they need
	// nothing here.
	if (!Object)
	{
		return;
	}

	if (FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName))
	{
#if WITH_EDITOR
		FPropertyChangedEvent ChangedEvent(Property, EPropertyChangeType::ValueSet);
		FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(Object, ChangedEvent);
#endif
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Bridge] NotifyPropertyChanged: '%s' has no property '%s'"),
			*Object->GetName(), *PropertyName.ToString());
	}
}
