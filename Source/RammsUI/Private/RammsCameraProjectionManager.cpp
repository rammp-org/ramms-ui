// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsCameraProjectionManager.h"
#include "RammsCameraProjectorComponent.h"
#include "RammsCameraProviderComponent.h"
#include "RammsStreamCameraBridge.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "TimerManager.h"

#if WITH_EDITOR
void URammsCameraProjectionManager::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Update all projectors when manager settings change
	for (auto& Pair : Projectors)
	{
		URammsCameraProjectorComponent* P = Pair.Value;
		if (!P)
			continue;

		P->ProjectionMaterial = ProjectionMaterial;
		P->FadeWidth = DefaultFadeWidth;
		P->MaxProjectionDistance = DefaultMaxDistance;
		P->TargetStencilValue = DefaultTargetStencil;
		P->bAutoDisableStencilOnVulkan = bAutoDisableStencilOnVulkan;
		P->DecalBoundsInflation = DefaultDecalBoundsInflation;

		P->bEnablePGM = bEnablePGM;
		P->bGPUAccelerated = bGPUAccelerated;
		P->PGMMaterial = PGMMaterial;
		P->MaxEdgeStretchCM = MaxEdgeStretchCM;
		P->DepthScaleToCM = DepthScaleToCM;
		P->MinDepthCM = MinDepthCM;
		P->MaxDepthCM = MaxDepthCM;
		P->Decimation = Decimation;
		P->SensorBaselineY = SensorBaselineY;
		P->SyncThresholdMS = SyncThresholdMS;
		P->SetPGMCustomDepthStencil(bPGMRenderCustomDepth, PGMCustomStencilValue);

		P->RefreshMaterialParameters();
	}
}
#endif

DEFINE_LOG_CATEGORY_STATIC(LogRammsProjection, Log, All);

URammsCameraProjectionManager::URammsCameraProjectionManager()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Default PGM settings matching URammsCameraProjectorComponent
	bEnablePGM = false;
	MaxEdgeStretchCM = 50.0f;
	DepthScaleToCM = 1.0f;
	MinDepthCM = 10.0f;
	MaxDepthCM = 1000.0f;
	Decimation = 4;
	SensorBaselineY = 0.0f;
	SyncThresholdMS = 100.0f;
}

void URammsCameraProjectionManager::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogRammsProjection, Log,
		TEXT("ProjectionManager BeginPlay on '%s' (autoCreate=%d, material=%s)"),
		*GetOwner()->GetName(), bAutoCreateProjectors ? 1 : 0,
		ProjectionMaterial ? *ProjectionMaterial->GetName() : TEXT("NONE"));

	if (bAutoCreateProjectors)
	{
		DiscoverProviders();

		// Schedule a deferred re-scan so we pick up providers created
		// later in BeginPlay (e.g. StreamCameraBridge auto-creates one).
		// Use 0.1s delay — UE timers with 0.0 delay may not fire.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				DeferredDiscoveryHandle, FTimerDelegate::CreateWeakLambda(this, [this]() {
					UE_LOG(LogRammsProjection, Log, TEXT("Deferred discovery timer fired"));
					DiscoverProviders();
				}),
				0.1f, // small delay to ensure all BeginPlays have completed
				false // one-shot
			);
		}
	}

	// CPU PGM needs raw data from the bridge; GPU PGM does not.
	UpdateRawDataRequest();
}

void URammsCameraProjectionManager::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	// Release raw data request if we made one
	if (bRequestedRawData)
	{
		if (URammsStreamCameraBridge* Bridge = GetOwner()->FindComponentByClass<URammsStreamCameraBridge>())
		{
			Bridge->ReleaseRawDataForwarding();
		}
		bRequestedRawData = false;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredDiscoveryHandle);
	}
	UnbindAllProviders();
	RemoveAllProjectors();
	Super::EndPlay(EndPlayReason);
}

// ── Provider Discovery ────────────────────────────────────────────

