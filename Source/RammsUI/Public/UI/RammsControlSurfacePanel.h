// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "RammsControlTypes.h"
#include "RammsControlSurfacePanel.generated.h"

class UBorder;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class URammsCollapsibleContainer;
class URammsControlRow;
class URammsSurfaceJoystick;

/**
 * Renders a robot's control surface, entirely from its description: one
 * collapsible section per group; a joystick for each pair of Continuous axes
 * (drive, arm move, camera orbit — the lower-Order axis is the vertical one),
 * hold buttons for lone rate axes, a slider row (URammsAxisControl) for
 * Position / Velocity targets, a button per Action. Readback values are
 * pulled at ReadbackInterval. No robot-specific code: whatever the surface
 * describes appears, and the panel rebuilds when the surface's version
 * changes (a contributor added, a motor registry loaded).
 *
 * The surface is found through URammsUISubsystem's registry (first one, or
 * by TargetRobotName) unless TargetSurface is set.
 */
UCLASS(meta = (DisplayName = "Ramms Control Surface Panel"))
class RAMMSUI_API URammsControlSurfacePanel : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	/** Object implementing IRammsControlSurfaceProvider + IRammsControlSink. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	TObjectPtr<UObject> TargetSurface;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	bool bAutoFindSurface = true;

	/** With bAutoFindSurface: prefer the registered surface with this robot name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	FString TargetRobotName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	ERammsControlSource Source = ERammsControlSource::Touch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface", meta = (ClampMin = "0.02"))
	float ReadbackInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	bool bShowRobotName = true;

	/** Groups start collapsed except these. Empty = all expanded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	TArray<FName> ExpandedGroups;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	float JoystickRadius = 56.0f;

	/** The panel fills its slot; this keeps rows readable in a narrow one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	float MinPanelWidth = 360.0f;

	UFUNCTION(BlueprintCallable, Category = "Control Surface")
	void SetTargetSurface(UObject* Surface);

	UFUNCTION(BlueprintCallable, Category = "Control Surface")
	void Rebuild();

	UFUNCTION(BlueprintPure, Category = "Control Surface")
	UObject* GetTargetSurface() const { return TargetSurface; }

	/** The row rendering a control, if any (tests, decorations). */
	UFUNCTION(BlueprintPure, Category = "Control Surface")
	URammsControlRow* FindRow(FName ControlId) const;

	/** The joystick rendering a paired axis, if any. */
	UFUNCTION(BlueprintPure, Category = "Control Surface")
	URammsSurfaceJoystick* FindJoystick(FName ControlId) const;

	UFUNCTION(BlueprintPure, Category = "Control Surface")
	int32 GetRowCount() const { return Rows.Num(); }

	/** Vertical scroll position of the groups list (tests). */
	UFUNCTION(BlueprintPure, Category = "Control Surface")
	float GetScrollOffset() const;

	/** Scrollable extent: X = the groups list's allotted height, Y = its scroll offset at the end (0 = nothing to scroll). */
	UFUNCTION(BlueprintPure, Category = "Control Surface")
	FVector2D GetScrollExtent() const;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void ApplyStyle_Implementation() override;

	/** Pixels the groups list scrolls per wheel notch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Surface")
	float WheelScrollStep = 48.0f;

	/** Scroll the groups list to an absolute offset (tests). */
	UFUNCTION(BlueprintCallable, Category = "Control Surface")
	void SetScrollOffset(float Offset);

protected:
	virtual void	 BuildWidgetTree() override;
	virtual void	 ResetCachedWidgets() override;
	virtual UWidget* GetRootWidgetForValidation() override;

private:
	UFUNCTION()
	void OnRegistryChanged(UObject* Provider, bool bRegistered);

	bool ResolveSurface();
	void BuildGroup(FName Group, const TArray<FRammsControlAxis>& Axes);

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> MainVBox;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> GroupsScroll;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> GroupsVBox;
	UPROPERTY(Transient)
	TArray<TObjectPtr<URammsCollapsibleContainer>> Groups;
	UPROPERTY(Transient)
	TArray<TObjectPtr<URammsControlRow>> Rows;
	UPROPERTY(Transient)
	TArray<TObjectPtr<URammsSurfaceJoystick>> Joysticks;

	int32 BuiltVersion = -1;
	float ReadbackAccum = 0.0f;
	bool  bSubscribed = false;
};
