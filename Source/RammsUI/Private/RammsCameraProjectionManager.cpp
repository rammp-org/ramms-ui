// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsCameraProjectionManager.h"
#include "RammsCameraProjectorComponent.h"
#include "RammsCameraProviderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

URammsCameraProjectionManager::URammsCameraProjectionManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URammsCameraProjectionManager::BeginPlay()
{
	Super::BeginPlay();

	// Auto-discover: if no provider assigned yet, find one in the world
	if (!CameraProviderObject.IsValid() && bAutoCreateProjectors)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			// First try actor-based providers
			TArray<AActor*> ProviderActors;
			UGameplayStatics::GetAllActorsWithInterface(World, URammsCameraProvider::StaticClass(), ProviderActors);
			for (AActor* Actor : ProviderActors)
			{
				if (Actor == GetOwner())
					continue;
				if (IRammsCameraProvider* Iface = Cast<IRammsCameraProvider>(Actor))
				{
					TScriptInterface<IRammsCameraProvider> Provider;
					Provider.SetObject(Actor);
					Provider.SetInterface(Iface);
					SetCameraProvider(Provider);
					UE_LOG(LogTemp, Log, TEXT("RammsCameraProjectionManager: Auto-discovered actor camera provider: %s"), *Actor->GetName());
					break;
				}
			}

			// If no actor-based provider found, search for component-based providers
			if (!CameraProviderObject.IsValid())
			{
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					if (*It == GetOwner())
						continue;
					if (URammsCameraProviderComponent* Comp = It->FindComponentByClass<URammsCameraProviderComponent>())
					{
						TScriptInterface<IRammsCameraProvider> Provider;
						Provider.SetObject(Comp);
						Provider.SetInterface(static_cast<IRammsCameraProvider*>(Comp));
						SetCameraProvider(Provider);
						UE_LOG(LogTemp, Log, TEXT("RammsCameraProjectionManager: Auto-discovered component camera provider on: %s"), *It->GetName());
						break;
					}
				}
			}
		}
	}
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
	ExtrinsicUpdatedHandle = Iface->OnCameraExtrinsicUpdated().AddUObject(
		this, &URammsCameraProjectionManager::OnCameraExtrinsicUpdated);
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
		if (ExtrinsicUpdatedHandle.IsValid())
		{
			Iface->OnCameraExtrinsicUpdated().Remove(ExtrinsicUpdatedHandle);
		}
	}
	FrameReadyHandle.Reset();
	StreamStatusHandle.Reset();
	ExtrinsicUpdatedHandle.Reset();
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

void URammsCameraProjectionManager::OnCameraExtrinsicUpdated(const FString& StreamID, const FTransform& WorldTransform)
{
	if (TObjectPtr<URammsCameraProjectorComponent>* Found = Projectors.Find(StreamID))
	{
		(*Found)->SetCameraTransform(WorldTransform);
	}
}
