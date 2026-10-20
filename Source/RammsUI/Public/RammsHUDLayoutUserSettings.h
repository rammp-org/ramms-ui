// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RammsHUDLayoutUserSettings.generated.h"

/** Where a floating HUD panel is and how it is shown (URammsFloatingPanel). */
USTRUCT(BlueprintType)
struct RAMMSUI_API FRammsFloatingPanelState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FName Id;

	/** Its top-left as a fraction of the room the viewport leaves around it
	 *  (0,0 top-left, 1,1 bottom-right), so a panel against an edge stays
	 *  there when the window is resized; negative: never placed (its
	 *  default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FVector2D Placement = FVector2D(-1.0, -1.0);

	/** Its body's size (Slate units); 0 on an axis: the content's own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FVector2D Size = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bMinimized = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bHidden = false;

	/** In a window of its own, at WindowPosition (desktop pixels) with a
	 *  client area of WindowSize. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bPoppedOut = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FVector2D WindowPosition = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FVector2D WindowSize = FVector2D::ZeroVector;
};

/**
 * The HUD layout each user arranged: every floating panel's place, size and
 * state, kept per user in GameUserSettings.ini (Saved/Config), never in the
 * project's config. ramms.UI.ResetLayout forgets it.
 */
UCLASS(Config = GameUserSettings)
class RAMMSUI_API URammsHUDLayoutUserSettings : public UObject
{
	GENERATED_BODY()

public:
	static URammsHUDLayoutUserSettings* Get() { return GetMutableDefault<URammsHUDLayoutUserSettings>(); }

	/** The panel's stored state; false when it has none. */
	bool Find(FName Id, FRammsFloatingPanelState& OutState) const;

	/** Store (replace) the panel's state and write the file. */
	void Store(const FRammsFloatingPanelState& State);

	/** Forget every panel's state and write the file. */
	void ResetAll();

private:
	void Write();

	UPROPERTY(Config)
	TArray<FRammsFloatingPanelState> Panels;
};
