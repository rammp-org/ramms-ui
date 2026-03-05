// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsCameraProjectorComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

URammsCameraProjectorComponent::URammsCameraProjectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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

// ── Public API ─────────────────────────────────────────────────────

void URammsCameraProjectorComponent::SetCameraTexture(UTexture* Texture)
{
	EnsureDecalCreated();
	if (MaterialInstance)
	{
		MaterialInstance->SetTextureParameterValue(FName("CameraTexture"), Texture);
	}
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
}

void URammsCameraProjectorComponent::RefreshMaterialParameters()
{
	UpdateDecalSize();
	UpdateMaterialParameters();
}
