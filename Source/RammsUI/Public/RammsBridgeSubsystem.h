// RammsBridgeSubsystem.h
//
// Engine-level hub between the Remote Control receiver actor and the UI.
// One instance shared by the editor world, PIE and packaged games, so it does not
// matter which receiver copy a Remote Control call lands on.
//
//   Inbound  (bridge -> UE): receiver forwards raw JSON strings here -> delegates -> widgets parse
//   Outbound (UE -> bridge): widgets call PostButtonPress / PostObjectTapped -> UIEventsJson

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Dom/JsonObject.h"
#include "RammsBridgeSubsystem.generated.h"

/** One entry of the bridge's detected-objects "objects" array, parsed from JSON. */
USTRUCT(BlueprintType)
struct RAMMSUI_API FRammsBridgeDetectedObject
{
	GENERATED_BODY()

	/** "instance_id": the id to send back in PostObjectTapped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramms Bridge|Detected Object")
	FString InstanceId;

	/** "perception_class", e.g. "cup". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramms Bridge|Detected Object")
	FString PerceptionClass;

	/** "source": which camera the object belongs to; falls back to the payload's stream_id when absent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramms Bridge|Detected Object")
	FString Source;

	/** "box" as X = x_min, Y = y_min, Z = x_max, W = y_max. Normalized 0-1, or pixels when any value is > 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramms Bridge|Detected Object")
	FVector4 Box = FVector4(0, 0, 0, 0);

	/** False when the payload had no box, or an all-zero/degenerate one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramms Bridge|Detected Object")
	bool bHasBox = false;

	/** "centroid"; the box centre when the payload left it out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramms Bridge|Detected Object")
	FVector2D Centroid = FVector2D::ZeroVector;

	/** True when this object's instance_id is the payload's selected_instance_id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramms Bridge|Detected Object")
	bool bSelected = false;
};

/** The whole {stream_id, selected_instance_id, objects} payload, parsed. */
USTRUCT(BlueprintType)
struct RAMMSUI_API FRammsBridgeDetectedObjects
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramms Bridge|Detected Object")
	FString StreamId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramms Bridge|Detected Object")
	FString SelectedInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramms Bridge|Detected Object")
	TArray<FRammsBridgeDetectedObject> Objects;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRammsPanelJson, const FString&, PanelJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRammsDetectedObjectsJson, const FString&, DetectedObjectsJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRammsDetectedObjects, const FRammsBridgeDetectedObjects&, DetectedObjects);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRammsUIEventsJson, const FString&, UIEventsJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRammsCameraFeedRequested, bool, bOpen);

