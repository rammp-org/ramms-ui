// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsCameraProjectorComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
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

	if (PropertyChangedEvent.Property && (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(URammsCameraProjectorComponent, bEnablePGM) || PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(URammsCameraProjectorComponent, PGMMaterial)))
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
	const float HalfWidth = MaxProjectionDistance * (float)ImageWidth / (2.0f * Fx);
	const float HalfHeight = MaxProjectionDistance * (float)ImageHeight / (2.0f * Fy);
	const float HalfDepth = MaxProjectionDistance * 0.5f;

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
	const FVector	  Pos = WorldXform.GetLocation();
	const FVector	  Fwd = WorldXform.GetUnitAxis(EAxis::X);
	const FVector	  Right = WorldXform.GetUnitAxis(EAxis::Y);
	const FVector	  Up = WorldXform.GetUnitAxis(EAxis::Z);

	MaterialInstance->SetVectorParameterValue(FName("CameraWorldPos"),
		FLinearColor(Pos.X, Pos.Y, Pos.Z, 0.0f));
	MaterialInstance->SetVectorParameterValue(FName("CameraForward"),
		FLinearColor(Fwd.X, Fwd.Y, Fwd.Z, 0.0f));
	MaterialInstance->SetVectorParameterValue(FName("CameraRight"),
		FLinearColor(Right.X, Right.Y, Right.Z, 0.0f));
	MaterialInstance->SetVectorParameterValue(FName("CameraUp"),
		FLinearColor(Up.X, Up.Y, Up.Z, 0.0f));
}

// ── Raw data → FLinearColor helpers ────────────────────────────────

namespace
{
	/** Convert raw pixel bytes to an array of FLinearColor, handling different pixel formats. */
	bool ConvertRawToLinearColor(const TArray<uint8>& RawData, EPixelFormat Format,
		int32 Width, int32 Height, TArray<FLinearColor>& OutPixels)
	{
		const int32 PixelCount = Width * Height;
		if (PixelCount <= 0)
			return false;

		OutPixels.SetNumUninitialized(PixelCount);

		switch (Format)
		{
			case PF_B8G8R8A8:
			{
				const int32 Expected = PixelCount * 4;
				if (RawData.Num() < Expected)
					return false;
				const uint8* Bytes = RawData.GetData();
				for (int32 i = 0; i < PixelCount; ++i)
				{
					OutPixels[i] = FLinearColor(
						Bytes[i * 4 + 2] / 255.0f,	// R (stored as BGRA)
						Bytes[i * 4 + 1] / 255.0f,	// G
						Bytes[i * 4 + 0] / 255.0f,	// B
						Bytes[i * 4 + 3] / 255.0f); // A
				}
				return true;
			}
			case PF_R32_FLOAT:
			{
				const int32 Expected = PixelCount * static_cast<int32>(sizeof(float));
				if (RawData.Num() < Expected)
					return false;
				const uint8* Src = RawData.GetData();
				for (int32 i = 0; i < PixelCount; ++i)
				{
					float Val;
					FMemory::Memcpy(&Val, Src + i * sizeof(float), sizeof(float));
					OutPixels[i] = FLinearColor(Val, 0.0f, 0.0f, 1.0f);
				}
				return true;
			}
			case PF_G32R32F:
			{
				const int32 Expected = PixelCount * static_cast<int32>(sizeof(float)) * 2;
				if (RawData.Num() < Expected)
					return false;
				const uint8* Src = RawData.GetData();
				for (int32 i = 0; i < PixelCount; ++i)
				{
					float R, G;
					FMemory::Memcpy(&R, Src + i * 2 * sizeof(float), sizeof(float));
					FMemory::Memcpy(&G, Src + (i * 2 + 1) * sizeof(float), sizeof(float));
					OutPixels[i] = FLinearColor(R, G, 0.0f, 1.0f);
				}
				return true;
			}
			case PF_A32B32G32R32F:
			{
				const int32 Expected = PixelCount * static_cast<int32>(sizeof(float)) * 4;
				if (RawData.Num() < Expected)
					return false;
				const uint8* Src = RawData.GetData();
				for (int32 i = 0; i < PixelCount; ++i)
				{
					FMemory::Memcpy(&OutPixels[i], Src + i * sizeof(FLinearColor), sizeof(FLinearColor));
				}
				return true;
			}
			default:
				return false;
		}
	}
} // namespace

