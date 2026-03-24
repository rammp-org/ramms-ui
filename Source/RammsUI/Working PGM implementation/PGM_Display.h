#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "CaptureComponent.h"
#include "PGM_Display.generated.h"

UCLASS()
class RAMMSUI_API APGM_Display : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APGM_Display();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// The procedural mesh component used to render the projective grid
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PGM Rendering")
	UProceduralMeshComponent* ProcMeshComp;

	// The source actor that contains a UCaptureComponent (e.g. your Simulated D435i Blueprint)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM Source")
	AActor* CaptureSourceActor;

	// Maximum allowed edge length between vertices to form a face (in cm). Prevents huge stretched triangles.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM Algorithm")
	float MaxEdgeStretchCM = 50.0f;

	// Multiplier to convert raw depth texture values to Centimeters. Set to 100.0 if raw is in Meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM Algorithm")
	float DepthScaleToCM = 1.0f;

	// Minimum depth to consider valid (in cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM Algorithm")
	float MinDepthCM = 10.0f;

	// Maximum depth to consider valid (in cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM Algorithm")
	float MaxDepthCM = 1500.0f;

	// Decimation factor (1 = full res, 2 = half res, 4 = quarter res). 
	// Higher numbers improve performance at the cost of detail.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM Algorithm", meta=(ClampMin="1"))
	int32 Decimation = 4;

	// Offset applied to RGB sampling due to the physical distance between Depth and RGB sensors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM Algorithm")
	float SensorBaselineY = 1.5f;

	// Material used to render vertex colors. Needs a material that uses the "Vertex Color" node mapped to Base Color!
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PGM Rendering")
	UMaterialInterface* VertexColorMaterial;

	// The main execution function to be called via Blueprints (e.g. on key press 'K')
	UFUNCTION(BlueprintCallable, Category = "PGM Action")
	void UpdateDisplay();

private:
	// Helper function to extract intrinsically correct FOV and Principal Points
	void CalculateIntrinsics(UCaptureComponent* CaptureComp, int32 Width, int32 Height, float& outFx, float& outFy, float& outCx, float& outCy);
};