void URammsCameraProjectionManager::DiscoverProviders()
{
	UWorld* World = GetWorld();
	if (!World)
		return;

	int32 NewCount = 0;

	// Search for actor-based providers
	TArray<AActor*> ProviderActors;
	UGameplayStatics::GetAllActorsWithInterface(World, URammsCameraProvider::StaticClass(), ProviderActors);
	for (AActor* Actor : ProviderActors)
	{
		if (Actor == GetOwner())
			continue;
		if (IsProviderBound(Actor))
			continue;
		if (IRammsCameraProvider* Iface = Cast<IRammsCameraProvider>(Actor))
		{
			BindProvider(Actor, Iface);
			CreateProjectorsForProvider(Iface);
			UE_LOG(LogRammsProjection, Log,
				TEXT("Bound actor provider: %s"), *Actor->GetName());
			++NewCount;
		}
	}

	// Search for component-based providers (include our own actor —
	// the bridge may have auto-created a provider component on it)
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<URammsCameraProviderComponent*> Comps;
		(*It)->GetComponents<URammsCameraProviderComponent>(Comps);
		for (URammsCameraProviderComponent* Comp : Comps)
		{
			if (IsProviderBound(Comp))
				continue;
			BindProvider(Comp, static_cast<IRammsCameraProvider*>(Comp));
			CreateProjectorsForProvider(static_cast<IRammsCameraProvider*>(Comp));
			UE_LOG(LogRammsProjection, Log,
				TEXT("Bound component provider on: %s"), *(*It)->GetName());
			++NewCount;
		}
	}

	if (NewCount > 0)
	{
		UE_LOG(LogRammsProjection, Log,
			TEXT("Discovery complete: %d new provider(s), %d total"),
			NewCount, ProviderBindings.Num());
	}
	else
	{
		UE_LOG(LogRammsProjection, Log,
			TEXT("Discovery found no new providers (%d already bound)"),
			ProviderBindings.Num());
	}
}

// ── Provider Binding ──────────────────────────────────────────────

IRammsCameraProvider* URammsCameraProjectionManager::GetProviderFromBinding(const FProviderBinding& Binding)
{
	UObject* Obj = Binding.Object.Get();
	return Obj ? Cast<IRammsCameraProvider>(Obj) : nullptr;
}

IRammsCameraProvider* URammsCameraProjectionManager::FindProviderForStream(const FString& StreamID) const
{
	for (const FProviderBinding& B : ProviderBindings)
	{
		if (IRammsCameraProvider* Iface = GetProviderFromBinding(B))
		{
			FRammsCameraStreamInfo Info;
			if (Iface->GetStreamInfo(StreamID, Info))
			{
				return Iface;
			}
		}
	}
	return nullptr;
}

bool URammsCameraProjectionManager::IsProviderBound(UObject* Obj) const
{
	for (const FProviderBinding& B : ProviderBindings)
	{
		if (B.Object.Get() == Obj)
			return true;
	}
	return false;
}

void URammsCameraProjectionManager::SetCameraProvider(TScriptInterface<IRammsCameraProvider> Provider)
{
	// Legacy single-provider API: clear all, then add this one
	UnbindAllProviders();

	UObject* Obj = Provider.GetObject();
	if (!Obj)
		return;

	IRammsCameraProvider* Iface = Cast<IRammsCameraProvider>(Obj);
	if (!Iface)
		return;

	BindProvider(Obj, Iface);

	if (bAutoCreateProjectors)
	{
		CreateProjectorsForProvider(Iface);
	}
}

void URammsCameraProjectionManager::AddCameraProvider(TScriptInterface<IRammsCameraProvider> Provider)
{
	UObject* Obj = Provider.GetObject();
	if (!Obj)
		return;
	if (IsProviderBound(Obj))
		return;

	IRammsCameraProvider* Iface = Cast<IRammsCameraProvider>(Obj);
	if (!Iface)
		return;

	BindProvider(Obj, Iface);

	if (bAutoCreateProjectors)
	{
		CreateProjectorsForProvider(Iface);
	}
}

void URammsCameraProjectionManager::ClearCameraProvider()
{
	UnbindAllProviders();
}

