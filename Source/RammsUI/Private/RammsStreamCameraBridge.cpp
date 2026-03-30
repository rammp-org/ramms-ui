// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsStreamCameraBridge.h"
#include "RammsCameraProviderComponent.h"
#include "RammsStreamSinkComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogRammsStreamBridge, Log, All);

URammsStreamCameraBridge::URammsStreamCameraBridge()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URammsStreamCameraBridge::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
		return;

	// Find or create a camera provider component on the same actor
	CameraProvider = Owner->FindComponentByClass<URammsCameraProviderComponent>();
	if (!CameraProvider)
	{
		CameraProvider = NewObject<URammsCameraProviderComponent>(Owner,
			URammsCameraProviderComponent::StaticClass(),
			TEXT("StreamCameraProvider"));
		CameraProvider->RegisterComponent();
		UE_LOG(LogRammsStreamBridge, Log, TEXT("Auto-created CameraProviderComponent on %s"),
			*Owner->GetName());
	}

	// Bind to every sink component on this actor
	TArray<URammsStreamSinkComponent*> Sinks;
	Owner->GetComponents<URammsStreamSinkComponent>(Sinks);
	for (URammsStreamSinkComponent* Sink : Sinks)
	{
		Sink->OnFrameReceived.AddUniqueDynamic(this, &URammsStreamCameraBridge::OnStreamFrameReceived);
		BoundSinks.Add(Sink);
	}

	UE_LOG(LogRammsStreamBridge, Log, TEXT("StreamCameraBridge bound to %d sink(s) on %s"),
		Sinks.Num(), *Owner->GetName());
}

