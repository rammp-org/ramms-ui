// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsCameraProjectionManager.h"
#include "RammsCameraProjectorComponent.h"

URammsCameraProjectionManager::URammsCameraProjectionManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URammsCameraProjectionManager::BeginPlay()
{
	Super::BeginPlay();
}

void URammsCameraProjectionManager::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	UnbindProvider();
	RemoveAllProjectors();
	Super::EndPlay(EndPlayReason);
}

// ── Provider Binding ───────────────────────────────────────────────

IRammsCameraProvider* URammsCameraProjectionManager::GetProvider() const
{
	UObject* Obj = CameraProviderObject.Get();
	return Obj ? Cast<IRammsCameraProvider>(Obj) : nullptr;
}

void URammsCameraProjectionManager::SetCameraProvider(TScriptInterface<IRammsCameraProvider> Provider)
{
	UnbindProvider();

	UObject* Obj = Provider.GetObject();
	if (!Obj)
		return;

	IRammsCameraProvider* Iface = Cast<IRammsCameraProvider>(Obj);
	if (!Iface)
		return;

	CameraProviderObject = Obj;
	BindProvider(Iface);

	if (bAutoCreateProjectors)
	{
		CreateProjectorsForExistingStreams();
	}
}

void URammsCameraProjectionManager::ClearCameraProvider()
{
	UnbindProvider();
}

void URammsCameraProjectionManager::BindProvider(IRammsCameraProvider* Iface)
{
	FrameReadyHandle = Iface->OnCameraFrameReady().AddUObject(
		this, &URammsCameraProjectionManager::OnCameraFrameReady);
	StreamStatusHandle = Iface->OnCameraStreamStatus().AddUObject(
		this, &URammsCameraProjectionManager::OnCameraStreamStatus);
}

void URammsCameraProjectionManager::UnbindProvider()
{
	IRammsCameraProvider* Iface = GetProvider();
	if (Iface)
	{
		if (FrameReadyHandle.IsValid())
		{
			Iface->OnCameraFrameReady().Remove(FrameReadyHandle);
		}
		if (StreamStatusHandle.IsValid())
		{
			Iface->OnCameraStreamStatus().Remove(StreamStatusHandle);
		}
	}
	FrameReadyHandle.Reset();
	StreamStatusHandle.Reset();
	CameraProviderObject.Reset();
}

void URammsCameraProjectionManager::CreateProjectorsForExistingStreams()
{
	IRammsCameraProvider* Iface = GetProvider();
	if (!Iface)
		return;

	for (const FRammsCameraStreamInfo& Info : Iface->GetAvailableStreams())
	{
		// Skip depth streams — only project colour imagery
		if (Info.bIsDepth || Projectors.Contains(Info.StreamID))
			continue;

		URammsCameraProjectorComponent* Projector = AddProjector(Info.StreamID);
		if (Projector)
		{
			Projector->SetIntrinsicsFromStreamInfo(Info);
		}
	}
}

// ── Projector Management ───────────────────────────────────────────

URammsCameraProjectorComponent* URammsCameraProjectionManager::AddProjector(const FString& StreamID)
{
	if (TObjectPtr<URammsCameraProjectorComponent>* Existing = Projectors.Find(StreamID))
	{
		return *Existing;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !ProjectionMaterial)
		return nullptr;

	URammsCameraProjectorComponent* Projector = NewObject<URammsCameraProjectorComponent>(Owner);
	Projector->ProjectionMaterial    = ProjectionMaterial;
	Projector->FadeWidth             = DefaultFadeWidth;
	Projector->MaxProjectionDistance  = DefaultMaxDistance;
	Projector->TargetStencilValue    = DefaultTargetStencil;

	Projector->SetupAttachment(Owner->GetRootComponent());
	Projector->RegisterComponent();

	// Apply intrinsics from provider if available
	IRammsCameraProvider* Iface = GetProvider();
	if (Iface)
	{
		FRammsCameraStreamInfo Info;
		if (Iface->GetStreamInfo(StreamID, Info))
		{
			Projector->SetIntrinsicsFromStreamInfo(Info);
		}

		// Start the stream if not yet active
		if (!Iface->IsStreamActive(StreamID))
		{
			Iface->StartStream(StreamID);
		}

		// Attach any already-available texture
		UTexture* Tex = Iface->GetStreamTexture(StreamID);
		if (Tex)
		{
			Projector->SetCameraTexture(Tex);
		}
	}

	Projectors.Add(StreamID, Projector);
	return Projector;
}

void URammsCameraProjectionManager::SetProjectorTransform(const FString& StreamID, const FTransform& WorldTransform)
{
	if (TObjectPtr<URammsCameraProjectorComponent>* Found = Projectors.Find(StreamID))
	{
		(*Found)->SetCameraTransform(WorldTransform);
	}
}

void URammsCameraProjectionManager::RemoveProjector(const FString& StreamID)
{
	TObjectPtr<URammsCameraProjectorComponent> Projector;
	if (Projectors.RemoveAndCopyValue(StreamID, Projector) && Projector)
	{
		Projector->DestroyComponent();
	}
}

void URammsCameraProjectionManager::RemoveAllProjectors()
{
	for (auto& Pair : Projectors)
	{
		if (Pair.Value)
		{
			Pair.Value->DestroyComponent();
		}
	}
	Projectors.Empty();
}

URammsCameraProjectorComponent* URammsCameraProjectionManager::GetProjector(const FString& StreamID) const
{
	if (const TObjectPtr<URammsCameraProjectorComponent>* Found = Projectors.Find(StreamID))
	{
		return *Found;
	}
	return nullptr;
}

TArray<FString> URammsCameraProjectionManager::GetProjectorStreamIDs() const
{
	TArray<FString> IDs;
	Projectors.GetKeys(IDs);
	return IDs;
}

// ── Provider Callbacks ─────────────────────────────────────────────

void URammsCameraProjectionManager::OnCameraFrameReady(const FString& StreamID, UTexture* Texture, int64 Timestamp)
{
	if (TObjectPtr<URammsCameraProjectorComponent>* Found = Projectors.Find(StreamID))
	{
		(*Found)->SetCameraTexture(Texture);
	}
}

void URammsCameraProjectionManager::OnCameraStreamStatus(const FString& StreamID, bool bActive)
{
	if (bActive && bAutoCreateProjectors && !Projectors.Contains(StreamID))
	{
		IRammsCameraProvider* Iface = GetProvider();
		if (Iface)
		{
			FRammsCameraStreamInfo Info;
			if (Iface->GetStreamInfo(StreamID, Info) && !Info.bIsDepth)
			{
				URammsCameraProjectorComponent* Projector = AddProjector(StreamID);
				if (Projector)
				{
					Projector->SetIntrinsicsFromStreamInfo(Info);
				}
			}
		}
	}

	if (TObjectPtr<URammsCameraProjectorComponent>* Found = Projectors.Find(StreamID))
	{
		(*Found)->SetProjectionEnabled(bActive);
	}
}
