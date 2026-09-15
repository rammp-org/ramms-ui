// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RammsControlTypes.h"
#include "RammsControlSink.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class URammsControlSink : public UInterface
{
	GENERATED_BODY()
};

/**
 * Where control commands go. Every input source — a keyboard/gamepad input
 * map, a touch panel, a Remote Control call, an autonomy client — drives the
 * robot through this, by control Id, and says who it is; the sink arbitrates
 * (an external source holds an axis over local input for a timeout) and
 * routes to whatever implements the control.
 *
 * Return values are honest: false means the command was not applied (unknown
 * Id, held by a higher-priority source, the backend can't do it).
 */
class RAMMSCONTROL_API IRammsControlSink
{
	GENERATED_BODY()

public:
	/** Set a Continuous / Position / Velocity control's value (clamped to its range). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ramms|Control")
	bool SetAxis(FName Id, float Value, ERammsControlSource Source);

	/** Fire an Action control. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ramms|Control")
	bool TriggerAction(FName Id, ERammsControlSource Source);

	/** The source lets go of a control: a Continuous axis springs to its
	 *  default, a Position / Velocity servo may be released (stops holding). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ramms|Control")
	bool ReleaseAxis(FName Id, ERammsControlSource Source);

	/** Live value of a control with readback; DefaultValue if unknown. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ramms|Control")
	float GetAxisValue(FName Id) const;

	/** Which source currently holds a control (Script if none). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ramms|Control")
	ERammsControlSource GetAxisOwner(FName Id) const;
};
