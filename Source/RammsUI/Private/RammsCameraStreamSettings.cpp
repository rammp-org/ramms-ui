// RammsCameraStreamSettings.cpp

#include "RammsCameraStreamSettings.h"

#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "RammsCameraStreamSettings"

namespace
{
	FRammsCameraDataStream DataStream(const TCHAR* StreamID, ERammsCameraDataKind Kind, const FText& Label)
	{
		FRammsCameraDataStream Stream;
		Stream.StreamID = StreamID;
		Stream.Kind = Kind;
		Stream.Label = Label;
		return Stream;
	}

	FRammsCameraDataKindMaterials Materials(const TCHAR* Visualization, const TCHAR* Overlay)
	{
		FRammsCameraDataKindMaterials Result;
		Result.VisualizationMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(Visualization));
		Result.OverlayMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(Overlay));
		return Result;
	}
}

URammsCameraStreamSettings::URammsCameraStreamSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("Ramms Camera Streams");

	SceneColorStreamID = TEXT("camera/scene/color");
	SceneDataStreams = { DataStream(TEXT("camera/scene/depth"), ERammsCameraDataKind::Depth, LOCTEXT("Depth", "Depth")) };
	SceneDetectionSources = { TEXT(""), TEXT("scene"), TEXT("camera/scene/color"), TEXT("camera/scene/depth") };

	OverlayColorStreamID = TEXT("engine/overlay");
	OverlayDataStreams = {
		DataStream(TEXT("engine/overlay/depth"), ERammsCameraDataKind::Depth, LOCTEXT("Depth", "Depth")),
		DataStream(TEXT("engine/overlay/segmentation"), ERammsCameraDataKind::Segmentation, LOCTEXT("Segmentation", "Segmentation")),
	};
	OverlayDetectionSources = { TEXT("overlay"), TEXT("engine/overlay") };

	DepthMaterials = Materials(TEXT("/RammsUI/Materials/MI_DepthColormap.MI_DepthColormap"), TEXT("/RammsUI/Materials/MI_CombineDepthRGB.MI_CombineDepthRGB"));
	SegmentationMaterials = Materials(TEXT("/RammsUI/Materials/MI_MaskColormap.MI_MaskColormap"), TEXT("/RammsUI/Materials/MI_CombineMaskRGB.MI_CombineMaskRGB"));
	MotionVectorMaterials = Materials(TEXT("/RammsUI/Materials/MI_MotionVectorColormap.MI_MotionVectorColormap"), TEXT("/RammsUI/Materials/MI_CombineMotionRGB.MI_CombineMotionRGB"));
}

FRammsCameraStreamSetConfig URammsCameraStreamSettings::GetStreamSet(ERammsCameraStreamSet Set)
{
	const URammsCameraStreamSettings* Settings = GetDefault<URammsCameraStreamSettings>();
	FRammsCameraStreamSetConfig		  Config;
	switch (Set)
	{
		case ERammsCameraStreamSet::Scene:
			Config.ColorStreamID = Settings->SceneColorStreamID;
			Config.DataStreams = Settings->SceneDataStreams;
			Config.DetectionSources = Settings->SceneDetectionSources;
			break;
		case ERammsCameraStreamSet::EngineOverlay:
			Config.ColorStreamID = Settings->OverlayColorStreamID;
			Config.DataStreams = Settings->OverlayDataStreams;
			Config.DetectionSources = Settings->OverlayDetectionSources;
			break;
		default:
			break;
	}
	return Config;
}

const FRammsCameraDataKindMaterials& URammsCameraStreamSettings::GetMaterials(ERammsCameraDataKind Kind) const
{
	switch (Kind)
	{
		case ERammsCameraDataKind::Segmentation:
			return SegmentationMaterials;
		case ERammsCameraDataKind::MotionVectors:
			return MotionVectorMaterials;
		default:
			return DepthMaterials;
	}
}

#undef LOCTEXT_NAMESPACE