UCLASS()
class RAMMSUI_API URammsBridgeSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// ── Inbound (bridge -> UE) ────────────────────────────────────────

	/** Fired with the raw InteractionPanel JSON. Panel widget binds and parses. */
	UPROPERTY(BlueprintAssignable, Category = "Ramms Bridge|Inbound")
	FOnRammsPanelJson OnPanelJson;

	/** Fired with the raw {stream_id, selected_instance_id, objects} JSON. Overlay widget binds and parses. */
	UPROPERTY(BlueprintAssignable, Category = "Ramms Bridge|Inbound")
	FOnRammsDetectedObjectsJson OnDetectedObjectsJson;

	/** Fired right after OnDetectedObjectsJson with the same payload parsed into an array. Not fired if the JSON doesn't parse. */
	UPROPERTY(BlueprintAssignable, Category = "Ramms Bridge|Inbound")
	FOnRammsDetectedObjects OnDetectedObjects;

	/** The last detected-objects payload received, parsed. Lets a widget created later start with the current objects. */
	UFUNCTION(BlueprintPure, Category = "Ramms Bridge|Inbound")
	const FRammsBridgeDetectedObjects& GetLastDetectedObjects() const { return LastDetectedObjects; }

	/**
	 * Parses a {stream_id, selected_instance_id, objects} string. box/centroid accept {"X":..} objects or bare arrays.
	 * Returns false (and leaves OutObjects empty) if the string isn't a JSON object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms Bridge|Inbound")
	static bool ParseDetectedObjectsJson(const FString& DetectedObjectsJson, FRammsBridgeDetectedObjects& OutObjects);

	/**
	 * Whether the bridge's panel JSON carries a "progress" object.
	 *
	 * The schema makes progress optional by OMITTING THE KEY rather than sending null, so that the
	 * key's presence is itself the flag (USER_INPUT_JSON_SCHEMA.md 6.1). Json String To Struct
	 * cannot express that: it matches keys to property names, so FInteractionPanel's hasprogress --
	 * which no producer sends, because it is not in the schema -- stays false and the progress bar
	 * never shows, for the orchestrator's panels as much as for a test push. Set hasprogress from
	 * this after parsing.
	 *
	 * False for a payload that is not a JSON object, or whose "progress" is not an object.
	 */
	UFUNCTION(BlueprintPure, Category = "Ramms Bridge|Inbound")
	static bool PanelJsonHasProgress(const FString& PanelJson);

	UFUNCTION(BlueprintCallable, Category = "Ramms Bridge|Inbound")
	void BroadcastPanelJson(const FString& PanelJson);

	UFUNCTION(BlueprintCallable, Category = "Ramms Bridge|Inbound")
	void BroadcastDetectedObjectsJson(const FString& DetectedObjectsJson);

	/** Fired when the bridge asks to open (true) or close (false) the camera feed. The UI binds and shows/hides it. */
	UPROPERTY(BlueprintAssignable, Category = "Ramms Bridge|Inbound")
	FOnRammsCameraFeedRequested OnCameraFeedRequested;

	/** Records the requested state and fires OnCameraFeedRequested. Fires even if the state is unchanged. */
	UFUNCTION(BlueprintCallable, Category = "Ramms Bridge|Inbound")
	void SetCameraFeedOpen(bool bOpen);

	/** Last state the bridge asked for. Lets a widget created later start in the right state. */
	UFUNCTION(BlueprintPure, Category = "Ramms Bridge|Inbound")
	bool IsCameraFeedOpen() const { return bCameraFeedOpen; }

	// ── Outbound (UE -> bridge) ───────────────────────────────────────

	/** Call from a panel button's OnClicked with that button's button_id. */
	UFUNCTION(BlueprintCallable, Category = "Ramms Bridge|Outbound")
	void PostButtonPress(const FString& ButtonId);

	/**
	 * Call from a detection box's OnClicked with that box's instance_id, and the stream_id of the
	 * camera it was tapped on (e.g. "camera/scene/color"; left out of the event when empty).
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms Bridge|Outbound")
	void PostObjectTapped(const FString& InstanceId, const FString& StreamId = TEXT(""));

	/** Fired with the full {"latest_seq":N,"events":[...]} string after every event. Receiver binds. */
	UPROPERTY(BlueprintAssignable, Category = "Ramms Bridge|Outbound")
	FOnRammsUIEventsJson OnUIEventsJsonChanged;

	/** Current {"latest_seq":N,"events":[...]} string. */
	UFUNCTION(BlueprintPure, Category = "Ramms Bridge|Outbound")
	FString GetUIEventsJson() const { return CurrentUIEventsJson; }

	UFUNCTION(BlueprintPure, Category = "Ramms Bridge|Outbound")
	int32 GetLatestSequence() const { return Sequence; }

	/** For the polling fallback: every buffered event with seq > SinceSequence, as individual JSON strings. */
	void GetEventsSince(int32 SinceSequence, int32& OutLatestSequence, TArray<FString>& OutEvents) const;

	/** Raises a property-changed notification so Remote Control's WebSocket subscription sees code-side writes. */
	UFUNCTION(BlueprintCallable, Category = "Ramms Bridge|Outbound")
	static void NotifyPropertyChanged(UObject* Object, FName PropertyName);

private:
	void PushEvent(const TSharedRef<FJsonObject>& Event);

	struct FBufferedEvent
	{
		int32					Seq = 0;
		TSharedPtr<FJsonObject> Object;
		FString					Json;
	};

	TArray<FBufferedEvent>		RecentEvents;
	FRammsBridgeDetectedObjects LastDetectedObjects;
	FString						CurrentUIEventsJson = TEXT("{\"latest_seq\":0,\"events\":[]}");
	int32						Sequence = 0;
	bool						bCameraFeedOpen = false;

	static constexpr int32 MaxRecentEvents = 16;
};
