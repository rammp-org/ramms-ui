// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsCameraProviderBase.h"

ARammsCameraProviderBase::ARammsCameraProviderBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.0f; // Only need tick for frame rate calculation
}

void ARammsCameraProviderBase::RegisterStream(const FRammsCameraStreamInfo& StreamInfo, bool bActivate)
{
	FRammsCameraStreamState& State = Streams.FindOrAdd(StreamInfo.StreamID);
	State.Info = StreamInfo;

	if (bActivate && !State.bActive)
	{
		State.bActive = true;
		CameraStreamStatusDelegate.Broadcast(StreamInfo.StreamID, true);
	}
}

void ARammsCameraProviderBase::UnregisterStream(const FString& StreamID)
{
	if (FRammsCameraStreamState* State = Streams.Find(StreamID))
	{
		if (State->bActive)
		{
			State->bActive = false;
			CameraStreamStatusDelegate.Broadcast(StreamID, false);
		}
		Streams.Remove(StreamID);
	}
}

void ARammsCameraProviderBase::BroadcastFrame(const FString& StreamID, UTexture* Texture)
{
	FRammsCameraStreamState* State = Streams.Find(StreamID);
	if (!State)
		return;

	State->Texture = Texture;
	State->LastTimestamp = FDateTime::UtcNow().GetTicks();
	State->FrameCount++;

	CameraFrameReadyDelegate.Broadcast(StreamID, Texture, State->LastTimestamp);
}

void ARammsCameraProviderBase::BroadcastFrameData(const FString& StreamID, const TArray<uint8>& PixelData, int32 Width, int32 Height)
{
	FRammsCameraStreamState* State = Streams.Find(StreamID);
	if (!State || PixelData.Num() == 0 || Width <= 0 || Height <= 0)
		return;

	// Create or recreate UTexture2D if size changed (BroadcastFrameData always uses Texture2D)
	UTexture2D* Tex2D = Cast<UTexture2D>(State->Texture);
	if (!Tex2D || Tex2D->GetSizeX() != Width || Tex2D->GetSizeY() != Height)
	{
		Tex2D = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
		if (!Tex2D)
			return;
		Tex2D->UpdateResource();
		State->Texture = Tex2D;
	}

	// Update texture data
	FTexture2DMipMap& Mip = Tex2D->GetPlatformData()->Mips[0];
	void*			  TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	const int32		  ExpectedSize = Width * Height * 4;
	const int32		  CopySize = FMath::Min(PixelData.Num(), ExpectedSize);
	FMemory::Memcpy(TextureData, PixelData.GetData(), CopySize);
	Mip.BulkData.Unlock();
	Tex2D->UpdateResource();

	// Store CPU-side copy
	State->RawFrameData = PixelData;
	State->FramePixelFormat = PF_B8G8R8A8;

	State->LastTimestamp = FDateTime::UtcNow().GetTicks();
	State->FrameCount++;

	CameraFrameReadyDelegate.Broadcast(StreamID, State->Texture, State->LastTimestamp);
}

void ARammsCameraProviderBase::BroadcastFrameWithRawData(
	const FString& StreamID, UTexture* Texture, TArray<uint8>&& RawData, EPixelFormat PixelFormat)
{
	FRammsCameraStreamState* State = Streams.Find(StreamID);
	if (!State)
		return;

	State->Texture = Texture;
	State->RawFrameData = MoveTemp(RawData);
	State->FramePixelFormat = PixelFormat;
	State->LastTimestamp = FDateTime::UtcNow().GetTicks();
	State->FrameCount++;

	CameraFrameReadyDelegate.Broadcast(StreamID, Texture, State->LastTimestamp);
}

bool ARammsCameraProviderBase::GetStreamRawData(
	const FString& StreamID, const TArray<uint8>*& OutData, EPixelFormat& OutFormat) const
{
	if (const FRammsCameraStreamState* State = Streams.Find(StreamID))
	{
		if (State->RawFrameData.Num() > 0)
		{
			OutData = &State->RawFrameData;
			OutFormat = State->FramePixelFormat;
			return true;
		}
	}
	OutData = nullptr;
	OutFormat = PF_Unknown;
	return false;
}

