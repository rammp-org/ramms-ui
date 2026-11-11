// RammsBridgeReceiver.h
//
// Placed once in the level and exposed through the RCP_RammsBridge Remote Control Preset.
// Holds no logic of its own: inbound calls forward to URammsBridgeSubsystem, and the
// outbound UIEventsJson property mirrors the subsystem so the preset subscription fires.
//
// Preset exposes:
//   Functions:  UpdatePanel, UpdateDetectedObjects, UpdateWorldState, OpenCameraFeed,
//               CloseCameraFeed, PollUIEvents
//   Property:   UIEventsJson

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RammsBridgeReceiver.generated.h"

class URammsBridgeSubsystem;

UCLASS()
class RAMMSUI_API ARammsBridgeReceiver : public AActor
{
	GENERATED_BODY()

public:
	ARammsBridgeReceiver();

	// ── Inbound (called by the bridge via Remote Control) ─────────────
	// Parameter names are the JSON keys the bridge sends in "Parameters". Do not rename
	// without telling the bridge author.

	UFUNCTION(BlueprintCallable, Category = "Ramms Bridge|Inbound")
	void UpdatePanel(const FString& PanelJSON);

	UFUNCTION(BlueprintCallable, Category = "Ramms Bridge|Inbound")
	void UpdateDetectedObjects(const FString& DetectedObjectsJSON);

	UFUNCTION(BlueprintCallable, Category = "Ramms Bridge|Inbound")
	void UpdateWorldState(const FString& WorldStateJSON);

	/** Asks the UI to show the camera feed. Takes no parameters. */
	UFUNCTION(BlueprintCallable, Category = "Ramms Bridge|Inbound")
	void OpenCameraFeed();

	/** Asks the UI to hide the camera feed. Takes no parameters. */
	UFUNCTION(BlueprintCallable, Category = "Ramms Bridge|Inbound")
	void CloseCameraFeed();

	// ── Outbound (read by the bridge) ─────────────────────────────────

	/** {"latest_seq":N,"events":[...]}. Bridge subscribes to changes via the preset WebSocket. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ramms Bridge|Outbound")
	FString UIEventsJson;

	/** Polling fallback: returns every buffered event with seq > SinceSequence. */
	UFUNCTION(BlueprintCallable, Category = "Ramms Bridge|Outbound")
	void PollUIEvents(int32 SinceSequence, int32& LatestSequence, TArray<FString>& Events);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleUIEventsJsonChanged(const FString& NewUIEventsJson);

	static URammsBridgeSubsystem* GetBridge();
	FString GetWorldLabel() const;
};
