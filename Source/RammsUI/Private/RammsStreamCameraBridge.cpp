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
	if (!Owner) return;

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
		Sink->OnFrameReceived.AddDynamic(this, &URammsStreamCameraBridge::OnStreamFrameReceived);
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
	int32 ChannelID, UTexture2D* Texture, const FString& MetadataJson)
{
	if (!CameraProvider || !Texture) return;

	FString StreamID = FString::Printf(TEXT("%s/%d"), *StreamPrefix, ChannelID);

	// Parse metadata (needed for both registration and per-frame updates)
	TSharedPtr<FJsonObject> Meta;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MetadataJson);
	const bool bHasMeta = FJsonSerializer::Deserialize(Reader, Meta) && Meta.IsValid();

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
				Info.Intrinsics.SetNum(4);
				Info.Intrinsics[0] = static_cast<float>((*IntrObj)->GetNumberField(TEXT("fx")));
				Info.Intrinsics[1] = static_cast<float>((*IntrObj)->GetNumberField(TEXT("fy")));
				Info.Intrinsics[2] = static_cast<float>((*IntrObj)->GetNumberField(TEXT("cx")));
				Info.Intrinsics[3] = static_cast<float>((*IntrObj)->GetNumberField(TEXT("cy")));
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

	// Forward the texture
	CameraProvider->BroadcastFrame(StreamID, Texture);
}

bool URammsStreamCameraBridge::ParseTransformFromMeta(
	const TSharedPtr<FJsonObject>& Meta, FTransform& OutTransform) const
{
	const TSharedPtr<FJsonObject>* TransObj = nullptr;
	if (!Meta->TryGetObjectField(TEXT("transform"), TransObj) || !TransObj->IsValid())
	{
		return false;
	}

	FVector Loc(
		(*TransObj)->GetNumberField(TEXT("x")),
		(*TransObj)->GetNumberField(TEXT("y")),
		(*TransObj)->GetNumberField(TEXT("z"))
	);
	FRotator Rot(
		(*TransObj)->GetNumberField(TEXT("pitch")),
		(*TransObj)->GetNumberField(TEXT("yaw")),
		(*TransObj)->GetNumberField(TEXT("roll"))
	);
	FTransform ParsedTransform(Rot, Loc);

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