void URammsCameraProjectionManager::BindProvider(UObject* Obj, IRammsCameraProvider* Iface)
{
	FProviderBinding Binding;
	Binding.Object = Obj;
	Binding.FrameReadyHandle = Iface->OnCameraFrameReady().AddUObject(
		this, &URammsCameraProjectionManager::OnCameraFrameReady);
	Binding.StreamStatusHandle = Iface->OnCameraStreamStatus().AddUObject(
		this, &URammsCameraProjectionManager::OnCameraStreamStatus);
	Binding.ExtrinsicUpdatedHandle = Iface->OnCameraExtrinsicUpdated().AddUObject(
		this, &URammsCameraProjectionManager::OnCameraExtrinsicUpdated);
	ProviderBindings.Add(MoveTemp(Binding));
}

void URammsCameraProjectionManager::UnbindAllProviders()
{
	for (FProviderBinding& B : ProviderBindings)
	{
		if (IRammsCameraProvider* Iface = GetProviderFromBinding(B))
		{
			if (B.FrameReadyHandle.IsValid())
				Iface->OnCameraFrameReady().Remove(B.FrameReadyHandle);
			if (B.StreamStatusHandle.IsValid())
				Iface->OnCameraStreamStatus().Remove(B.StreamStatusHandle);
			if (B.ExtrinsicUpdatedHandle.IsValid())
				Iface->OnCameraExtrinsicUpdated().Remove(B.ExtrinsicUpdatedHandle);
		}
	}
	ProviderBindings.Empty();
}

void URammsCameraProjectionManager::CreateProjectorsForProvider(IRammsCameraProvider* Iface)
{
	if (!Iface)
		return;

	TArray<FRammsCameraStreamInfo> Streams = Iface->GetAvailableStreams();
	UE_LOG(LogRammsProjection, Log, TEXT("CreateProjectorsForProvider: %d available stream(s)"), Streams.Num());

	for (const FRammsCameraStreamInfo& Info : Streams)
	{
		UE_LOG(LogRammsProjection, Verbose,
			TEXT("  Stream '%s': role=%d, group='%s', %dx%d, intrinsics=%d, hasExtrinsic=%d, category=%d"),
			*Info.StreamID, static_cast<int32>(Info.StreamRole), *Info.GroupID,
			Info.Width, Info.Height,
			Info.Intrinsics.Num(), Info.bHasExtrinsic ? 1 : 0,
			static_cast<int32>(Info.FrameCategory));

		// Skip non-visual streams (motion vectors, point clouds, etc.)
		if (Info.FrameCategory != ERammsFrameCategory::Visual)
			continue;

		// Skip explicitly excluded streams
		if (ExcludeStreamIDs.Contains(Info.StreamID))
			continue;

		// Skip existing projectors
		if (Projectors.Contains(Info.StreamID))
			continue;

		// Depth streams don't get their own decal projector — they are
		// linked to a color projector for PGM use.  But we must still
		// ensure the depth stream is started so frames flow.
		if (Info.bIsDepth || Info.StreamRole == ERammsStreamRole::Depth)
		{
			if (!Iface->IsStreamActive(Info.StreamID))
			{
				Iface->StartStream(Info.StreamID);
				UE_LOG(LogRammsProjection, Log,
					TEXT("Started depth stream '%s' (no projector — will be linked to color)"),
					*Info.StreamID);
			}
			continue;
		}

		URammsCameraProjectorComponent* Projector = AddProjector(Info.StreamID);
		if (Projector)
		{
			Projector->SetIntrinsicsFromStreamInfo(Info);
			if (Info.bHasExtrinsic)
			{
				Projector->SetCameraTransform(Info.Extrinsic);
			}

			// Auto-link depth stream if one is already registered
			FString DepthStreamID = FindDepthStreamForColor(Info.StreamID, Iface);
			if (!DepthStreamID.IsEmpty())
			{
				Projector->DepthStreamID = DepthStreamID;
				FRammsCameraStreamInfo DepthInfo;
				if (Iface->GetStreamInfo(DepthStreamID, DepthInfo))
				{
					Projector->ApplyDepthStreamFormat(DepthInfo);
				}
			}
		}
	}
}

