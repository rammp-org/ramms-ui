// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Components/Border.h"
#include "Components/NamedSlot.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "RammsPanel.generated.h"

/**
 * A standalone styled panel widget with rounded corners, background, and optional border.
 * Use as a container to wrap any content with consistent Ramms UI styling.
 *
 * Features:
 * - Rounded corner background from URammsUIStyle
 * - Optional header bar with title text
 * - Optional border outline
 * - NamedSlot "ContentSlot" for placing child widgets in the designer
 * - Configurable padding, opacity, and corner radius overrides
 */
UCLASS(meta = (DisplayName = "Ramms Panel"))
class RAMMSUI_API URammsPanel : public URammsBaseWidget
{
	GENERATED_BODY()

protected:
	// ── Panel Settings ──

	/** Optional header text. Leave empty for a headerless panel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel", meta = (ExposeOnSpawn = true))
	FText HeaderText;

	/** Show border outline around the panel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel|Style")
	bool bShowBorder = true;

	/** Show header bar (only visible when HeaderText is non-empty) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel|Style")
	bool bShowHeader = true;

	/** Background opacity override (0.0–1.0). Negative = use style default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel|Style", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float BackgroundOpacity = -1.0f;

	/** Corner radius override. Negative = use style default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel|Style", meta = (ClampMin = "-1.0"))
	float CornerRadiusOverride = -1.0f;

	/** Inner content padding */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel|Style")
	FMargin ContentPadding = FMargin(8.0f);

	// ── Cached Widgets ──

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> HeaderBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HeaderLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> ContentVBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ContentBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UNamedSlot> ContentSlot;

public:
	/** Get the content vertical box for adding children programmatically */
	UFUNCTION(BlueprintPure, Category = "Panel")
	UVerticalBox* GetContentBox() const { return ContentVBox; }

	/** Add a widget to the content area */
	UFUNCTION(BlueprintCallable, Category = "Panel")
	void AddContentChild(UWidget* Child);

	/** Set header text at runtime */
	UFUNCTION(BlueprintCallable, Category = "Panel")
	void SetHeaderText(FText Text);

	/** Set whether the border outline is visible */
	UFUNCTION(BlueprintCallable, Category = "Panel")
	void SetShowBorder(bool bShow);

	virtual void ApplyStyle_Implementation() override;
	virtual void SynchronizeProperties() override;

protected:
	virtual void ResetCachedWidgets() override;
	virtual void BuildWidgetTree() override;
	void		 UpdateHeaderVisibility();
};