void URammsCameraProjectorComponent::EnsurePGMCreated()
{
	if (ProcMeshComponent)
		return; // already created

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogRammsPGM, Warning, TEXT("EnsurePGMCreated: no owner actor, cannot create ProcMeshComponent"));
		return;
	}

	FName MeshName = MakeUniqueObjectName(Owner, UProceduralMeshComponent::StaticClass(), TEXT("PGMProceduralMesh"));
	ProcMeshComponent = NewObject<UProceduralMeshComponent>(Owner, MeshName);
	ProcMeshComponent->bUseAsyncCooking = false; // sync so mesh is visible immediately
	ProcMeshComponent->SetupAttachment(this);
	ProcMeshComponent->RegisterComponent();
	ProcMeshComponent->SetVisibility(false);
	Owner->AddInstanceComponent(ProcMeshComponent);

	if (PGMMaterial)
	{
		ProcMeshComponent->SetMaterial(0, PGMMaterial);
	}

	UE_LOG(LogRammsPGM, Log, TEXT("EnsurePGMCreated: ProcMeshComponent created on '%s'"), *Owner->GetName());
}

void URammsCameraProjectorComponent::MaybeUpdatePGM()
{
	if (!bEnablePGM)
		return;

	// Only rebuild when BOTH color and depth have been updated since last PGM run
	if (LastColorTimestamp > LastPGMTimestamp && LastDepthTimestamp > LastPGMTimestamp)
	{
		UpdatePGM();
	}
}

