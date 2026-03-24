// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsCameraProjectorComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "TextureResource.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"

DEFINE_LOG_CATEGORY_STATIC(LogRammsPGM, Log, All);

URammsCameraProjectorComponent::URammsCameraProjectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// ProcMeshComponent is created lazily in EnsurePGMCreated() because
	// this component is often instantiated via NewObject (not SpawnActor),
	// which means CreateDefaultSubobject would silently fail at runtime.
}

void URammsCameraProjectorComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureDecalCreated();
}

#if WITH_EDITOR
void URammsCameraProjectorComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateDecalSize();
	UpdateMaterialParameters();
	
	if (PropertyChangedEvent.Property && 
		(PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(URammsCameraProjectorComponent, bEnablePGM) ||
		 PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(URammsCameraProjectorComponent, PGMMaterial)))
	{
		UpdatePGM();
	}
}
#endif

// ── Internal ───────────────────────────────────────────────────────

void URammsCameraProjectorComponent::EnsureDecalCreated()
{
	if (DecalComponent || !ProjectionMaterial || !GetOwner())
		return;

	DecalComponent = NewObject<UDecalComponent>(GetOwner());
	DecalComponent->SetupAttachment(this);
	DecalComponent->SetFadeScreenSize(0.0f); // never fade by screen size
	DecalComponent->RegisterComponent();

	MaterialInstance = UMaterialInstanceDynamic::Create(ProjectionMaterial, this);
	DecalComponent->SetDecalMaterial(MaterialInstance);

	UpdateDecalSize();
	UpdateMaterialParameters();
}

void URammsCameraProjectorComponent::UpdateDecalSize()
{
	if (!DecalComponent)
		return;

	const float Fx = FMath::Max(FocalLengthX, 1.0f);
	const float Fy = FMath::Max(FocalLengthY, 1.0f);

	// Conservative half-extents of the frustum at max distance
	const float HalfWidth  = MaxProjectionDistance * (float)ImageWidth  / (2.0f * Fx);
	const float HalfHeight = MaxProjectionDistance * (float)ImageHeight / (2.0f * Fy);
	const float HalfDepth  = MaxProjectionDistance * 0.5f;

	// DecalSize = half-extents (X=depth, Y=width, Z=height)
	DecalComponent->DecalSize = FVector(HalfDepth, HalfWidth, HalfHeight);

	// Offset the decal so the near face starts at the camera position
	DecalComponent->SetRelativeLocation(FVector(HalfDepth, 0.0f, 0.0f));
}

void URammsCameraProjectorComponent::UpdateMaterialParameters()
{
	if (!MaterialInstance)
		return;

	MaterialInstance->SetVectorParameterValue(
		FName("Intrinsics"),
		FLinearColor(FocalLengthX, FocalLengthY, PrincipalPointX, PrincipalPointY));

	MaterialInstance->SetVectorParameterValue(
		FName("ImageSize"),
		FLinearColor((float)ImageWidth, (float)ImageHeight, 0.0f, 0.0f));

	MaterialInstance->SetScalarParameterValue(FName("FadeWidth"), FadeWidth);
	MaterialInstance->SetScalarParameterValue(FName("TargetStencil"), (float)TargetStencilValue);

	UpdateCameraTransformParameters();
}

void URammsCameraProjectorComponent::UpdateCameraTransformParameters()
{
	if (!MaterialInstance)
		return;

	const FTransform& WorldXform = GetComponentTransform();
	const FVector Pos = WorldXform.GetLocation();
	const FVector Fwd = WorldXform.GetUnitAxis(EAxis::X);
	const FVector Right = WorldXform.GetUnitAxis(EAxis::Y);
	const FVector Up = WorldXform.GetUnitAxis(EAxis::Z);

	MaterialInstance->SetVectorParameterValue(FName("CameraWorldPos"),
		FLinearColor(Pos.X, Pos.Y, Pos.Z, 0.0f));
	MaterialInstance->SetVectorParameterValue(FName("CameraForward"),
		FLinearColor(Fwd.X, Fwd.Y, Fwd.Z, 0.0f));
	MaterialInstance->SetVectorParameterValue(FName("CameraRight"),
		FLinearColor(Right.X, Right.Y, Right.Z, 0.0f));
	MaterialInstance->SetVectorParameterValue(FName("CameraUp"),
		FLinearColor(Up.X, Up.Y, Up.Z, 0.0f));
}

