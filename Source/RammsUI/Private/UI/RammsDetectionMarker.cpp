// RammsDetectionMarker.cpp

#include "UI/RammsDetectionMarker.h"

#include "Engine/Engine.h"
#include "RammsBridgeSubsystem.h"

void URammsDetectionMarker::SetDetection(const FRammsBoundingBox& InDetection)
{
	Detection = InDetection;
	OnDetectionSet(Detection);
}

void URammsDetectionMarker::PostObjectTapped() const
{
	if (Detection.ID.IsNone())
	{
		return;
	}
	if (URammsBridgeSubsystem* Bridge = GEngine ? GEngine->GetEngineSubsystem<URammsBridgeSubsystem>() : nullptr)
	{
		Bridge->PostObjectTapped(Detection.ID.ToString());
	}
}