void URammsStreamCameraBridge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AActor* Owner = GetOwner();
	if (Owner)
	{
		TArray<URammsStreamSinkComponent*> Sinks;
		Owner->GetComponents<URammsStreamSinkComponent>(Sinks);
		for (URammsStreamSinkComponent* Sink : Sinks)
		{
			Sink->OnFrameReceived.RemoveDynamic(this, &URammsStreamCameraBridge::OnStreamFrameReceived);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void URammsStreamCameraBridge::OnStreamFrameReceived(
	int32 ChannelID, UTexture2D* Texture, const FString& MetadataJson, ERammsStreamMessageType MessageType)
{
	if (!CameraProvider || !Texture)
		return;

	// Category-based filter
	if (bVisualOnly && GetFrameCategory(MessageType) != ERammsFrameCategory::Visual)
		return;

	// Explicit channel exclusion
	if (ExcludeChannels.Contains(ChannelID))
		return;

	FString StreamID = FString::Printf(TEXT("%s/%d"), *StreamPrefix, ChannelID);

	// Parse metadata (needed for both registration and per-frame updates)
	TSharedPtr<FJsonObject>	  Meta;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MetadataJson);
	const bool				  bHasMeta = FJsonSerializer::Deserialize(Reader, Meta) && Meta.IsValid();

	// Auto-register the stream on first frame
	if (!RegisteredChannels.Contains(ChannelID))
	{
		const int32 Width = Texture->GetSizeX();
		const int32 Height = Texture->GetSizeY();

		FRammsCameraStreamInfo Info;
		Info.StreamID = StreamID;
		Info.DisplayName = FString::Printf(TEXT("Stream Channel %d"), ChannelID);
		Info.Width = Width;
		Info.Height = Height;
		Info.FrameCategory = GetFrameCategory(MessageType);

		if (bHasMeta)
		{
			// Detect depth vs RGB from format metadata
			FString Fmt;
			if (Meta->TryGetStringField(TEXT("fmt"), Fmt))
			{
				if (Fmt == TEXT("float32") || Fmt == TEXT("depth"))
				{
					Info.bIsDepth = true;
					Info.PixelFormat = TEXT("R32F");
				}
				else
				{
					Info.PixelFormat = Fmt.ToUpper();
				}
			}
			else
			{
				Info.PixelFormat = TEXT("BGRA8");
			}

			// Extract intrinsics [fx, fy, cx, cy]
			const TSharedPtr<FJsonObject>* IntrObj = nullptr;
			if (Meta->TryGetObjectField(TEXT("intrinsics"), IntrObj) && IntrObj->IsValid())
			{
				double Fx, Fy, Cx, Cy;
				if ((*IntrObj)->TryGetNumberField(TEXT("fx"), Fx)
					&& (*IntrObj)->TryGetNumberField(TEXT("fy"), Fy)
					&& (*IntrObj)->TryGetNumberField(TEXT("cx"), Cx)
					&& (*IntrObj)->TryGetNumberField(TEXT("cy"), Cy))
				{
					Info.Intrinsics.SetNum(4);
					Info.Intrinsics[0] = static_cast<float>(Fx);
					Info.Intrinsics[1] = static_cast<float>(Fy);
					Info.Intrinsics[2] = static_cast<float>(Cx);
					Info.Intrinsics[3] = static_cast<float>(Cy);
				}
			}

			// Extract extrinsic transform
			FTransform ExtractedTransform;
			if (ParseTransformFromMeta(Meta, ExtractedTransform))
			{
				Info.Extrinsic = ExtractedTransform;
				Info.bHasExtrinsic = true;
			}
		}
		else
		{
			Info.PixelFormat = TEXT("BGRA8");
		}

		CameraProvider->RegisterStream(Info);
		CameraProvider->SetStreamActive(StreamID, true);
		RegisteredChannels.Add(ChannelID);

		UE_LOG(LogRammsStreamBridge, Log,
			TEXT("Auto-registered stream '%s' (%dx%d, depth=%d, intrinsics=%d)"),
			*StreamID, Width, Height, Info.bIsDepth ? 1 : 0, Info.Intrinsics.Num());
	}
	else if (bHasMeta)
	{
		// Per-frame extrinsic update (camera may move between frames)
		FTransform ExtractedTransform;
		if (ParseTransformFromMeta(Meta, ExtractedTransform))
		{
			CameraProvider->UpdateStreamExtrinsic(StreamID, ExtractedTransform);
		}
	}

	// Forward the texture with CPU-side raw data for PGM consumers
	// (only when at least one consumer has requested raw data forwarding).
	TArray<uint8> SinkRaw;
	EPixelFormat  PixelFormat = PF_Unknown;
	if (RawDataRequestCount > 0)
	{
		for (URammsStreamSinkComponent* Sink : BoundSinks)
		{
			if (!Sink)
				continue;
			if (Sink->GetLatestRawData(ChannelID, SinkRaw) && SinkRaw.Num() > 0)
			{
				PixelFormat = Sink->GetLatestPixelFormat(ChannelID);
				break;
			}
		}
	}

	if (SinkRaw.Num() > 0)
	{
		CameraProvider->BroadcastFrameWithRawData(StreamID, Texture, MoveTemp(SinkRaw), PixelFormat);
	}
	else
	{
		CameraProvider->BroadcastFrame(StreamID, Texture);
	}
}

bool URammsStreamCameraBridge::ParseTransformFromMeta(
	const TSharedPtr<FJsonObject>& Meta, FTransform& OutTransform) const
{
	const TSharedPtr<FJsonObject>* TransObj = nullptr;
	if (!Meta->TryGetObjectField(TEXT("transform"), TransObj) || !TransObj->IsValid())
	{
		return false;
	}

	double X = 0, Y = 0, Z = 0, Pitch = 0, Yaw = 0, Roll = 0;
	if (!(*TransObj)->TryGetNumberField(TEXT("x"), X)
		|| !(*TransObj)->TryGetNumberField(TEXT("y"), Y)
		|| !(*TransObj)->TryGetNumberField(TEXT("z"), Z)
		|| !(*TransObj)->TryGetNumberField(TEXT("pitch"), Pitch)
		|| !(*TransObj)->TryGetNumberField(TEXT("yaw"), Yaw)
		|| !(*TransObj)->TryGetNumberField(TEXT("roll"), Roll))
	{
		return false;
	}

	FTransform ParsedTransform(FRotator(Pitch, Yaw, Roll), FVector(X, Y, Z));

	// If the transform is relative, compose it with our owner actor's world transform
	FString TransformSpace;
	if (Meta->TryGetStringField(TEXT("transform_space"), TransformSpace)
		&& TransformSpace == TEXT("relative"))
	{
		if (AActor* Owner = GetOwner())
		{
			OutTransform = ParsedTransform * Owner->GetActorTransform();
		}
		else
		{
			OutTransform = ParsedTransform;
		}
	}
	else
	{
		OutTransform = ParsedTransform;
	}
	return true;
}

void URammsStreamCameraBridge::RequestRawDataForwarding()
{
	++RawDataRequestCount;
	UE_LOG(LogTemp, Log, TEXT("StreamCameraBridge: raw data requested (count=%d)"), RawDataRequestCount);
}

void URammsStreamCameraBridge::ReleaseRawDataForwarding()
{
	RawDataRequestCount = FMath::Max(0, RawDataRequestCount - 1);
	UE_LOG(LogTemp, Log, TEXT("StreamCameraBridge: raw data released (count=%d)"), RawDataRequestCount);
}
