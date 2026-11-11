// RammsStreamReceiver.h
//
// Placed once in a level to receive the robot's image / binary streams and hand them to the UI's
// camera widgets. In BeginPlay it starts the RammsStreaming server the robot connects to, and it
// carries the chain from that server to the widgets:
//
//   URammsStreamSinkComponent      frames from the server, as textures
//   URammsStreamCameraBridge       registers each channel as a camera stream (stream_id from the frame's metadata)
//   URammsCameraProviderComponent  what URammsCameraWidget finds and subscribes to (bAutoFindProvider)
//
// The provider is a default subobject rather than left to the bridge to create in its BeginPlay:
// a camera widget looks for providers once, when it is constructed, and the HUD can be constructed
// before this actor's BeginPlay runs.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RammsStreamReceiver.generated.h"

class URammsStreamSinkComponent;
class URammsStreamCameraBridge;
class URammsCameraProviderComponent;

UCLASS(ClassGroup = (RAMMS), meta = (DisplayName = "Ramms Stream Receiver"))
class RAMMSUI_API ARammsStreamReceiver : public AActor
{
	GENERATED_BODY()

public:
	ARammsStreamReceiver();

	/** Start the RammsStreaming server in BeginPlay, unless one is already running. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RAMMS|Streaming")
	bool bStartServer = true;

	/** Port the robot's stream client connects to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RAMMS|Streaming",
		meta = (EditCondition = "bStartServer", ClampMin = "1", ClampMax = "65535"))
	int32 Port = 30030;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RAMMS|Streaming",
		meta = (EditCondition = "bStartServer", ClampMin = "1"))
	int32 MaxClients = 8;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RAMMS|Streaming")
	TObjectPtr<URammsStreamSinkComponent> StreamSink;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RAMMS|Streaming")
	TObjectPtr<URammsStreamCameraBridge> CameraBridge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RAMMS|Streaming")
	TObjectPtr<URammsCameraProviderComponent> CameraProvider;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** This actor started the server, so it stops it. */
	bool bStartedServer = false;
};