void URammsCameraProjectorComponent::UpdatePGM()
{
	if (!bEnablePGM)
	{
		if (ProcMeshComponent)
			ProcMeshComponent->SetVisibility(false);
		return;
	}

	// Require both raw data buffers
	if (DepthRawData.Num() == 0 || ColorRawData.Num() == 0)
	{
		UE_LOG(LogRammsPGM, Log,
			TEXT("UpdatePGM: waiting for raw data (color=%d bytes, depth=%d bytes)"),
			ColorRawData.Num(), DepthRawData.Num());
		return;
	}

	// Check for sync — only enforce if we have real timestamps from both streams
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

	// Convert raw data to FLinearColor arrays using format-aware conversion
	TArray<FLinearColor> ColorPixels;
	TArray<FLinearColor> DepthPixels;

	if (!ConvertRawToLinearColor(ColorRawData, ColorPixelFormat, ColorFrameWidth, ColorFrameHeight, ColorPixels))
	{
		UE_LOG(LogRammsPGM, Warning,
			TEXT("UpdatePGM: failed to convert color data (fmt=%d, %dx%d, %d bytes)"),
			static_cast<int32>(ColorPixelFormat), ColorFrameWidth, ColorFrameHeight, ColorRawData.Num());
		return;
	}
	if (!ConvertRawToLinearColor(DepthRawData, DepthPixelFormat, DepthFrameWidth, DepthFrameHeight, DepthPixels))
	{
		UE_LOG(LogRammsPGM, Warning,
			TEXT("UpdatePGM: failed to convert depth data (fmt=%d, %dx%d, %d bytes)"),
			static_cast<int32>(DepthPixelFormat), DepthFrameWidth, DepthFrameHeight, DepthRawData.Num());
		return;
	}

	const int32 DepthW = DepthFrameWidth;
	const int32 DepthH = DepthFrameHeight;
	const int32 ColorW = ColorFrameWidth;
	const int32 ColorH = ColorFrameHeight;

	UE_LOG(LogRammsPGM, Verbose,
		TEXT("UpdatePGM: color=%dx%d (%d px), depth=%dx%d (%d px)"),
		ColorW, ColorH, ColorPixels.Num(), DepthW, DepthH, DepthPixels.Num());

	// Sample depth range for diagnostics (first update only)
	if (LastPGMTimestamp == 0)
	{
		float MinD = FLT_MAX, MaxD = -FLT_MAX, SumD = 0;
		int32 ValidCount = 0;
		for (int32 i = 0; i < DepthPixels.Num(); ++i)
		{
			float D = DepthPixels[i].R;
			if (D > 0.0f)
			{
				MinD = FMath::Min(MinD, D);
				MaxD = FMath::Max(MaxD, D);
				SumD += D;
				++ValidCount;
			}
		}
		UE_LOG(LogRammsPGM, Log,
			TEXT("UpdatePGM depth diagnostics: raw range [%.4f, %.4f], mean=%.4f, validPx=%d/%d, DepthScaleToCM=%.4f → cm range [%.1f, %.1f]"),
			MinD, MaxD, ValidCount > 0 ? SumD / ValidCount : 0.0f,
			ValidCount, DepthPixels.Num(), DepthScaleToCM,
			MinD * DepthScaleToCM, MaxD * DepthScaleToCM);
		UE_LOG(LogRammsPGM, Log,
			TEXT("UpdatePGM intrinsics: fx=%.1f fy=%.1f cx=%.1f cy=%.1f, image=%dx%d, depthFmt=%d, colorFmt=%d"),
			FocalLengthX, FocalLengthY, PrincipalPointX, PrincipalPointY,
			ImageWidth, ImageHeight,
			static_cast<int32>(DepthPixelFormat), static_cast<int32>(ColorPixelFormat));
	}

	if (DepthW <= 0 || DepthH <= 0 || DepthPixels.Num() < (DepthW * DepthH))
	{
		UE_LOG(LogRammsPGM, Warning, TEXT("UpdatePGM: depth data invalid (%dx%d, %d pixels)"), DepthW, DepthH, DepthPixels.Num());
		return;
	}

	// Porting deprojection logic from PGM_Display.cpp
	TArray<FVector>			 Vertices;
	TArray<int32>			 Triangles;
	TArray<FVector>			 Normals;
	TArray<FVector2D>		 UV0;
	TArray<FLinearColor>	 VertexColors;
	TArray<FProcMeshTangent> Tangents;

	int32 Stride = FMath::Max(1, Decimation);
	int32 GridWidth = DepthW / Stride;
	int32 GridHeight = DepthH / Stride;

	// Flat grid-to-vertex index (INDEX_NONE = no vertex at this grid cell)
	TArray<int32> GridToVertex;
	GridToVertex.SetNumUninitialized(GridWidth * GridHeight);
	FMemory::Memset(GridToVertex.GetData(), 0xFF, GridToVertex.Num() * sizeof(int32)); // INDEX_NONE = -1

	float Fx = FocalLengthX;
	float Fy = FocalLengthY;
	float Cx = PrincipalPointX;
	float Cy = PrincipalPointY;

	// Adjust intrinsics if depth texture resolution differs from calibrated resolution
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

			float Z = DepthPixels[Index].R * DepthScaleToCM;

			if (Z < MinDepthCM || Z > MaxDepthCM)
				continue;

			// Deprojection math (Unreal space)
			float X_cam = Z;
			float Y_cam = ((SrcX - Cx) * Z) / Fx;
			float Z_cam = ((Cy - SrcY) * Z) / Fy;

			// Parallax Color Sampling
			float Y_rgb_world = Y_cam - SensorBaselineY;

			int32 u_color = FMath::RoundToInt(((Y_rgb_world * FocalLengthX) / X_cam) + PrincipalPointX);
			int32 v_color = FMath::RoundToInt(PrincipalPointY - ((Z_cam * FocalLengthY) / X_cam));

			u_color = FMath::Clamp(u_color, 0, ColorW - 1);
			v_color = FMath::Clamp(v_color, 0, ColorH - 1);
			FLinearColor SampledColor = ColorPixels[(v_color * ColorW) + u_color];

			GridToVertex[y * GridWidth + x] = Vertices.Num();
			Vertices.Add(FVector(X_cam, Y_cam, Z_cam));
			VertexColors.Add(SampledColor);
			UV0.Add(FVector2D((float)SrcX / DepthW, (float)SrcY / DepthH));
		}
	}

	// 2. Generate Triangles (Connecting the projective grid)
	for (int32 y = 0; y < GridHeight - 1; ++y)
	{
		for (int32 x = 0; x < GridWidth - 1; ++x)
		{
			const int32 i0 = GridToVertex[y * GridWidth + x];
			const int32 i1 = GridToVertex[y * GridWidth + (x + 1)];
			const int32 i2 = GridToVertex[(y + 1) * GridWidth + x];
			const int32 i3 = GridToVertex[(y + 1) * GridWidth + (x + 1)];

			if (i0 == INDEX_NONE || i1 == INDEX_NONE || i2 == INDEX_NONE || i3 == INDEX_NONE)
				continue;

			const FVector& p0 = Vertices[i0];
			const FVector& p1 = Vertices[i1];
			const FVector& p2 = Vertices[i2];
			const FVector& p3 = Vertices[i3];

			if (FVector::Dist(p0, p1) < MaxEdgeStretchCM && FVector::Dist(p0, p2) < MaxEdgeStretchCM && FVector::Dist(p1, p3) < MaxEdgeStretchCM && FVector::Dist(p2, p3) < MaxEdgeStretchCM)
			{
				Triangles.Add(i0);
				Triangles.Add(i2);
				Triangles.Add(i1);

				Triangles.Add(i1);
				Triangles.Add(i2);
				Triangles.Add(i3);
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

	LastPGMTimestamp = FMath::Max(LastColorTimestamp, LastDepthTimestamp);
}

// ── GPU Readback Fallback ─────────────────────────────────────────

bool URammsCameraProjectorComponent::TryReadTextureToRawData(
	UTexture*	   Texture,
	TArray<uint8>& OutRawData, EPixelFormat& OutFormat,
	int32& OutWidth, int32& OutHeight)
{
	if (!Texture)
		return false;

	// --- UTextureRenderTarget2D (scene capture render targets) ---
	if (UTextureRenderTarget2D* RT = Cast<UTextureRenderTarget2D>(Texture))
	{
		FTextureRenderTargetResource* RTResource = RT->GameThread_GetRenderTargetResource();
		if (!RTResource)
			return false;

		OutWidth = RT->SizeX;
		OutHeight = RT->SizeY;
		const EPixelFormat RTFormat = RT->GetFormat();

		// Float formats: use ReadLinearColorPixels for full precision (depth data)
		if (RTFormat == PF_FloatRGBA || RTFormat == PF_R32_FLOAT || RTFormat == PF_R16F || RTFormat == PF_A32B32G32R32F || RTFormat == PF_G32R32F)
		{
			TArray<FLinearColor> Pixels;
			if (RTResource->ReadLinearColorPixels(Pixels))
			{
				OutFormat = PF_A32B32G32R32F;
				OutRawData.SetNumUninitialized(Pixels.Num() * sizeof(FLinearColor));
				FMemory::Memcpy(OutRawData.GetData(), Pixels.GetData(), OutRawData.Num());
				return true;
			}
		}

		// Standard 8-bit formats
		TArray<FColor> Pixels;
		if (RTResource->ReadPixels(Pixels))
		{
			OutFormat = PF_B8G8R8A8;
			OutRawData.SetNumUninitialized(Pixels.Num() * 4);
			FMemory::Memcpy(OutRawData.GetData(), Pixels.GetData(), OutRawData.Num());
			return true;
		}
		return false;
	}

	// --- UTexture2D (streaming sink textures) ---
	if (UTexture2D* Tex2D = Cast<UTexture2D>(Texture))
	{
		OutFormat = Tex2D->GetPixelFormat();
		OutWidth = Tex2D->GetSizeX();
		OutHeight = Tex2D->GetSizeY();

		if (Tex2D->GetPlatformData() && Tex2D->GetPlatformData()->Mips.Num() > 0)
		{
			FTexture2DMipMap& Mip = Tex2D->GetPlatformData()->Mips[0];
			const int64		  BulkSize = Mip.BulkData.GetBulkDataSize();
			if (BulkSize > 0)
			{
				const void* Ptr = Mip.BulkData.Lock(LOCK_READ_ONLY);
				if (Ptr)
				{
					OutRawData.SetNumUninitialized(static_cast<int32>(BulkSize));
					FMemory::Memcpy(OutRawData.GetData(), Ptr, BulkSize);
					Mip.BulkData.Unlock();
					return true;
				}
				Mip.BulkData.Unlock();
			}
		}
		return false;
	}

	return false;
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
	if (!Texture)
		return;

	CurrentColorTexture = Texture;
	LastColorTimestamp = Timestamp;

	if (MaterialInstance)
	{
		MaterialInstance->SetTextureParameterValue(FName("CameraTexture"), Texture);
	}

	// PGM fallback: GPU readback when no raw data was provided via SetColorTextureWithData
	if (bEnablePGM)
	{
		ColorRawData.Reset();
		if (!TryReadTextureToRawData(Texture, ColorRawData, ColorPixelFormat, ColorFrameWidth, ColorFrameHeight))
		{
			UE_LOG(LogRammsPGM, Warning, TEXT("SetColorTexture: GPU readback failed for PGM"));
		}
	}

	MaybeUpdatePGM();
}

void URammsCameraProjectorComponent::SetColorTextureWithData(
	UTexture* Texture, int64 Timestamp,
	TConstArrayView<uint8> RawData, EPixelFormat Format, int32 Width, int32 Height)
{
	if (!Texture)
		return;

	CurrentColorTexture = Texture;
	LastColorTimestamp = Timestamp;
	ColorRawData.SetNumUninitialized(RawData.Num());
	FMemory::Memcpy(ColorRawData.GetData(), RawData.GetData(), RawData.Num());
	ColorPixelFormat = Format;
	ColorFrameWidth = Width;
	ColorFrameHeight = Height;

	if (MaterialInstance)
	{
		MaterialInstance->SetTextureParameterValue(FName("CameraTexture"), Texture);
	}

	MaybeUpdatePGM();
}

void URammsCameraProjectorComponent::SetDepthTexture(UTexture* Texture, int64 Timestamp)
{
	if (!Texture)
		return;

	CurrentDepthTexture = Texture;
	LastDepthTimestamp = Timestamp;

	// PGM fallback: GPU readback when no raw data was provided
	if (bEnablePGM)
	{
		DepthRawData.Reset();
		if (!TryReadTextureToRawData(Texture, DepthRawData, DepthPixelFormat, DepthFrameWidth, DepthFrameHeight))
		{
			UE_LOG(LogRammsPGM, Warning, TEXT("SetDepthTexture: GPU readback failed for PGM"));
		}
	}

	MaybeUpdatePGM();
}

void URammsCameraProjectorComponent::SetDepthTextureWithData(
	UTexture* Texture, int64 Timestamp,
	TConstArrayView<uint8> RawData, EPixelFormat Format, int32 Width, int32 Height)
{
	if (!Texture)
		return;

	CurrentDepthTexture = Texture;
	LastDepthTimestamp = Timestamp;
	DepthRawData.SetNumUninitialized(RawData.Num());
	FMemory::Memcpy(DepthRawData.GetData(), RawData.GetData(), RawData.Num());
	DepthPixelFormat = Format;
	DepthFrameWidth = Width;
	DepthFrameHeight = Height;

	MaybeUpdatePGM();
}

void URammsCameraProjectorComponent::SetIntrinsicsFromStreamInfo(const FRammsCameraStreamInfo& StreamInfo)
{
	ImageWidth = StreamInfo.Width;
	ImageHeight = StreamInfo.Height;

	if (StreamInfo.Intrinsics.Num() >= 4)
	{
		FocalLengthX = StreamInfo.Intrinsics[0];
		FocalLengthY = StreamInfo.Intrinsics[1];
		PrincipalPointX = StreamInfo.Intrinsics[2];
		PrincipalPointY = StreamInfo.Intrinsics[3];
	}
	else
	{
		// Fallback: assume ~90° FOV centered
		PrincipalPointX = (float)ImageWidth * 0.5f;
		PrincipalPointY = (float)ImageHeight * 0.5f;
		FocalLengthX = PrincipalPointX;
		FocalLengthY = PrincipalPointY;
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
	// Use only location and rotation from the extrinsic — ignore scale so that
	// a scaled camera actor (e.g. 0.1 for a small preview mesh) doesn't shrink
	// the projected geometry.
	FTransform Unscaled(WorldTransform.GetRotation(), WorldTransform.GetLocation(), FVector::OneVector);
	SetWorldTransform(Unscaled);
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
