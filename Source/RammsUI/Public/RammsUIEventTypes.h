// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
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

	// ── Structured Payloads ──────────────────────────────────────

	/** Carry any USTRUCT as payload. Use GetPayloadAs<T>() in C++ or "Get Struct" node in Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event|Payload")
	FInstancedStruct StructPayload;

	/** Key-value string map for ad-hoc data without defining a new USTRUCT */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event|Payload")
	TMap<FName, FString> Properties;

	// ── C++ typed getters for Properties map ─────────────────────

	FString GetPayloadString(FName Key, const FString& Default = FString()) const
	{
		const FString* Found = Properties.Find(Key);
		return Found ? *Found : Default;
	}

	float GetPayloadFloat(FName Key, float Default = 0.0f) const
	{
		const FString* Found = Properties.Find(Key);
		return (Found && !Found->IsEmpty()) ? FCString::Atof(**Found) : Default;
	}

	int32 GetPayloadInt(FName Key, int32 Default = 0) const
	{
		const FString* Found = Properties.Find(Key);
		return (Found && Found->IsNumeric()) ? FCString::Atoi(**Found) : Default;
	}

	bool GetPayloadBool(FName Key, bool Default = false) const
	{
		const FString* Found = Properties.Find(Key);
		if (!Found || Found->IsEmpty())
			return Default;
		return Found->Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Found->Equals(TEXT("1"))
			|| Found->Equals(TEXT("yes"), ESearchCase::IgnoreCase);
	}

	uint8 GetPayloadByte(FName Key, uint8 Default = 0) const
	{
		const FString* Found = Properties.Find(Key);
		return (Found && Found->IsNumeric()) ? static_cast<uint8>(FCString::Atoi(**Found)) : Default;
	}

	/** Get the struct payload as a specific type (C++ only). Returns nullptr if type doesn't match. */
	template <typename T>
	const T* GetPayloadAs() const
	{
		return StructPayload.GetPtr<T>();
	}
};
