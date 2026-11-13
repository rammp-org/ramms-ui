// RammsCameraStreamSettings.h
//
// Project Settings > Plugins > Ramms Camera Streams: which streams the UI's cameras show, kept in one
// place because the bridge's stream IDs change, and for debugging. A camera widget whose StreamSet is
// Scene or Engine Overlay takes its streams from here (URammsCameraWidget::StreamSet):
//
//   Scene camera    the main screen's background (URammsSceneCameraView)
//   Engine overlay  the portal (RAMMS_PassThroughPortal): the running engine's own camera, which the
//                   bridge sends while the engine shows it (OpenCameraFeed / CloseCameraFeed)
//
// Config = Game, so besides Project Settings / DefaultGame.ini they can be changed without a rebuild:
// in a packaged build's Saved/Config/<Platform>/Game.ini, or on the command line, for example
//   -ini:Game:[/Script/RammsUI.RammsCameraStreamSettings]:OverlayColorStreamID=camera/wrist/color
// URammsCameraWidget::ApplyStreamSet re-reads them at runtime.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RammsCameraStreamSettings.generated.h"

class UMaterialInterface;

/** What a data stream holds, which picks the material that shows it. */
UENUM(BlueprintType)
enum class ERammsCameraDataKind : uint8
{
	Depth,
	Segmentation UMETA(DisplayName = "Segmentation / mask"),
	MotionVectors UMETA(DisplayName = "Motion vectors"),
};

/** Where a camera widget's streams come from. */
UENUM(BlueprintType)
enum class ERammsCameraStreamSet : uint8
{
	/** The widget's own StreamID, DataStreamID, DataStreamConfig and BridgeDetectionSources. */
	Custom,
	/** Ramms Camera Streams' scene camera set. */
	Scene UMETA(DisplayName = "Scene camera"),
	/** Ramms Camera Streams' engine overlay set. */
	EngineOverlay UMETA(DisplayName = "Engine overlay"),
};

/** One of a camera's extra images (depth, segmentation ...), shown through its kind's material. */
USTRUCT(BlueprintType)
struct RAMMSUI_API FRammsCameraDataStream
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Camera Streams")
	FString StreamID;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Camera Streams")
	ERammsCameraDataKind Kind = ERammsCameraDataKind::Depth;

	/** Name for the camera's view-mode button; empty: the kind's name. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Camera Streams")
	FText Label;
};

/** A camera's streams: its image, its extra images, and which of the bridge's objects are its. */
USTRUCT(BlueprintType)
struct RAMMSUI_API FRammsCameraStreamSetConfig
{
	GENERATED_BODY()

	/** The image the camera shows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Streams")
	FString ColorStreamID;

	/** Extra images of the same view; the first is the camera's data stream, SetDataStreamIndex picks another. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Streams")
	TArray<FRammsCameraDataStream> DataStreams;

	/** Which of the bridge's objects are this camera's (URammsCameraWidget::BridgeDetectionSources). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Streams")
	TArray<FString> DetectionSources;
};

/** The materials that show one kind of data stream (see FRammsDataStreamMaterialConfig). */
USTRUCT(BlueprintType)
struct RAMMSUI_API FRammsCameraDataKindMaterials
{
	GENERATED_BODY()

	/** The data on its own (the Data view). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Camera Streams")
	TSoftObjectPtr<UMaterialInterface> VisualizationMaterial;

	/** The data blended over the image (the Overlay view). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Camera Streams")
	TSoftObjectPtr<UMaterialInterface> OverlayMaterial;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Ramms Camera Streams"))
class RAMMSUI_API URammsCameraStreamSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URammsCameraStreamSettings();

	// -- Scene camera ------------------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "Scene Camera")
	FString SceneColorStreamID;

	UPROPERTY(Config, EditAnywhere, Category = "Scene Camera")
	TArray<FRammsCameraDataStream> SceneDataStreams;

	/** The orchestrator's own objects come with no source (""); an engine marks the scene's "scene". */
	UPROPERTY(Config, EditAnywhere, Category = "Scene Camera")
	TArray<FString> SceneDetectionSources;

	// -- Engine overlay ----------------------------------------------------------

	/** The bridge sends the running engine's camera as "engine/overlay" (channel 2). */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Overlay")
	FString OverlayColorStreamID;

	UPROPERTY(Config, EditAnywhere, Category = "Engine Overlay")
	TArray<FRammsCameraDataStream> OverlayDataStreams;

	/** An engine marks objects on its own camera "overlay". */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Overlay")
	TArray<FString> OverlayDetectionSources;

	// -- Data stream materials ---------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "Data Stream Materials")
	FRammsCameraDataKindMaterials DepthMaterials;

	UPROPERTY(Config, EditAnywhere, Category = "Data Stream Materials")
	FRammsCameraDataKindMaterials SegmentationMaterials;

	UPROPERTY(Config, EditAnywhere, Category = "Data Stream Materials")
	FRammsCameraDataKindMaterials MotionVectorMaterials;

	/** A stream set as configured (Custom: empty). */
	UFUNCTION(BlueprintPure, Category = "Ramms|Camera Streams")
	static FRammsCameraStreamSetConfig GetStreamSet(ERammsCameraStreamSet Set);

	/** The materials for a kind of data stream. */
	const FRammsCameraDataKindMaterials& GetMaterials(ERammsCameraDataKind Kind) const;
};
