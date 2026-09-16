// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsLayoutBase.h"
#include "RammsSimLayout.generated.h"

/**
 * The simulator's default layout: a control-surface panel down the right
 * edge, a drive joystick bottom-left, a status area top-left. Slots
 * (pool tags): "SurfacePanel", "Joystick", "Status". Built in code so no
 * Blueprint asset is needed; subclass in Blueprint to restyle.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Ramms Sim Layout"))
class RAMMSUI_API URammsSimLayout : public URammsLayoutBase
{
	GENERATED_BODY()

public:
	/** Fraction of the viewport width the right-hand panel column takes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0.15", ClampMax = "0.5"))
	float PanelColumnFraction = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FVector2D JoystickAreaSize = FVector2D(200.0f, 200.0f);

	virtual TArray<FName> GetLayoutSlotNames_Implementation() const override;

protected:
	virtual void BuildWidgetTree() override;
};