void ARammsCameraProviderBase::SetStreamActive(const FString& StreamID, bool bActive)
{
	FRammsCameraStreamState* State = Streams.Find(StreamID);
	if (!State || State->bActive == bActive)
		return;

	State->bActive = bActive;
	CameraStreamStatusDelegate.Broadcast(StreamID, bActive);
}

void ARammsCameraProviderBase::UpdateStreamExtrinsic(const FString& StreamID, const FTransform& WorldTransform)
{
	FRammsCameraStreamState* State = Streams.Find(StreamID);
	if (!State)
		return;

	State->Info.Extrinsic = WorldTransform;
	State->Info.bHasExtrinsic = true;
	CameraExtrinsicUpdatedDelegate.Broadcast(StreamID, WorldTransform);
}

void ARammsCameraProviderBase::UpdateStreamMaterialParams(const FString& StreamID, const TMap<FName, float>& Params)
{
	FRammsCameraStreamState* State = Streams.Find(StreamID);
	if (!State)
		return;

	State->Info.MaterialScalarParams = Params;
}

bool ARammsCameraProviderBase::OnStreamStartRequested_Implementation(const FString& StreamID)
{
	// Default: just mark as active. Override in Blueprint for custom behavior.
	SetStreamActive(StreamID, true);
	return true;
}

void ARammsCameraProviderBase::OnStreamStopRequested_Implementation(const FString& StreamID)
{
	// Default: just mark as inactive. Override in Blueprint for custom behavior.
	SetStreamActive(StreamID, false);
}

// --- IRammsCameraProvider ---

TArray<FRammsCameraStreamInfo> ARammsCameraProviderBase::GetAvailableStreams()
{
	TArray<FRammsCameraStreamInfo> Result;
	for (auto& Pair : Streams)
	{
		Result.Add(Pair.Value.Info);
	}
	return Result;
}

bool ARammsCameraProviderBase::GetStreamInfo(const FString& StreamID, FRammsCameraStreamInfo& OutInfo)
{
	if (const FRammsCameraStreamState* State = Streams.Find(StreamID))
	{
		OutInfo = State->Info;
		return true;
	}
	return false;
}

bool ARammsCameraProviderBase::StartStream(const FString& StreamID)
{
	if (!Streams.Contains(StreamID))
		return false;

	return OnStreamStartRequested(StreamID);
}

void ARammsCameraProviderBase::StopStream(const FString& StreamID)
{
	if (!Streams.Contains(StreamID))
		return;

	OnStreamStopRequested(StreamID);
}

bool ARammsCameraProviderBase::IsStreamActive(const FString& StreamID) const
{
	if (const FRammsCameraStreamState* State = Streams.Find(StreamID))
	{
		return State->bActive;
	}
	return false;
}

UTexture* ARammsCameraProviderBase::GetStreamTexture(const FString& StreamID)
{
	if (const FRammsCameraStreamState* State = Streams.Find(StreamID))
	{
		return State->Texture;
	}
	return nullptr;
}

int64 ARammsCameraProviderBase::GetLastFrameTimestamp(const FString& StreamID) const
{
	if (const FRammsCameraStreamState* State = Streams.Find(StreamID))
	{
		return State->LastTimestamp;
	}
	return 0;
}

float ARammsCameraProviderBase::GetActualFrameRate(const FString& StreamID) const
{
	if (const FRammsCameraStreamState* State = Streams.Find(StreamID))
	{
		return State->ActualFrameRate;
	}
	return 0.0f;
}

void ARammsCameraProviderBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update frame rate calculation every second
	for (auto& Pair : Streams)
	{
		FRammsCameraStreamState& State = Pair.Value;
		State.FrameRateTimer += DeltaTime;
		if (State.FrameRateTimer >= 1.0)
		{
			State.ActualFrameRate = State.FrameCount / static_cast<float>(State.FrameRateTimer);
			State.FrameCount = 0;
			State.FrameRateTimer = 0.0;
		}
	}
}
