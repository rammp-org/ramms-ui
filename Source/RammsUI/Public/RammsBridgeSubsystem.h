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
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
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

	/**
	 * A panel's progress "pct" as a 0-1 fraction, accepting either scale a producer may send.
	 *
	 * USER_INPUT_JSON_SCHEMA.md 6.4 states the convention as 0.0-1.0, matching
	 * UProgressBar::Percent, and in the same breath records fridge_ui_demo as yielding
	 * 0.0-100.0 because the widget it targets binds a Percent (0-100) field. Nothing on the
	 * bridge rescales or validates it, so the UI is where the two can be reconciled: an engine
	 * following the documented convention and one following the demo both get the right bar,
	 * instead of each having to know which widget it is talking to.
	 *
	 * Over 1 is read as a percentage and divided by 100; 1 or less is already a fraction. That
	 * is the same rule this codebase uses for detection coordinates, where every component
	 * within 0-1 means normalized and anything larger means pixels. It makes 1.0 mean full
	 * rather than one percent -- the ambiguity is inherent in accepting both, and a producer
	 * wanting 1% can send 1.0 on the percentage scale only by sending 0.01.
	 *
	 * Clamped to 0-1. A negative, infinite or NaN pct gives 0.
	 */
	UFUNCTION(BlueprintPure, Category = "Ramms Bridge|Inbound")
	static float PanelProgressFraction(float Pct);

	/**
	 * Whether two panel payloads are the SAME panel, differing only in what a running action
	 * updates.
	 *
	 * Identity is the title, the buttons (their ids, labels and styles, in order) and the panel
	 * style. The message and the progress are what a step changes as it runs, so they are not part
	 * of it -- an engine pushing "opening door, 30%" then "opening door, 65%" is one panel twice,
	 * not two panels. A style change is a new panel: an error panel should announce itself.
	 *
	 * Keep the previous payload yourself and pass it here: that way what the answer depends on is
	 * visible in the graph, and it is the same answer wherever it is asked. Use it to play a
	 * panel's entrance animation only when the panel actually changed.
	 *
	 * False when either payload is not a JSON object, INCLUDING two identical malformed ones, so a
	 * payload that cannot be understood animates rather than being silently treated as an update.
	 */
	UFUNCTION(BlueprintPure, Category = "Ramms Bridge|Inbound")
	static bool PanelJsonIsSamePanel(const FString& PanelJsonA, const FString& PanelJsonB);

	/**
	 * Whether the panel currently being broadcast is an update to the one before it.
	 *
	 * Convenience over PanelJsonIsSamePanel for the common case: read it inside an OnPanelJson
	 * handler and play the entrance animation only when it is false. It describes the payload
	 * being delivered -- it is set before the broadcast -- so it is only meaningful there; ask the
	 * pure comparison instead if you need an answer at any other time, or want the inputs visible.
	 *
	 * False for the first panel of a session, and for anything that changes the panel's identity.
	 */
	UFUNCTION(BlueprintPure, Category = "Ramms Bridge|Inbound")
	bool IsPanelAnUpdate() const { return bLastPanelWasUpdate; }

	UFUNCTION(BlueprintCallable, Category = "Ramms Bridge|Inbound")
	void BroadcastPanelJson(const FString& PanelJson);

	/**
	 * Forget what the last session showed: the panel it ended on and the objects it had.
	 *
	 * Called automatically when a play session starts. It exists because this is a
	 * UEngineSubsystem: it outlives a PIE run, so without this the first panel of a new run is
	 * compared against the last panel of the previous one -- and a session that ends on the idle
	 * panel and begins with it reads as an update, which skips the entrance animation and leaves
	 * the panel hidden.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms Bridge|Inbound")
	void ResetSessionState();

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

	/** The panel before the one being broadcast, and what comparing their identities said. */
	FString LastPanelJson;
	bool	bLastPanelWasUpdate = false;

	/** Registration for the play-session reset; released in Deinitialize. */
	FDelegateHandle GameInstanceStartHandle;

	FString CurrentUIEventsJson = TEXT("{\"latest_seq\":0,\"events\":[]}");
	int32	Sequence = 0;
	bool	bCameraFeedOpen = false;

	static constexpr int32 MaxRecentEvents = 16;
};
