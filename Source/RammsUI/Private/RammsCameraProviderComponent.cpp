// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsCameraProviderComponent.h"
#include "Engine/Texture2D.h"

URammsCameraProviderComponent::URammsCameraProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 1.0f; // frame rate calculation only
}

// ── Stream Management ──────────────────────────────────────────────

void URammsCameraProviderComponent::RegisterStream(const FRammsCameraStreamInfo& StreamInfo)
{
	FRammsCameraStreamState& State = Streams.FindOrAdd(StreamInfo.StreamID);
	State.Info = StreamInfo;
}

void URammsCameraProviderComponent::UnregisterStream(const FString& StreamID)
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

void URammsCameraProviderComponent::BroadcastFrame(const FString& StreamID, UTexture* Texture)
{
	FRammsCameraStreamState* State = Streams.Find(StreamID);
	if (!State)
		return;

	State->Texture = Texture;
	State->LastTimestamp = FDateTime::UtcNow().GetTicks();
	State->FrameCount++;

	CameraFrameReadyDelegate.Broadcast(StreamID, Texture, State->LastTimestamp);
}

void URammsCameraProviderComponent::BroadcastFrameData(
	const FString& StreamID, const TArray<uint8>& PixelData, int32 Width, int32 Height)
{
	FRammsCameraStreamState* State = Streams.Find(StreamID);
	if (!State || PixelData.Num() == 0 || Width <= 0 || Height <= 0)
		return;

	UTexture2D* Tex2D = Cast<UTexture2D>(State->Texture);
	if (!Tex2D || Tex2D->GetSizeX() != Width || Tex2D->GetSizeY() != Height)
	{
		Tex2D = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
		if (!Tex2D)
			return;
		Tex2D->UpdateResource();
		State->Texture = Tex2D;
	}

	FTexture2DMipMap& Mip = Tex2D->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	const int32 ExpectedSize = Width * Height * 4;
	const int32 CopySize = FMath::Min(PixelData.Num(), ExpectedSize);
	FMemory::Memcpy(TextureData, PixelData.GetData(), CopySize);
	Mip.BulkData.Unlock();
	Tex2D->UpdateResource();

	State->LastTimestamp = FDateTime::UtcNow().GetTicks();
	State->FrameCount++;

	CameraFrameReadyDelegate.Broadcast(StreamID, State->Texture, State->LastTimestamp);
}

void URammsCameraProviderComponent::SetStreamActive(const FString& StreamID, bool bActive)
{
	FRammsCameraStreamState* State = Streams.Find(StreamID);
	if (!State || State->bActive == bActive)
		return;

	State->bActive = bActive;
	CameraStreamStatusDelegate.Broadcast(StreamID, bActive);
}

void URammsCameraProviderComponent::UpdateStreamExtrinsic(const FString& StreamID, const FTransform& WorldTransform)
{
	FRammsCameraStreamState* State = Streams.Find(StreamID);
	if (!State)
		return;

	State->Info.Extrinsic = WorldTransform;
	State->Info.bHasExtrinsic = true;
	CameraExtrinsicUpdatedDelegate.Broadcast(StreamID, WorldTransform);
}

// ── Blueprint Events ───────────────────────────────────────────────

bool URammsCameraProviderComponent::OnStreamStartRequested_Implementation(const FString& StreamID)
{
	SetStreamActive(StreamID, true);
	return true;
}

void URammsCameraProviderComponent::OnStreamStopRequested_Implementation(const FString& StreamID)
{
	SetStreamActive(StreamID, false);
}

// ── IRammsCameraProvider Interface ─────────────────────────────────

TArray<FRammsCameraStreamInfo> URammsCameraProviderComponent::GetAvailableStreams()
{
	TArray<FRammsCameraStreamInfo> Result;
	for (auto& Pair : Streams)
	{
		Result.Add(Pair.Value.Info);
	}
	return Result;
}

bool URammsCameraProviderComponent::GetStreamInfo(const FString& StreamID, FRammsCameraStreamInfo& OutInfo)
{
	if (const FRammsCameraStreamState* State = Streams.Find(StreamID))
	{
		OutInfo = State->Info;
		return true;
	}
	return false;
}

bool URammsCameraProviderComponent::StartStream(const FString& StreamID)
{
	if (!Streams.Contains(StreamID))
		return false;
	return OnStreamStartRequested(StreamID);
}

void URammsCameraProviderComponent::StopStream(const FString& StreamID)
{
	if (!Streams.Contains(StreamID))
		return;
	OnStreamStopRequested(StreamID);
}

bool URammsCameraProviderComponent::IsStreamActive(const FString& StreamID) const
{
	if (const FRammsCameraStreamState* State = Streams.Find(StreamID))
	{
		return State->bActive;
	}
	return false;
}

UTexture* URammsCameraProviderComponent::GetStreamTexture(const FString& StreamID)
{
	if (const FRammsCameraStreamState* State = Streams.Find(StreamID))
	{
		return State->Texture;
	}
	return nullptr;
}

int64 URammsCameraProviderComponent::GetLastFrameTimestamp(const FString& StreamID) const
{
	if (const FRammsCameraStreamState* State = Streams.Find(StreamID))
	{
		return State->LastTimestamp;
	}
	return 0;
}

float URammsCameraProviderComponent::GetActualFrameRate(const FString& StreamID) const
{
	if (const FRammsCameraStreamState* State = Streams.Find(StreamID))
	{
		return State->ActualFrameRate;
	}
	return 0.0f;
}

void URammsCameraProviderComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

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
