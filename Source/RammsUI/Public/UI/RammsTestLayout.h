// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsLayoutBase.h"
#include "RammsTestLayout.generated.h"

/**
 * Simple concrete layout for testing the LayoutHost system.
 *
 * Creates a full-screen Overlay with two named injection containers:
 * - "Main" — fills the entire layout area
 * - "Sidebar" — positioned as a smaller overlay on the right side
 *
 * Use this layout to verify pool widget injection works before creating
 * Blueprint layout subclasses.
 *
 * Example setup in Blueprint or C++:
 *   Host->AddLayout(URammsTestLayout::StaticClass(), "TestLayout");
 *   Host->AddPoolWidget("Main", MyCameraWidget);
 *   Host->AddPoolWidget("Sidebar", MyStatusWidget);
 */
UCLASS(meta = (DisplayName = "Ramms Test Layout"))
class RAMMSUI_API URammsTestLayout : public URammsLayoutBase
{
	GENERATED_BODY()

protected:
	virtual void BuildWidgetTree() override;
};