// ── Projector Management ──────────────────────────────────────────

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
	Projector->ProjectionMaterial = ProjectionMaterial;
	Projector->FadeWidth = DefaultFadeWidth;
	Projector->MaxProjectionDistance = DefaultMaxDistance;
	Projector->DecalBoundsInflation = DefaultDecalBoundsInflation;
	Projector->TargetStencilValue = DefaultTargetStencil;
	Projector->bAutoDisableStencilOnVulkan = bAutoDisableStencilOnVulkan;

	// Copy PGM settings
	Projector->bEnablePGM = bEnablePGM;
	Projector->bGPUAccelerated = bGPUAccelerated;
	Projector->PGMMaterial = PGMMaterial;
	Projector->MaxEdgeStretchCM = MaxEdgeStretchCM;
	Projector->DepthScaleToCM = DepthScaleToCM;
	Projector->MinDepthCM = MinDepthCM;
	Projector->MaxDepthCM = MaxDepthCM;
	Projector->Decimation = Decimation;
	Projector->SensorBaselineY = SensorBaselineY;
	Projector->SyncThresholdMS = SyncThresholdMS;
	Projector->bPGMRenderCustomDepth = bPGMRenderCustomDepth;
	Projector->PGMCustomStencilValue = PGMCustomStencilValue;

	Projector->SetupAttachment(Owner->GetRootComponent());
	Projector->RegisterComponent();

	// Apply intrinsics, extrinsic, and texture from whichever provider knows this stream
	IRammsCameraProvider* Iface = FindProviderForStream(StreamID);
	if (Iface)
	{
		FRammsCameraStreamInfo Info;
		if (Iface->GetStreamInfo(StreamID, Info))
		{
			Projector->SetIntrinsicsFromStreamInfo(Info);

			// Apply initial extrinsic so the projector is positioned correctly
			if (Info.bHasExtrinsic)
			{
				Projector->SetCameraTransform(Info.Extrinsic);
				UE_LOG(LogRammsProjection, Log,
					TEXT("Projector '%s' initial extrinsic: %s"),
					*StreamID, *Info.Extrinsic.GetLocation().ToString());
			}
		}

		if (!Iface->IsStreamActive(StreamID))
		{
			Iface->StartStream(StreamID);
		}

		UTexture* Tex = Iface->GetStreamTexture(StreamID);
		if (Tex)
		{
			Projector->SetCameraTexture(Tex);
		}

		// --- Auto-Link Depth Stream ---
		FString DepthID = FindDepthStreamForColor(StreamID, Iface);
		if (!DepthID.IsEmpty())
		{
			Projector->DepthStreamID = DepthID;

			// Apply depth format scaling from the depth stream's info
			FRammsCameraStreamInfo DepthInfo;
			if (Iface->GetStreamInfo(DepthID, DepthInfo))
			{
				Projector->ApplyDepthStreamFormat(DepthInfo);
			}

			if (!Iface->IsStreamActive(DepthID))
			{
				Iface->StartStream(DepthID);
			}
			UTexture* DepthTex = Iface->GetStreamTexture(DepthID);
			if (DepthTex)
			{
				Projector->SetDepthTexture(DepthTex, Iface->GetLastFrameTimestamp(DepthID));
			}
		}

		UE_LOG(LogRammsProjection, Log,
			TEXT("Created projector for '%s' (texture=%s)"),
			*StreamID, Tex ? TEXT("yes") : TEXT("no"));
	}
	else
	{
		UE_LOG(LogRammsProjection, Warning,
			TEXT("AddProjector('%s'): no provider found — projector has no data source"),
			*StreamID);
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

// ── Provider Callbacks ────────────────────────────────────────────

void URammsCameraProjectionManager::OnCameraFrameReady(const FString& StreamID, UTexture* Texture, int64 Timestamp)
{
	// Try to get raw data from the provider for PGM use
	IRammsCameraProvider* Provider = FindProviderForStream(StreamID);
	const TArray<uint8>*  RawData = nullptr;
	EPixelFormat		  RawFormat = PF_Unknown;
	int32				  TexWidth = 0;
	int32				  TexHeight = 0;

	if (Provider)
	{
		Provider->GetStreamRawData(StreamID, RawData, RawFormat);
	}
	if (UTexture2D* Tex2D = Cast<UTexture2D>(Texture))
	{
		TexWidth = Tex2D->GetSizeX();
		TexHeight = Tex2D->GetSizeY();
	}
	else if (UTextureRenderTarget2D* RT = Cast<UTextureRenderTarget2D>(Texture))
	{
		TexWidth = RT->SizeX;
		TexHeight = RT->SizeY;
	}
	else if (Provider)
	{
		// Fallback: use stream info dimensions
		FRammsCameraStreamInfo Info;
		if (Provider->GetStreamInfo(StreamID, Info))
		{
			TexWidth = Info.Width;
			TexHeight = Info.Height;
		}
	}

	UE_LOG(LogRammsProjection, Verbose,
		TEXT("OnCameraFrameReady('%s'): raw=%d bytes, fmt=%d, %dx%d"),
		*StreamID, RawData ? RawData->Num() : 0, (int32)RawFormat, TexWidth, TexHeight);

	// 1. Try to find a projector where this is the primary (color) stream
	if (TObjectPtr<URammsCameraProjectorComponent>* Found = Projectors.Find(StreamID))
	{
		if (RawData && RawData->Num() > 0)
		{
			(*Found)->SetColorTextureWithData(Texture, Timestamp,
				TConstArrayView<uint8>(*RawData), RawFormat, TexWidth, TexHeight);
		}
		else
		{
			(*Found)->SetColorTexture(Texture, Timestamp);
		}
		return;
	}

	// 2. Try to find a projector where this is the secondary (depth) stream
	for (auto& Pair : Projectors)
	{
		if (Pair.Value && Pair.Value->DepthStreamID == StreamID)
		{
			if (RawData && RawData->Num() > 0)
			{
				Pair.Value->SetDepthTextureWithData(Texture, Timestamp,
					TConstArrayView<uint8>(*RawData), RawFormat, TexWidth, TexHeight);
			}
			else
			{
				Pair.Value->SetDepthTexture(Texture, Timestamp);
			}
			return;
		}
	}

	// 3. Unknown stream — might be a newly-registered depth stream that
	//    wasn't linked yet. Try to auto-link.
	if (Provider && bAutoCreateProjectors)
	{
		FRammsCameraStreamInfo Info;
		if (Provider->GetStreamInfo(StreamID, Info)
			&& (Info.bIsDepth || Info.StreamRole == ERammsStreamRole::Depth))
		{
			FString ColorStreamID = FindColorStreamForDepth(StreamID, Provider);
			if (!ColorStreamID.IsEmpty())
			{
				if (TObjectPtr<URammsCameraProjectorComponent>* ColorProjector = Projectors.Find(ColorStreamID))
				{
					(*ColorProjector)->DepthStreamID = StreamID;
					(*ColorProjector)->ApplyDepthStreamFormat(Info);
					if (RawData && RawData->Num() > 0)
					{
						(*ColorProjector)->SetDepthTextureWithData(Texture, Timestamp, TConstArrayView<uint8>(*RawData), RawFormat, TexWidth, TexHeight);
					}
					else
					{
						(*ColorProjector)->SetDepthTexture(Texture, Timestamp);
					}
				}
			}
		}
	}
}

void URammsCameraProjectionManager::OnCameraStreamStatus(const FString& StreamID, bool bActive)
{
	UE_LOG(LogRammsProjection, Log,
		TEXT("OnCameraStreamStatus('%s', active=%d) — %d provider(s) bound, %d projector(s)"),
		*StreamID, bActive ? 1 : 0, ProviderBindings.Num(), Projectors.Num());

	if (bActive && bAutoCreateProjectors && !Projectors.Contains(StreamID))
	{
		// Apply the same filtering as CreateProjectorsForProvider
		if (ExcludeStreamIDs.Contains(StreamID))
		{
			UE_LOG(LogRammsProjection, Verbose,
				TEXT("OnCameraStreamStatus: skipping excluded stream '%s'"), *StreamID);
		}
		else
		{
			IRammsCameraProvider* Iface = FindProviderForStream(StreamID);
			if (Iface)
			{
				FRammsCameraStreamInfo Info;
				if (Iface->GetStreamInfo(StreamID, Info))
				{
					// Skip non-visual streams
					if (Info.FrameCategory != ERammsFrameCategory::Visual)
					{
						UE_LOG(LogRammsProjection, Verbose,
							TEXT("OnCameraStreamStatus: skipping non-visual stream '%s' (category=%d)"),
							*StreamID, static_cast<int32>(Info.FrameCategory));
					}
					else if (Info.bIsDepth || Info.StreamRole == ERammsStreamRole::Depth)
					{
						// Depth stream activated — try to auto-link to its color projector
						FString ColorStreamID = FindColorStreamForDepth(StreamID, Iface);
						if (!ColorStreamID.IsEmpty())
						{
							if (TObjectPtr<URammsCameraProjectorComponent>* ColorProjector = Projectors.Find(ColorStreamID))
							{
								(*ColorProjector)->DepthStreamID = StreamID;
								(*ColorProjector)->ApplyDepthStreamFormat(Info);
							}
						}
					}
					else
					{
						// Non-depth stream — create a projector for it
						URammsCameraProjectorComponent* Projector = AddProjector(StreamID);
						if (Projector)
						{
							Projector->SetIntrinsicsFromStreamInfo(Info);
							UE_LOG(LogRammsProjection, Log,
								TEXT("Auto-created projector for stream '%s' (%dx%d)"),
								*StreamID, Info.Width, Info.Height);
						}
					}
				}
			}
		} // !ExcludeStreamIDs
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

void URammsCameraProjectionManager::SetPGMEnabled(bool bEnabled)
{
	bEnablePGM = bEnabled;

	for (auto& Pair : Projectors)
	{
		if (Pair.Value)
		{
			Pair.Value->SetPGMEnabled(bEnabled);
		}
	}

	UpdateRawDataRequest();
}

void URammsCameraProjectionManager::SetGPUAccelerated(bool bGPU)
{
	bGPUAccelerated = bGPU;

	for (auto& Pair : Projectors)
	{
		if (Pair.Value)
		{
			Pair.Value->bGPUAccelerated = bGPU;
		}
	}

	UpdateRawDataRequest();
}

void URammsCameraProjectionManager::SetMinDepth(float InMinDepthCM)
{
	MinDepthCM = FMath::Max(0.0f, InMinDepthCM);

	for (auto& Pair : Projectors)
	{
		if (Pair.Value)
		{
			Pair.Value->MinDepthCM = MinDepthCM;
			Pair.Value->RefreshMaterialParameters();
		}
	}
}

void URammsCameraProjectionManager::SetMaxDepth(float InMaxDepthCM)
{
	MaxDepthCM = FMath::Max(0.01f, InMaxDepthCM);

	for (auto& Pair : Projectors)
	{
		if (Pair.Value)
		{
			Pair.Value->MaxDepthCM = MaxDepthCM;
			Pair.Value->RefreshMaterialParameters();
		}
	}
}

void URammsCameraProjectionManager::SetMaxEdgeStretch(float InMaxEdgeStretchCM)
{
	MaxEdgeStretchCM = FMath::Max(0.0f, InMaxEdgeStretchCM);

	for (auto& Pair : Projectors)
	{
		if (Pair.Value)
		{
			Pair.Value->MaxEdgeStretchCM = MaxEdgeStretchCM;
			Pair.Value->RefreshMaterialParameters();
		}
	}
}

void URammsCameraProjectionManager::SetDepthScale(float InDepthScaleToCM)
{
	DepthScaleToCM = FMath::Max(0.0001f, InDepthScaleToCM);

	for (auto& Pair : Projectors)
	{
		if (Pair.Value)
		{
			Pair.Value->DepthScaleToCM = DepthScaleToCM;
			Pair.Value->RefreshMaterialParameters();
		}
	}
}

void URammsCameraProjectionManager::SetDecimation(int32 InDecimation)
{
	Decimation = FMath::Max(1, InDecimation);

	for (auto& Pair : Projectors)
	{
		if (Pair.Value)
		{
			Pair.Value->Decimation = Decimation;
			Pair.Value->RefreshMaterialParameters();
		}
	}
}

void URammsCameraProjectionManager::SetSensorBaseline(float InBaselineY)
{
	SensorBaselineY = InBaselineY;

	for (auto& Pair : Projectors)
	{
		if (Pair.Value)
		{
			Pair.Value->SensorBaselineY = SensorBaselineY;
			Pair.Value->RefreshMaterialParameters();
		}
	}
}

void URammsCameraProjectionManager::SetDepthConfig(float InMinDepthCM, float InMaxDepthCM, float InMaxEdgeStretchCM, float InDepthScaleToCM)
{
	float NewMinDepthCM = MinDepthCM;
	float NewMaxDepthCM = MaxDepthCM;
	float NewMaxEdgeStretchCM = MaxEdgeStretchCM;
	float NewDepthScaleToCM = DepthScaleToCM;

	if (InMinDepthCM >= 0.0f)
		NewMinDepthCM = FMath::Max(0.0f, InMinDepthCM);
	if (InMaxDepthCM >= 0.0f)
		NewMaxDepthCM = FMath::Max(0.01f, InMaxDepthCM);
	if (InMaxEdgeStretchCM >= 0.0f)
		NewMaxEdgeStretchCM = InMaxEdgeStretchCM;
	if (InDepthScaleToCM >= 0.0f)
		NewDepthScaleToCM = FMath::Max(0.0001f, InDepthScaleToCM);

	NewMinDepthCM = FMath::Min(NewMinDepthCM, NewMaxDepthCM);

	MinDepthCM = NewMinDepthCM;
	MaxDepthCM = NewMaxDepthCM;
	MaxEdgeStretchCM = NewMaxEdgeStretchCM;
	DepthScaleToCM = NewDepthScaleToCM;
	for (auto& Pair : Projectors)
	{
		if (Pair.Value)
		{
			Pair.Value->MinDepthCM = MinDepthCM;
			Pair.Value->MaxDepthCM = MaxDepthCM;
			Pair.Value->MaxEdgeStretchCM = MaxEdgeStretchCM;
			Pair.Value->DepthScaleToCM = DepthScaleToCM;
			Pair.Value->RefreshMaterialParameters();
		}
	}
}

void URammsCameraProjectionManager::UpdateRawDataRequest()
{
	const bool bNeedRawData = bEnablePGM && !bGPUAccelerated;

	if (bNeedRawData == bRequestedRawData)
	{
		return;
	}

	URammsStreamCameraBridge* Bridge = GetOwner()
		? GetOwner()->FindComponentByClass<URammsStreamCameraBridge>()
		: nullptr;

	if (!Bridge)
	{
		return;
	}

	if (bNeedRawData)
	{
		Bridge->RequestRawDataForwarding();
		bRequestedRawData = true;
		UE_LOG(LogRammsProjection, Log, TEXT("CPU PGM active — requested raw data forwarding from bridge"));
	}
	else
	{
		Bridge->ReleaseRawDataForwarding();
		bRequestedRawData = false;
		UE_LOG(LogRammsProjection, Log, TEXT("Raw data forwarding released (GPU PGM or PGM disabled)"));
	}
}

// ── Stream Association Helpers ───────────────────────────────────

FString URammsCameraProjectionManager::FindDepthStreamForColor(
	const FString& ColorStreamID, IRammsCameraProvider* Provider) const
{
	if (!Provider)
		return FString();

	FRammsCameraStreamInfo ColorInfo;
	if (!Provider->GetStreamInfo(ColorStreamID, ColorInfo))
		return FString();

	// ── Primary: match by GroupID ──
	if (!ColorInfo.GroupID.IsEmpty())
	{
		TArray<FRammsCameraStreamInfo> AllStreams = Provider->GetAvailableStreams();
		for (const FRammsCameraStreamInfo& S : AllStreams)
		{
			if (S.StreamID != ColorStreamID
				&& S.GroupID == ColorInfo.GroupID
				&& (S.StreamRole == ERammsStreamRole::Depth || S.bIsDepth))
			{
				UE_LOG(LogRammsProjection, Log,
					TEXT("Linked depth '%s' to color '%s' via GroupID '%s'"),
					*S.StreamID, *ColorStreamID, *ColorInfo.GroupID);
				return S.StreamID;
			}
		}
	}

	// ── Fallback (deprecated): channel + 100 convention ──
	int32 SlashIdx = INDEX_NONE;
	if (ColorStreamID.FindLastChar(TEXT('/'), SlashIdx))
	{
		FString Prefix = ColorStreamID.Left(SlashIdx);
		FString ChannelStr = ColorStreamID.Mid(SlashIdx + 1);
		if (ChannelStr.IsNumeric())
		{
			int32	ColorChannel = FCString::Atoi(*ChannelStr);
			int32	DepthChannel = ColorChannel + 100;
			FString PotentialDepthID = FString::Printf(TEXT("%s/%d"), *Prefix, DepthChannel);

			FRammsCameraStreamInfo DepthInfo;
			if (Provider->GetStreamInfo(PotentialDepthID, DepthInfo))
			{
				UE_LOG(LogRammsProjection, Warning,
					TEXT("Linked depth '%s' to color '%s' via deprecated channel+100 convention. "
						 "Please set 'group' and 'role' in stream metadata instead."),
					*PotentialDepthID, *ColorStreamID);
				return PotentialDepthID;
			}
		}
	}

	return FString();
}

FString URammsCameraProjectionManager::FindColorStreamForDepth(
	const FString& DepthStreamID, IRammsCameraProvider* Provider) const
{
	if (!Provider)
		return FString();

	FRammsCameraStreamInfo DepthInfo;
	if (!Provider->GetStreamInfo(DepthStreamID, DepthInfo))
		return FString();

	// ── Primary: match by GroupID ──
	if (!DepthInfo.GroupID.IsEmpty())
	{
		for (auto& Pair : Projectors)
		{
			if (!Pair.Value)
				continue;

			FRammsCameraStreamInfo ColorInfo;
			if (Provider->GetStreamInfo(Pair.Key, ColorInfo)
				&& ColorInfo.GroupID == DepthInfo.GroupID
				&& (ColorInfo.StreamRole == ERammsStreamRole::Color
					|| (ColorInfo.StreamRole == ERammsStreamRole::Other && !ColorInfo.bIsDepth)))
			{
				UE_LOG(LogRammsProjection, Log,
					TEXT("Linked depth '%s' to projector '%s' via GroupID '%s'"),
					*DepthStreamID, *Pair.Key, *DepthInfo.GroupID);
				return Pair.Key;
			}
		}
	}

	// ── Fallback (deprecated): channel - 100 convention ──
	int32 SlashIdx = INDEX_NONE;
	if (DepthStreamID.FindLastChar(TEXT('/'), SlashIdx))
	{
		FString Prefix = DepthStreamID.Left(SlashIdx);
		FString ChannelStr = DepthStreamID.Mid(SlashIdx + 1);
		if (ChannelStr.IsNumeric())
		{
			int32 DepthChannel = FCString::Atoi(*ChannelStr);
			int32 ColorChannel = DepthChannel - 100;
			if (ColorChannel >= 0)
			{
				FString ColorStreamID = FString::Printf(TEXT("%s/%d"), *Prefix, ColorChannel);
				if (Projectors.Contains(ColorStreamID))
				{
					UE_LOG(LogRammsProjection, Warning,
						TEXT("Linked depth '%s' to projector '%s' via deprecated channel-100 convention. "
							 "Please set 'group' and 'role' in stream metadata instead."),
						*DepthStreamID, *ColorStreamID);
					return ColorStreamID;
				}
			}
		}
	}

	return FString();
}
