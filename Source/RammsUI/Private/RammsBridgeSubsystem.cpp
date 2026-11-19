// RammsBridgeSubsystem.cpp

#include "RammsBridgeSubsystem.h"

#include "Engine/World.h"

#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogRammsBridge, Log, All);

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

void URammsBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// A UEngineSubsystem outlives a play session, so whatever the last run ended with is still
	// here when the next one starts. For the detections that only means a stale replay; for the
	// panel it is worse, because the first panel of a run then reads as an update to the last
	// panel of the previous one and its entrance animation -- which is what makes it visible --
	// is skipped. A walkthrough that ends on the idle panel and starts with it does exactly that.
	GameInstanceStartHandle = FWorldDelegates::OnStartGameInstance.AddWeakLambda(
		this, [this](UGameInstance*) { ResetSessionState(); });
}

void URammsBridgeSubsystem::ResetSessionState()
{
	LastPanelJson.Empty();
	bLastPanelWasUpdate = false;
	LastDetectedObjects = FRammsBridgeDetectedObjects();

	// Same stale-replay problem, and for the same reason: it exists so a widget created later
	// starts in the right state, which makes a session that ended with the feed open leak "open"
	// into the next run. The UI event sequence is NOT reset here -- a bridge polling across a PIE
	// restart holds a since_seq, and starting over at 0 would starve it.
	bCameraFeedOpen = false;
}

void URammsBridgeSubsystem::Deinitialize()
{
	FWorldDelegates::OnStartGameInstance.Remove(GameInstanceStartHandle);
	GameInstanceStartHandle.Reset();

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

float URammsBridgeSubsystem::PanelProgressFraction(float Pct)
{
	if (!FMath::IsFinite(Pct) || Pct <= 0.0f)
	{
		return 0.0f;
	}
	const float Fraction = Pct > 1.0f ? Pct / 100.0f : Pct;
	return FMath::Clamp(Fraction, 0.0f, 1.0f);
}

namespace RammsPanelIdentity
{
	/** The fields a running action changes are excluded; what is left is the panel's identity. */
	static FString Of(const FString& PanelJson)
	{
		TSharedPtr<FJsonObject>			Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PanelJson);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			// Not a JSON object: it has no identity to match, so it can never be "the same panel"
			// as anything, including itself. A malformed payload replays the animation, which is
			// the safe way round -- the alternative is a panel that silently never animates.
			return FString();
		}

		// A panel carries at least one of these. Without them every {}-shaped object -- an engine's
		// bare status panel, or anything else that reaches UpdatePanel -- would reduce to the same
		// identity and match the idle panel, so a different panel would skip its animation.
		if (!Root->HasField(TEXT("title")) && !Root->HasField(TEXT("style")) && !Root->HasField(TEXT("buttons")))
		{
			return FString();
		}

		// Absent style means "normal" (USER_INPUT_JSON_SCHEMA.md 6.1 and 6.3), so a producer that
		// starts sending it explicitly must not read as a different panel.
		const auto Style = [](const TSharedPtr<FJsonObject>& Object) {
			FString Value;
			Object->TryGetStringField(TEXT("style"), Value);
			return Value.IsEmpty() ? FString(TEXT("normal")) : Value;
		};

		FString Identity;
		FString Value;
		Root->TryGetStringField(TEXT("title"), Value);
		Identity += Value + TEXT("\x1f");
		Identity += Style(Root) + TEXT("\x1f");

		// Buttons are deliberately NOT part of this. A panel swaps its own controls as it runs --
		// Pause becomes Resume when the operator pauses an action -- and that is the same panel,
		// not a new one. They still decide whether a payload is a panel at all, above.
		return Identity;
	}
} // namespace RammsPanelIdentity

bool URammsBridgeSubsystem::PanelJsonIsSamePanel(const FString& PanelJsonA, const FString& PanelJsonB)
{
	const FString A = RammsPanelIdentity::Of(PanelJsonA);
	return !A.IsEmpty() && A == RammsPanelIdentity::Of(PanelJsonB);
}

void URammsBridgeSubsystem::BroadcastPanelJson(const FString& PanelJson)
{
	// Set before the broadcast: a handler reads it to decide whether to play the entrance
	// animation, so it has to describe the payload being delivered, not the one before it.
	bLastPanelWasUpdate = !LastPanelJson.IsEmpty() && PanelJsonIsSamePanel(LastPanelJson, PanelJson);
	LastPanelJson = PanelJson;

	// A handler may push a panel of its own in response. That inner call would leave its own
	// answer behind, and the handlers after it in this broadcast would read it against the payload
	// they are being handed -- so the outer answer is restored once the inner one is done.
	const bool bWasUpdate = bLastPanelWasUpdate;
	OnPanelJson.Broadcast(PanelJson);
	bLastPanelWasUpdate = bWasUpdate;
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

		// Negative is taken as "not sent": a producer with no answer should leave the field out,
		// but a sentinel is the obvious way to get that wrong and it must not read as a time.
		double Seconds = 0.0;
		if ((*Object)->TryGetNumberField(TEXT("time_since_detection_s"), Seconds) && Seconds >= 0.0)
		{
			Out.SecondsSinceDetection = static_cast<float>(Seconds);
			Out.bHasSecondsSinceDetection = true;
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
	if (ButtonId.IsEmpty())
	{
		// button_id is required of a panel button (USER_INPUT_JSON_SCHEMA.md 6.3), so an empty one
		// is a bug on this side -- a lost ExposeOnSpawn pin, a panel built without ids -- and the
		// press cannot be matched to anything. Dropping it keeps meaningless traffic off the
		// outbound path; the warning is so it does not become a button that silently does nothing.
		// A control belonging to this UI should not reach here at all: a local button calls its
		// own OnClicked and skips this.
		UE_LOG(LogRammsBridge, Warning, TEXT("[Bridge] button press with an empty button_id, dropped; a panel button needs an id, and a local control should not report at all"));
		return;
	}

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