static bool ReadTextureData(UTexture* Texture, TArray<FLinearColor>& OutData, int32& OutWidth, int32& OutHeight)
{
	if (!Texture) return false;

	// Path 1: Render Target — most reliable for live streams
	if (UTextureRenderTarget2D* RT = Cast<UTextureRenderTarget2D>(Texture))
	{
		FTextureRenderTargetResource* Resource = RT->GameThread_GetRenderTargetResource();
		if (!Resource)
		{
			UE_LOG(LogRammsPGM, Warning, TEXT("ReadTextureData: RenderTarget has no resource for '%s'"), *Texture->GetName());
			return false;
		}
		OutWidth = RT->SizeX;
		OutHeight = RT->SizeY;
		bool bOk = Resource->ReadLinearColorPixels(OutData);
		if (!bOk)
		{
			UE_LOG(LogRammsPGM, Warning, TEXT("ReadTextureData: ReadLinearColorPixels failed for RT '%s'"), *Texture->GetName());
		}
		return bOk;
	}

	// Path 2: UTexture2D — BulkData may be unavailable after UpdateResource() uploads
	// to GPU, so we use the texture's platform data only if it's still CPU-accessible.
	if (UTexture2D* Tex2D = Cast<UTexture2D>(Texture))
	{
		OutWidth = Tex2D->GetSizeX();
		OutHeight = Tex2D->GetSizeY();

		if (!Tex2D->GetPlatformData() || Tex2D->GetPlatformData()->Mips.Num() == 0)
		{
			UE_LOG(LogRammsPGM, Warning, TEXT("ReadTextureData: Texture2D '%s' has no platform mip data"), *Texture->GetName());
			return false;
		}

		FTexture2DMipMap& Mip = Tex2D->GetPlatformData()->Mips[0];

		// Only attempt if BulkData is loaded (it may have been discarded after GPU upload)
		if (!Mip.BulkData.IsBulkDataLoaded())
		{
			UE_LOG(LogRammsPGM, Warning,
				TEXT("ReadTextureData: Texture2D '%s' BulkData is not loaded (already GPU-uploaded). "
					 "Consider using a UTextureRenderTarget2D for depth streams."),
				*Texture->GetName());
			return false;
		}

		void* DataPtr = Mip.BulkData.Lock(LOCK_READ_ONLY);
		if (!DataPtr)
		{
			UE_LOG(LogRammsPGM, Warning, TEXT("ReadTextureData: BulkData lock returned null for '%s'"), *Texture->GetName());
			Mip.BulkData.Unlock();
			return false;
		}

		int32 PixelCount = OutWidth * OutHeight;
		OutData.SetNumUninitialized(PixelCount);
		// Provider uses PF_B8G8R8A8 — bytes are laid out B, G, R, A
		uint8* Bytes = static_cast<uint8*>(DataPtr);
		for (int32 i = 0; i < PixelCount; ++i)
		{
			OutData[i] = FLinearColor(
				Bytes[i * 4 + 2] / 255.0f, // R
				Bytes[i * 4 + 1] / 255.0f, // G
				Bytes[i * 4 + 0] / 255.0f, // B
				Bytes[i * 4 + 3] / 255.0f  // A
			);
		}
		Mip.BulkData.Unlock();
		return true;
	}

	UE_LOG(LogRammsPGM, Warning, TEXT("ReadTextureData: Unsupported texture type for '%s'"), *Texture->GetName());
	return false;
}

