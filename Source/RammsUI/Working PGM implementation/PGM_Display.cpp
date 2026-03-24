#include "PGM_Display.h"
#include "TextureResource.h"
#include "IntrinsicSceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"

APGM_Display::APGM_Display()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create root component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	// Create the procedural mesh
	ProcMeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	ProcMeshComp->SetupAttachment(RootComponent);
	// We want to use complex as simple collision if needed, but for display, false is faster
	ProcMeshComp->bUseAsyncCooking = true;
}

void APGM_Display::BeginPlay()
{
	Super::BeginPlay();

	if (VertexColorMaterial)
	{
		ProcMeshComp->SetMaterial(0, VertexColorMaterial);
	}
}

void APGM_Display::CalculateIntrinsics(UCaptureComponent* CaptureComp, int32 Width, int32 Height, float& outFx, float& outFy, float& outCx, float& outCy)
{
	// Default guess
	outCx = Width / 2.0f;
	outCy = Height / 2.0f;
	float FovRad = FMath::DegreesToRadians(87.0f);
	outFx = outCx / FMath::Tan(FovRad / 2.0f);
	outFy = outFx;

	// Try to get actual intrinsics from the plugin's camera if available
	if (CaptureComp && CaptureComp->RgbCameras.Num() > 0)
	{
		UIntrinsicSceneCaptureComponent2D* IntrinsicCamera = Cast<UIntrinsicSceneCaptureComponent2D>(CaptureComp->RgbCameras[0]);
		if (IntrinsicCamera)
		{
			FCameraIntrinsics Intrinsics = IntrinsicCamera->GetActiveIntrinsics();
			outFx = Intrinsics.FocalLengthX;
			outFy = Intrinsics.FocalLengthY;
			outCx = Intrinsics.PrincipalPointX;
			outCy = Intrinsics.PrincipalPointY;
		}
	}
}

