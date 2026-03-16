// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RammsUIEventTypes.generated.h"

// ── Camera / Viewport Overlay Types ──────────────────────────────

UENUM(BlueprintType)
enum class ERammsOverlayType : uint8
{
	None			UMETA(DisplayName = "None"),
	DepthMap		UMETA(DisplayName = "Depth Map"),
	ThermalMap		UMETA(DisplayName = "Thermal Map"),
	ObjectDetection UMETA(DisplayName = "Object Detection"),
	Segmentation	UMETA(DisplayName = "Segmentation")
};

// ── 3D Visualization Layer Types ─────────────────────────────────

UENUM(BlueprintType)
enum class ERammsVisualizationLayer : uint8
{
	None			  UMETA(DisplayName = "None"),
	LidarPoints		  UMETA(DisplayName = "Lidar Points"),
	NavigationPath	  UMETA(DisplayName = "Navigation Path"),
	CollisionZones	  UMETA(DisplayName = "Collision Zones"),
	JointAxes		  UMETA(DisplayName = "Joint Axes"),
	WorkspaceEnvelope UMETA(DisplayName = "Workspace Envelope"),
	CurbDetection	  UMETA(DisplayName = "Curb Detection")
};

// ── Robot Highlight Targets ──────────────────────────────────────

UENUM(BlueprintType)
enum class ERammsHighlightTarget : uint8
{
	None		UMETA(DisplayName = "None"),
	EndEffector UMETA(DisplayName = "End Effector"),
	Base		UMETA(DisplayName = "Base"),
	Wheels		UMETA(DisplayName = "Wheels"),
	Sensors		UMETA(DisplayName = "Sensors")
};

// ── Custom Event Payload (Blueprint-extensible) ──────────────────
/**
 * Typed payload for custom UI events. Use this when you need a new
 * event type from Blueprint without adding a C++ delegate.
 *
 * Set EventName to identify the event, then populate whichever
 * typed fields your event requires. Subscribers filter on EventName.
 */
USTRUCT(BlueprintType)
struct RAMMSUI_API FRammsUIEvent
{
	GENERATED_BODY()

	/** Identifier for this event (subscribers filter on this) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FName EventName;

	/** Generic boolean payload */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	bool bEnabled = false;

	/** Generic float payload */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	float FloatValue = 0.0f;

	/** Generic integer payload */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	int32 IntValue = 0;

	/** Color payload (for visualization styling) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FLinearColor ColorValue = FLinearColor::White;

	/** Target identifier (e.g., which overlay, which joint) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FName TargetID;

	/** String payload (for complex data that doesn't fit typed fields) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FString StringValue;

	/** Vector payload (position, direction, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FVector VectorValue = FVector::ZeroVector;
};