void URammsCameraProjectorComponent::EnsurePGMCreated()
{
	if (ProcMeshComponent) return; // already created

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogRammsPGM, Warning, TEXT("EnsurePGMCreated: no owner actor, cannot create ProcMeshComponent"));
		return;
	}

	ProcMeshComponent = NewObject<UProceduralMeshComponent>(Owner, TEXT("PGMProceduralMesh"));
	ProcMeshComponent->bUseAsyncCooking = false; // sync so mesh is visible immediately
	ProcMeshComponent->SetupAttachment(this);
	ProcMeshComponent->RegisterComponent();
	ProcMeshComponent->SetVisibility(false);

	if (PGMMaterial)
	{
		ProcMeshComponent->SetMaterial(0, PGMMaterial);
	}

	UE_LOG(LogRammsPGM, Log, TEXT("EnsurePGMCreated: ProcMeshComponent created on '%s'"), *Owner->GetName());
}

void URammsCameraProjectorComponent::UpdatePGM()
{
	if (!bEnablePGM)
	{
		if (ProcMeshComponent) ProcMeshComponent->SetVisibility(false);
		return;
	}

	if (!CurrentColorTexture || !CurrentDepthTexture)
	{
		UE_LOG(LogRammsPGM, Verbose,
			TEXT("UpdatePGM: waiting for both textures (color=%s, depth=%s)"),
			CurrentColorTexture ? *CurrentColorTexture->GetName() : TEXT("null"),
			CurrentDepthTexture ? *CurrentDepthTexture->GetName() : TEXT("null"));
		return;
	}

	// 1. Check for sync — only enforce if we have real timestamps from both streams
	if (LastColorTimestamp != 0 && LastDepthTimestamp != 0)
	{
		int64 DeltaTicks = FMath::Abs(LastColorTimestamp - LastDepthTimestamp);
		// FDateTime ticks are 100-nanosecond intervals → divide by 10000 for milliseconds
		float DeltaMS = static_cast<float>(DeltaTicks) / 10000.0f;
		
		if (DeltaMS > SyncThresholdMS)
		{
			UE_LOG(LogRammsPGM, Verbose,
				TEXT("UpdatePGM: frames out of sync (%.1f ms > %.1f ms threshold), skipping"),
				DeltaMS, SyncThresholdMS);
			return;
		}
	}

	EnsurePGMCreated();

	if (!ProcMeshComponent)
	{
		UE_LOG(LogRammsPGM, Error, TEXT("UpdatePGM: ProcMeshComponent is null after EnsurePGMCreated"));
		return;
	}

	TArray<FLinearColor> ColorPixels;
	TArray<FLinearColor> DepthPixels;
	int32 ColorW = 0, ColorH = 0, DepthW = 0, DepthH = 0;

	if (!ReadTextureData(CurrentColorTexture, ColorPixels, ColorW, ColorH))
	{
		UE_LOG(LogRammsPGM, Warning, TEXT("UpdatePGM: failed to read color texture '%s'"), *CurrentColorTexture->GetName());
		return;
	}
	if (!ReadTextureData(CurrentDepthTexture, DepthPixels, DepthW, DepthH))
	{
		UE_LOG(LogRammsPGM, Warning, TEXT("UpdatePGM: failed to read depth texture '%s'"), *CurrentDepthTexture->GetName());
		return;
	}

	UE_LOG(LogRammsPGM, Verbose,
		TEXT("UpdatePGM: color=%dx%d (%d px), depth=%dx%d (%d px)"),
		ColorW, ColorH, ColorPixels.Num(), DepthW, DepthH, DepthPixels.Num());

	// Depth drives the mesh — color dimensions may differ
	if (DepthW <= 0 || DepthH <= 0 || DepthPixels.Num() < (DepthW * DepthH))
	{
		UE_LOG(LogRammsPGM, Warning, TEXT("UpdatePGM: depth data invalid (%dx%d, %d pixels)"), DepthW, DepthH, DepthPixels.Num());
		return;
	}

	// Porting deprojection logic from PGM_Display.cpp
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	TMap<FIntPoint, int32> GridToVertexMap;

	int32 Stride = FMath::Max(1, Decimation);
	int32 GridWidth = DepthW / Stride;
	int32 GridHeight = DepthH / Stride;

	float Fx = FocalLengthX;
	float Fy = FocalLengthY;
	float Cx = PrincipalPointX;
	float Cy = PrincipalPointY;

	// Adjust intrinsics based on depth scale vs projector calibrated scale if needed?
	// Projector's FocalLengthX/Y are assumed for the Color texture.
	// If Depth texture is different resolution, we scale them.
	float DepthScaleX = (float)DepthW / (float)ImageWidth;
	float DepthScaleY = (float)DepthH / (float)ImageHeight;
	Fx *= DepthScaleX;
	Fy *= DepthScaleY;
	Cx *= DepthScaleX;
	Cy *= DepthScaleY;

	// 1. Generate Vertices
	for (int32 y = 0; y < GridHeight; ++y)
	{
		for (int32 x = 0; x < GridWidth; ++x)
		{
			int32 SrcX = x * Stride;
			int32 SrcY = y * Stride;
			int32 Index = (SrcY * DepthW) + SrcX;

			// PGM_Display logic: Red channel contains depth
			float Z = DepthPixels[Index].R * DepthScaleToCM;
			
			if (Z < MinDepthCM || Z > MaxDepthCM)
				continue;

			// Deprojection math (Unreal space)
			float X_cam = Z;
			float Y_cam = ((SrcX - Cx) * Z) / Fx;
			float Z_cam = ((Cy - SrcY) * Z) / Fy; 

			// Parallax Color Sampling
			float Y_rgb_world = Y_cam - SensorBaselineY;
			
			// Map Y_rgb_world back to pixel space in Color Texture
			// We need to use the COLOR intrinsics (the original ones)
			int32 u_color = FMath::RoundToInt(((Y_rgb_world * FocalLengthX) / X_cam) + PrincipalPointX);
			int32 v_color = FMath::RoundToInt(PrincipalPointY - ((Z_cam * FocalLengthY) / X_cam));

			u_color = FMath::Clamp(u_color, 0, ColorW - 1);
			v_color = FMath::Clamp(v_color, 0, ColorH - 1);
			FLinearColor SampledColor = ColorPixels[(v_color * ColorW) + u_color];

			Vertices.Add(FVector(X_cam, Y_cam, Z_cam));
			VertexColors.Add(SampledColor);
			UV0.Add(FVector2D((float)SrcX / DepthW, (float)SrcY / DepthH));

			GridToVertexMap.Add(FIntPoint(x, y), Vertices.Num() - 1);
		}
	}

	// 2. Generate Triangles (Connecting the projective grid)
	for (int32 y = 0; y < GridHeight - 1; ++y)
	{
		for (int32 x = 0; x < GridWidth - 1; ++x)
		{
			int32* v0 = GridToVertexMap.Find(FIntPoint(x, y));
			int32* v1 = GridToVertexMap.Find(FIntPoint(x + 1, y));
			int32* v2 = GridToVertexMap.Find(FIntPoint(x, y + 1));
			int32* v3 = GridToVertexMap.Find(FIntPoint(x + 1, y + 1));

			if (v0 && v1 && v2 && v3)
			{
				FVector p0 = Vertices[*v0];
				FVector p1 = Vertices[*v1];
				FVector p2 = Vertices[*v2];
				FVector p3 = Vertices[*v3];

				if (FVector::Dist(p0, p1) < MaxEdgeStretchCM &&
					FVector::Dist(p0, p2) < MaxEdgeStretchCM &&
					FVector::Dist(p1, p3) < MaxEdgeStretchCM && FVector::Dist(p2, p3) < MaxEdgeStretchCM)
				{
					Triangles.Add(*v0);
					Triangles.Add(*v2);
					Triangles.Add(*v1);

					Triangles.Add(*v1);
					Triangles.Add(*v2);
					Triangles.Add(*v3);
				}
			}
		}
	}

	Normals.Init(FVector(1, 0, 0), Vertices.Num());

	UE_LOG(LogRammsPGM, Log,
		TEXT("UpdatePGM: generated %d vertices, %d triangles (grid %dx%d, stride %d)"),
		Vertices.Num(), Triangles.Num() / 3, GridWidth, GridHeight, Stride);

	ProcMeshComponent->ClearAllMeshSections();
	if (Vertices.Num() > 0 && Triangles.Num() > 0)
	{
		ProcMeshComponent->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, false);
		ProcMeshComponent->SetVisibility(true);

		if (PGMMaterial)
		{
			ProcMeshComponent->SetMaterial(0, PGMMaterial);
		}
	}
	else
	{
		UE_LOG(LogRammsPGM, Warning,
			TEXT("UpdatePGM: no geometry produced — check MinDepthCM (%.0f), MaxDepthCM (%.0f), DepthScaleToCM (%.4f)"),
			MinDepthCM, MaxDepthCM, DepthScaleToCM);
	}
}