void APGM_Display::UpdateDisplay()
{
	if (!CaptureSourceActor)
	{
		UE_LOG(LogTemp, Error, TEXT("PGM_Display: CaptureSourceActor is not set!"));
		return;
	}

	UCaptureComponent* CaptureComp = CaptureSourceActor->FindComponentByClass<UCaptureComponent>();
	if (!CaptureComp)
	{
		UE_LOG(LogTemp, Error, TEXT("PGM_Display: Could not find UCaptureComponent on the Source Actor."));
		return;
	}

	if (CaptureComp->DmvTextures.Num() == 0 || CaptureComp->RgbTextures.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PGM_Display: CaptureComponent does not have DmvTextures or RgbTextures initialized yet."));
		return;
	}

	UTextureRenderTarget2D* DepthRT = CaptureComp->DmvTextures[0];
	UTextureRenderTarget2D* RGBRT = CaptureComp->RgbTextures[0];

	if (!DepthRT || !RGBRT) return;

	FTextureRenderTargetResource* DepthResource = DepthRT->GameThread_GetRenderTargetResource();
	FTextureRenderTargetResource* RGBResource = RGBRT->GameThread_GetRenderTargetResource();

	if (!DepthResource || !RGBResource) return;

	TArray<FLinearColor> DepthData;
	TArray<FLinearColor> RGBData;

	if (!DepthResource->ReadLinearColorPixels(DepthData) || !RGBResource->ReadLinearColorPixels(RGBData))
	{
		UE_LOG(LogTemp, Error, TEXT("PGM_Display: Failed to read pixels from the Render Targets."));
		return;
	}

	int32 SourceWidth = DepthRT->SizeX;
	int32 SourceHeight = DepthRT->SizeY;

	if (SourceWidth <= 0 || SourceHeight <= 0 || DepthData.Num() < (SourceWidth * SourceHeight))
		return;

	// Prepare procedural mesh buffers
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents; // Empty tangents are fine

	// Create a fast lookup map: maps 2D grid index to the generated 1D vertex array index
	TMap<FIntPoint, int32> GridToVertexMap;

	// Subsampling stride
	int32 Stride = FMath::Max(1, Decimation);
	int32 GridWidth = SourceWidth / Stride;
	int32 GridHeight = SourceHeight / Stride;

	// Pre-allocate memory based on estimate
	int32 EstimatedVertices = GridWidth * GridHeight;
	Vertices.Reserve(EstimatedVertices);
	VertexColors.Reserve(EstimatedVertices);
	UV0.Reserve(EstimatedVertices);

	float Fx, Fy, Cx, Cy;
	CalculateIntrinsics(CaptureComp, SourceWidth, SourceHeight, Fx, Fy, Cx, Cy);

	// 1. Generate Vertices
	for (int32 y = 0; y < GridHeight; ++y)
	{
		for (int32 x = 0; x < GridWidth; ++x)
		{
			int32 SrcX = x * Stride;
			int32 SrcY = y * Stride;
			int32 Index = (SrcY * SourceWidth) + SrcX;

			float Z = DepthData[Index].R * DepthScaleToCM;
			
			// Assume standard Unreal metric system mapping. If units are off, multiply Z here.
			if (Z < MinDepthCM || Z > MaxDepthCM)
				continue;

			// Deprojection math (Left-Handed Unreal Coordinate System Translation)
			// Depth is X forward, Y is right, Z is up (actually camera down is -Z, let's map it safely)
			float X_cam = Z;
			float Y_cam = ((SrcX - Cx) * Z) / Fx;
			float Z_cam = ((Cy - SrcY) * Z) / Fy; // Cy - SrcY maps Y downward in image to -Z in Unreal world

			// Parallax Color Sampling
			float Y_rgb = Y_cam - SensorBaselineY;
			int32 u_rgb = FMath::RoundToInt(((Y_rgb * Fx) / X_cam) + Cx);
			int32 v_rgb = FMath::RoundToInt(Cy - ((Z_cam * Fy) / X_cam));

			u_rgb = FMath::Clamp(u_rgb, 0, SourceWidth - 1);
			v_rgb = FMath::Clamp(v_rgb, 0, SourceHeight - 1);
			FLinearColor SampledColor = RGBData[(v_rgb * SourceWidth) + u_rgb];

			// Add valid point
			Vertices.Add(FVector(X_cam, Y_cam, Z_cam));
			VertexColors.Add(SampledColor);
			UV0.Add(FVector2D((float)SrcX / SourceWidth, (float)SrcY / SourceHeight));

			// Record its index for triangle generation
			GridToVertexMap.Add(FIntPoint(x, y), Vertices.Num() - 1);
		}
	}

	// 2. Generate Triangles (Connecting the projective grid)
	for (int32 y = 0; y < GridHeight - 1; ++y)
	{
		for (int32 x = 0; x < GridWidth - 1; ++x)
		{
			// Check if all 4 corners of the quad are valid points
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

				// Thresholding: Prevent stretching the mesh across huge depth discontinuities (like a foreground object to a background wall)
				if (FVector::Dist(p0, p1) < MaxEdgeStretchCM &&
					FVector::Dist(p0, p2) < MaxEdgeStretchCM &&
					FVector::Dist(p1, p3) < MaxEdgeStretchCM && FVector::Dist(p2, p3) < MaxEdgeStretchCM)
				{
					// First Triangle (v0, v2, v1)
					Triangles.Add(*v0);
					Triangles.Add(*v2);
					Triangles.Add(*v1);

					// Second Triangle (v1, v2, v3)
					Triangles.Add(*v1);
					Triangles.Add(*v2);
					Triangles.Add(*v3);
				}
			}
		}
	}

	// Default normals (recalculating accurate ones is expensive and optional for emissive point clouds)
	Normals.Init(FVector(1, 0, 0), Vertices.Num());

	// Update the mesh
	ProcMeshComp->ClearAllMeshSections();
	if (Vertices.Num() > 0 && Triangles.Num() > 0)
	{
		ProcMeshComp->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, false);
		
		if (VertexColorMaterial)
		{
			ProcMeshComp->SetMaterial(0, VertexColorMaterial);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PGM_Display: Valid points found, but no geometry was generated. Check depth thresholds or MaxEdgeStretchCM."));
	}
}
