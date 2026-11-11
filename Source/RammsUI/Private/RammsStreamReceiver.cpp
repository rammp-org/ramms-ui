// RammsStreamReceiver.cpp

#include "RammsStreamReceiver.h"

#include "RammsCameraProviderComponent.h"
#include "RammsStreamCameraBridge.h"
#include "RammsStreamingSubsystem.h"
#include "RammsStreamSinkComponent.h"

#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogRammsStreamReceiver, Log, All);

ARammsStreamReceiver::ARammsStreamReceiver()
{
	PrimaryActorTick.bCanEverTick = false;

	StreamSink = CreateDefaultSubobject<URammsStreamSinkComponent>(TEXT("StreamSink"));
	CameraBridge = CreateDefaultSubobject<URammsStreamCameraBridge>(TEXT("CameraBridge"));
	CameraProvider = CreateDefaultSubobject<URammsCameraProviderComponent>(TEXT("CameraProvider"));
}

void ARammsStreamReceiver::BeginPlay()
{
	// Components first: the sink binds to the streaming subsystem, the bridge to the sink and the provider.
	Super::BeginPlay();

	if (!bStartServer)
	{
		return;
	}

	UGameInstance*			  GameInstance = GetGameInstance();
	URammsStreamingSubsystem* Streaming = GameInstance ? GameInstance->GetSubsystem<URammsStreamingSubsystem>() : nullptr;
	if (!Streaming)
	{
		UE_LOG(LogRammsStreamReceiver, Warning, TEXT("%s: no RammsStreaming subsystem, so no stream server"), *GetName());
		return;
	}
	if (Streaming->IsServerRunning())
	{
		UE_LOG(LogRammsStreamReceiver, Log, TEXT("%s: stream server already running on port %d"), *GetName(), Streaming->GetServerPort());
		return;
	}
	bStartedServer = Streaming->StartServer(Port, MaxClients);
	if (bStartedServer)
	{
		UE_LOG(LogRammsStreamReceiver, Log, TEXT("%s: stream server listening on port %d"), *GetName(), Port);
	}
	else
	{
		UE_LOG(LogRammsStreamReceiver, Error, TEXT("%s: could not start the stream server on port %d"), *GetName(), Port);
	}
}

void ARammsStreamReceiver::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bStartedServer)
	{
		UGameInstance* GameInstance = GetGameInstance();
		if (URammsStreamingSubsystem* Streaming = GameInstance ? GameInstance->GetSubsystem<URammsStreamingSubsystem>() : nullptr)
		{
			Streaming->StopServer();
		}
		bStartedServer = false;
	}

	Super::EndPlay(EndPlayReason);
}
