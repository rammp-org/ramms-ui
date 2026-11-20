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

namespace
{
	/** Read metadata intrinsics {fx, fy, cx, cy} into [fx, fy, cx, cy]; false (and Out untouched) if absent or partial. */
	bool ReadIntrinsicsFromMeta(const TSharedPtr<FJsonObject>& Meta, TArray<float>& Out)
	{
		const TSharedPtr<FJsonObject>* IntrObj = nullptr;
		double						   Fx, Fy, Cx, Cy;
		if (!Meta.IsValid() || !Meta->TryGetObjectField(TEXT("intrinsics"), IntrObj) || !IntrObj->IsValid()
			|| !(*IntrObj)->TryGetNumberField(TEXT("fx"), Fx) || !(*IntrObj)->TryGetNumberField(TEXT("fy"), Fy)
			|| !(*IntrObj)->TryGetNumberField(TEXT("cx"), Cx) || !(*IntrObj)->TryGetNumberField(TEXT("cy"), Cy))
		{
			return false;
		}
		Out = { static_cast<float>(Fx), static_cast<float>(Fy), static_cast<float>(Cx), static_cast<float>(Cy) };
		return true;
	}
} // namespace

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

	// Default to the previously-resolved StreamID for this channel (prevents
	// flapping back to the auto-generated ID when a frame omits stream_id).
	const FString* CachedStreamID = ChannelStreamIDs.Find(ChannelID);
	FString		   StreamID = CachedStreamID
			   ? *CachedStreamID
			   : FString::Printf(TEXT("%s/%d"), *StreamPrefix, ChannelID);

	// Parse metadata (needed for both registration and per-frame updates)
	TSharedPtr<FJsonObject>	  Meta;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MetadataJson);
	const bool				  bHasMeta = FJsonSerializer::Deserialize(Reader, Meta) && Meta.IsValid();

	// Allow sender to override the auto-generated stream ID via metadata
	if (bHasMeta)
	{
		FString OverrideID;
		if (Meta->TryGetStringField(TEXT("stream_id"), OverrideID) && !OverrideID.IsEmpty())
		{
			StreamID = OverrideID;
		}
	}

	// Check if this channel needs (re-)registration.
	// A stream_id override arriving after the first frame requires re-registration.
	const bool bNewStreamID = !CachedStreamID || *CachedStreamID != StreamID;

	// So does a registration that no longer describes the stream. Intrinsics and size used to be read
	// from the first frame only, so a stream first seen without intrinsics -- or at another resolution
	// -- kept that registration for good, and everything that unprojects through it (depth-derived
	// world markers) failed for the rest of the session while later frames carried the right values.
	bool bStaleRegistration = false;
	if (!bNewStreamID)
	{
		FRammsCameraStreamInfo Registered;
		if (CameraProvider->GetStreamInfo(StreamID, Registered))
		{
			TArray<float> SentIntrinsics;
			bStaleRegistration = Registered.Width != Texture->GetSizeX() || Registered.Height != Texture->GetSizeY()
				|| (bHasMeta && ReadIntrinsicsFromMeta(Meta, SentIntrinsics) && SentIntrinsics != Registered.Intrinsics);
		}
	}
	const bool bNeedsRegistration = bNewStreamID || bStaleRegistration;

	if (bNeedsRegistration)
	{
		// Fully unregister the old stream so it doesn't linger in GetAvailableStreams(). Not when the
		// id is unchanged: RegisterStream updates it in place, and keeps it active.
		if (bNewStreamID && CachedStreamID && !CachedStreamID->IsEmpty())
		{
			CameraProvider->UnregisterStream(*CachedStreamID);
		}

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
			// Override display name from metadata
			FString NameStr;
			if (Meta->TryGetStringField(TEXT("name"), NameStr) && !NameStr.IsEmpty())
			{
				Info.DisplayName = NameStr;
			}

			// ── Stream association: group & role ──
			FString GroupStr;
			if (Meta->TryGetStringField(TEXT("group"), GroupStr))
			{
				Info.GroupID = GroupStr;
			}

			FString RoleStr;
			if (Meta->TryGetStringField(TEXT("role"), RoleStr))
			{
				if (RoleStr == TEXT("color") || RoleStr == TEXT("rgb"))
					Info.StreamRole = ERammsStreamRole::Color;
				else if (RoleStr == TEXT("depth"))
					Info.StreamRole = ERammsStreamRole::Depth;
				else if (RoleStr == TEXT("mask") || RoleStr == TEXT("segmentation"))
					Info.StreamRole = ERammsStreamRole::Mask;
				else if (RoleStr == TEXT("infrared") || RoleStr == TEXT("ir"))
					Info.StreamRole = ERammsStreamRole::Infrared;
				else
					Info.StreamRole = ERammsStreamRole::Other;
			}

			// ── Format detection ──
			FString Fmt;
			if (Meta->TryGetStringField(TEXT("fmt"), Fmt))
			{
				if (Fmt == TEXT("float32") || Fmt == TEXT("depth"))
				{
					Info.bIsDepth = true;
					Info.PixelFormat = TEXT("R32F");
					Info.DepthFormat = ERammsDepthFormat::Float32CM;
					if (Info.StreamRole == ERammsStreamRole::Other)
						Info.StreamRole = ERammsStreamRole::Depth;
				}
				else if (Fmt == TEXT("16uc1") || Fmt == TEXT("uint16") || Fmt == TEXT("mono16"))
				{
					Info.bIsDepth = true;
					Info.PixelFormat = TEXT("G16");
					Info.DepthFormat = ERammsDepthFormat::Uint16MM;
					if (Info.StreamRole == ERammsStreamRole::Other)
						Info.StreamRole = ERammsStreamRole::Depth;
				}
				else if (Fmt == TEXT("rgb8"))
				{
					Info.PixelFormat = TEXT("RGB8");
					if (Info.StreamRole == ERammsStreamRole::Other)
						Info.StreamRole = ERammsStreamRole::Color;
				}
				else if (Fmt == TEXT("r8") || Fmt == TEXT("gray8") || Fmt == TEXT("mono8"))
				{
					Info.PixelFormat = TEXT("G8");
					// Don't force a role — mono data could be a grayscale camera feed.
					// The existing fallback logic below infers Color from message type.
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

			// Infer role from bIsDepth / message type if role wasn't set explicitly
			if (Info.StreamRole == ERammsStreamRole::Other)
			{
				if (Info.bIsDepth || MessageType == ERammsStreamMessageType::FrameDepth)
					Info.StreamRole = ERammsStreamRole::Depth;
				else if (MessageType == ERammsStreamMessageType::FrameRGB
					|| MessageType == ERammsStreamMessageType::ImageData)
					Info.StreamRole = ERammsStreamRole::Color;
			}

			// Extract intrinsics [fx, fy, cx, cy]
			ReadIntrinsicsFromMeta(Meta, Info.Intrinsics);

			// Extract extrinsic transform
			FTransform ExtractedTransform;
			if (ParseTransformFromMeta(Meta, ExtractedTransform))
			{
				Info.Extrinsic = ExtractedTransform;
				Info.bHasExtrinsic = true;
			}

			// Extract dynamic material scalar parameters
			const TSharedPtr<FJsonObject>* MatParamsObj = nullptr;
			if (Meta->TryGetObjectField(TEXT("MaterialScalarParameters"), MatParamsObj) && MatParamsObj->IsValid())
			{
				constexpr int32 MaxParamEntries = 64;
				int32			Count = 0;
				for (const auto& Pair : (*MatParamsObj)->Values)
				{
					if (Count >= MaxParamEntries)
						break;
					double Val = 0.0;
					if (Pair.Key.Len() > 0 && Pair.Key.Len() <= 128
						&& Pair.Value.IsValid() && Pair.Value->TryGetNumber(Val))
					{
						const FName ParamName(*Pair.Key, FNAME_Find);
						if (ParamName != NAME_None)
						{
							Info.MaterialScalarParams.Add(ParamName, static_cast<float>(Val));
							++Count;
						}
					}
				}
			}
		}
		else
		{
			Info.PixelFormat = TEXT("BGRA8");
		}

		// Auto-generate GroupID from StreamPrefix + channel if sender didn't provide one
		if (Info.GroupID.IsEmpty())
		{
			Info.GroupID = FString::Printf(TEXT("%s/%d"), *StreamPrefix, ChannelID);
		}

		// A new stream starts active. An update in place keeps whatever state consumers left it in: a
		// stream a widget stopped stays stopped, and its projector disabled.
		CameraProvider->RegisterStream(Info, bNewStreamID);
		if (bNewStreamID)
		{
			CameraProvider->SetStreamActive(StreamID, true);
		}
		ChannelStreamIDs.Add(ChannelID, StreamID);

		UE_LOG(LogRammsStreamBridge, Log,
			TEXT("%s stream '%s' (group='%s', role=%d, %dx%d, fmt=%s, intrinsics=%d)"),
			bNewStreamID ? TEXT("Auto-registered") : TEXT("Re-registered"),
			*StreamID, *Info.GroupID, static_cast<int32>(Info.StreamRole),
			Width, Height, *Info.PixelFormat, Info.Intrinsics.Num());
	}
	else if (bHasMeta)
	{
		// Per-frame extrinsic update (camera may move between frames)
		FTransform ExtractedTransform;
		if (ParseTransformFromMeta(Meta, ExtractedTransform))
		{
			CameraProvider->UpdateStreamExtrinsic(StreamID, ExtractedTransform);
		}

		// Per-frame material scalar parameter update.
		// If metadata exists but field is absent, clear previous overrides
		// so senders can stop sending the field to remove params.
		const TSharedPtr<FJsonObject>* MatParamsObj = nullptr;
		if (Meta->TryGetObjectField(TEXT("MaterialScalarParameters"), MatParamsObj) && MatParamsObj->IsValid())
		{
			TMap<FName, float> Params;
			constexpr int32	   MaxParamEntries = 64;
			int32			   Count = 0;
			for (const auto& Pair : (*MatParamsObj)->Values)
			{
				if (Count >= MaxParamEntries)
					break;
				double Val = 0.0;
				if (Pair.Key.Len() > 0 && Pair.Key.Len() <= 128
					&& Pair.Value.IsValid() && Pair.Value->TryGetNumber(Val))
				{
					const FName ParamName(*Pair.Key, FNAME_Find);
					if (ParamName != NAME_None)
					{
						Params.Add(ParamName, static_cast<float>(Val));
						++Count;
					}
					else
					{
						UE_LOG(
							LogRammsStreamBridge,
							Verbose,
							TEXT("Ignoring unknown material scalar parameter name '%s' for stream '%s'"),
							*Pair.Key,
							*StreamID);
					}
				}
			}
			CameraProvider->UpdateStreamMaterialParams(StreamID, Params);
		}
		else
		{
			// Field absent from metadata — clear any previous overrides
			static const TMap<FName, float> EmptyParams;
			CameraProvider->UpdateStreamMaterialParams(StreamID, EmptyParams);
		}
	}

	// Forward the texture with CPU-side raw data for PGM consumers
	// (only when at least one consumer has requested raw data forwarding).
	TArray<uint8> SinkRaw;
	EPixelFormat  PixelFormat = PF_Unknown;
	bool		  bForwardRaw = RawDataRequestCount > 0;
	if (!bForwardRaw && DepthRawDataRequestCount > 0)
	{
		FRammsCameraStreamInfo Registered;
		bForwardRaw = MessageType == ERammsStreamMessageType::FrameDepth
			|| (CameraProvider->GetStreamInfo(StreamID, Registered)
				&& (Registered.StreamRole == ERammsStreamRole::Depth || Registered.bIsDepth));
	}
	if (bForwardRaw)
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
	UE_LOG(LogRammsStreamBridge, Verbose, TEXT("StreamCameraBridge: raw data requested (count=%d)"), RawDataRequestCount);
}

void URammsStreamCameraBridge::ReleaseRawDataForwarding()
{
	RawDataRequestCount = FMath::Max(0, RawDataRequestCount - 1);
	UE_LOG(LogRammsStreamBridge, Verbose, TEXT("StreamCameraBridge: raw data released (count=%d)"), RawDataRequestCount);
}

void URammsStreamCameraBridge::RequestDepthRawDataForwarding()
{
	++DepthRawDataRequestCount;
	UE_LOG(LogRammsStreamBridge, Verbose, TEXT("StreamCameraBridge: depth raw data requested (count=%d)"), DepthRawDataRequestCount);
}

void URammsStreamCameraBridge::ReleaseDepthRawDataForwarding()
{
	DepthRawDataRequestCount = FMath::Max(0, DepthRawDataRequestCount - 1);
	UE_LOG(LogRammsStreamBridge, Verbose, TEXT("StreamCameraBridge: depth raw data released (count=%d)"), DepthRawDataRequestCount);
}
