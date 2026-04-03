// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "Components/TextBlock.h"
#include "RammsTextBlock.generated.h"

/**
 * Text variant — selects which font from FRammsTypography to use
 */
UENUM(BlueprintType)
enum class ERammsTextVariant : uint8
{
	HeadingLarge,
	HeadingMedium,
	HeadingSmall,
	Body,
	Caption,
	Monospace
};

/**
 * Which palette color to use for text
 */
UENUM(BlueprintType)
enum class ERammsTextColor : uint8
{
	/** Uses Colors.TextPrimary */
	Primary,
	/** Uses Colors.TextSecondary */
	Secondary,
	/** Uses Colors.TextDisabled */
	Disabled,
	/** Uses Colors.Primary (accent) */
	Accent,
	/** Uses Colors.Success */
	Success,
	/** Uses Colors.Warning */
	Warning,
	/** Uses Colors.Error */
	Error,
	/** Uses a custom color (ColorOverride) */
	Custom
};

/**
 * Themed text block that automatically updates font and color when the theme changes.
 *
 * Usage:
 *   - Place in UMG designer or create programmatically
 *   - Select a Variant (HeadingLarge … Monospace) to pick the font
 *   - Select a TextColorMode (Primary … Custom) to pick the color
 *   - Text, justification, and wrapping are all exposed
 */
UCLASS(meta = (DisplayName = "Ramms Text Block"))
class RAMMSUI_API URammsTextBlock : public URammsBaseWidget
{
	GENERATED_BODY()

protected:
	// ── Content ──────────────────────────────────────────────────

	/** The text to display */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text", meta = (MultiLine = true))
	FText Text = FText::FromString(TEXT("Text"));

	/** Font variant from the theme */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text")
	ERammsTextVariant Variant = ERammsTextVariant::Body;

	/** Color mode from the theme palette */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text")
	ERammsTextColor TextColorMode = ERammsTextColor::Primary;

	/** Custom color (used when TextColorMode == Custom) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text",
		meta = (EditCondition = "TextColorMode == ERammsTextColor::Custom"))
	FLinearColor ColorOverride = FLinearColor::White;

	/** Text justification */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text")
	TEnumAsByte<ETextJustify::Type> Justification = ETextJustify::Left;

	/** Whether to auto-wrap text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text")
	bool bAutoWrapText = false;

	// ── Widget references ────────────────────────────────────────

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InnerText;

public:
	// ── API ──────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "Ramms|Text")
	void SetText(FText InText);

	UFUNCTION(BlueprintPure, Category = "Ramms|Text")
	FText GetText() const { return Text; }

	UFUNCTION(BlueprintCallable, Category = "Ramms|Text")
	void SetVariant(ERammsTextVariant InVariant);

	UFUNCTION(BlueprintCallable, Category = "Ramms|Text")
	void SetTextColorMode(ERammsTextColor InMode);

	UFUNCTION(BlueprintCallable, Category = "Ramms|Text")
	void SetColorOverride(FLinearColor InColor);

	/** Direct access to the inner UTextBlock (for advanced use) */
	UFUNCTION(BlueprintPure, Category = "Ramms|Text")
	UTextBlock* GetInnerTextBlock() const { return InnerText; }

	// ── Lifecycle ────────────────────────────────────────────────

	virtual void SynchronizeProperties() override;
	virtual void ApplyStyle_Implementation() override;

protected:
	virtual void	 BuildWidgetTree() override;
	virtual void	 ResetCachedWidgets() override;
	virtual UWidget* GetRootWidgetForValidation() override { return InnerText; }

private:
	/** Resolve the FSlateFontInfo for the current Variant from the theme */
	FSlateFontInfo GetFontForVariant() const;

	/** Resolve the FLinearColor for the current TextColorMode from the theme */
	FLinearColor GetColorForMode() const;
};