// ── Public API ─────────────────────────────────────────────────────

void URammsCameraProjectorComponent::SetCameraTexture(UTexture* Texture)
{
	EnsureDecalCreated();
	if (MaterialInstance)
	{
		MaterialInstance->SetTextureParameterValue(FName("CameraTexture"), Texture);
	}
	
	// Legacy: Just treat this as a color frame with current timestamp
	SetColorTexture(Texture, FDateTime::UtcNow().GetTicks());
}

void URammsCameraProjectorComponent::SetColorTexture(UTexture* Texture, int64 Timestamp)
{
	if (!Texture) return;

	CurrentColorTexture = Texture;
	LastColorTimestamp = Timestamp;

	// Also update decal
	if (MaterialInstance)
	{
		MaterialInstance->SetTextureParameterValue(FName("CameraTexture"), Texture);
	}

	UpdatePGM();
}

void URammsCameraProjectorComponent::SetDepthTexture(UTexture* Texture, int64 Timestamp)
{
	if (!Texture) return;

	CurrentDepthTexture = Texture;
	LastDepthTimestamp = Timestamp;

	UpdatePGM();
}

void URammsCameraProjectorComponent::SetIntrinsicsFromStreamInfo(const FRammsCameraStreamInfo& StreamInfo)
{
	ImageWidth  = StreamInfo.Width;
	ImageHeight = StreamInfo.Height;

	if (StreamInfo.Intrinsics.Num() >= 4)
	{
		FocalLengthX    = StreamInfo.Intrinsics[0];
		FocalLengthY    = StreamInfo.Intrinsics[1];
		PrincipalPointX = StreamInfo.Intrinsics[2];
		PrincipalPointY = StreamInfo.Intrinsics[3];
	}
	else
	{
		// Fallback: assume ~90° FOV centered
		PrincipalPointX = (float)ImageWidth  * 0.5f;
		PrincipalPointY = (float)ImageHeight * 0.5f;
		FocalLengthX    = PrincipalPointX;
		FocalLengthY    = PrincipalPointY;
	}

	// Apply extrinsic if provided
	if (StreamInfo.bHasExtrinsic)
	{
		SetCameraTransform(StreamInfo.Extrinsic);
	}

	UpdateDecalSize();
	UpdateMaterialParameters();
}

void URammsCameraProjectorComponent::SetCameraTransform(const FTransform& WorldTransform)
{
	SetWorldTransform(WorldTransform);
	UpdateCameraTransformParameters();
}

void URammsCameraProjectorComponent::SetProjectionEnabled(bool bEnabled)
{
	if (DecalComponent)
	{
		DecalComponent->SetVisibility(bEnabled);
	}

	if (ProcMeshComponent)
	{
		ProcMeshComponent->SetVisibility(bEnabled && bEnablePGM);
	}
}

void URammsCameraProjectorComponent::RefreshMaterialParameters()
{
	UpdateDecalSize();
	UpdateMaterialParameters();
	UpdatePGM();
}
