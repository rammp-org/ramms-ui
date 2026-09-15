// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RammsControlTypes.h"
#include "RammsControlSurfaceProvider.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class URammsControlSurfaceProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Something that describes a robot's controls (see FRammsControlSurface).
 * Implemented by the robot actor or a component on it — in the sim by the
 * adapter that gathers the RobotBase registry and the controllers present; on
 * a real robot by whatever bridges its command interface.
 *
 * The description can change (a controller resolves late, a MuJoCo scene
 * recompiles): GetControlSurfaceVersion increments on every change, so a
 * panel can cheaply notice and re-query.
 */
class RAMMSCONTROL_API IRammsControlSurfaceProvider
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ramms|Control")
	FRammsControlSurface GetControlSurface() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ramms|Control")
	int32 GetControlSurfaceVersion() const;
};
