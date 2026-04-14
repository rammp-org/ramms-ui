// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsUIStyle.h"
#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"

#if WITH_EDITOR
	#include "UObject/SavePackage.h"
	#include "AssetRegistry/AssetRegistryModule.h"
#endif

void URammsUIStyle::ApplyRoundedBrushToBorder(UBorder* Border, const FSlateBrush& Brush)
{
	if (!Border)
		return;

	Border->Background = Brush;

	// If the underlying Slate widget already exists, push changes to it
	if (Border->GetCachedWidget().IsValid())
	{
		Border->SynchronizeProperties();
	}
}

void URammsUIStyle::ApplyScrollBarStyle(UScrollBox* ScrollBox, const FRammsScrollBarStyle& SBS)
{
	if (!ScrollBox)
		return;

	float R = SBS.CornerRadius;

	// Thumb brushes (rounded, semi-transparent)
	FSlateBrush ThumbNormal = MakeRoundedBoxBrush(SBS.ThumbNormalColor, R);
	FSlateBrush ThumbHovered = MakeRoundedBoxBrush(SBS.ThumbHoveredColor, R);
	FSlateBrush ThumbDragged = MakeRoundedBoxBrush(SBS.ThumbDraggedColor, R);

	// Track brush
	FSlateBrush TrackBrush = MakeRoundedBoxBrush(SBS.TrackColor, R);

	FScrollBarStyle BarStyle = FScrollBarStyle::GetDefault();
	BarStyle.SetNormalThumbImage(ThumbNormal);
	BarStyle.SetHoveredThumbImage(ThumbHovered);
	BarStyle.SetDraggedThumbImage(ThumbDragged);
	BarStyle.SetVerticalBackgroundImage(TrackBrush);
	BarStyle.SetHorizontalBackgroundImage(TrackBrush);
	BarStyle.SetVerticalTopSlotImage(TrackBrush);
	BarStyle.SetVerticalBottomSlotImage(TrackBrush);
	BarStyle.SetHorizontalTopSlotImage(TrackBrush);
	BarStyle.SetHorizontalBottomSlotImage(TrackBrush);
	BarStyle.SetThickness(SBS.Thickness);

	ScrollBox->SetWidgetBarStyle(BarStyle);
	ScrollBox->SetScrollbarThickness(FVector2D(SBS.Thickness, SBS.Thickness));
	ScrollBox->SetScrollbarPadding(SBS.Padding);

	// Disable scroll edge shadows (they ignore rounded corners)
	if (SBS.bDisableEdgeShadows)
	{
		FScrollBoxStyle BoxStyle = ScrollBox->GetWidgetStyle();
		FSlateBrush		EmptyBrush;
		EmptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		BoxStyle.SetTopShadowBrush(EmptyBrush);
		BoxStyle.SetBottomShadowBrush(EmptyBrush);
		BoxStyle.SetLeftShadowBrush(EmptyBrush);
		BoxStyle.SetRightShadowBrush(EmptyBrush);
		ScrollBox->SetWidgetStyle(BoxStyle);
	}
}

void URammsUIStyle::ApplySliderStyle(USlider* Slider, const FRammsSliderStyle& SS)
{
	if (!Slider)
		return;

	float ThumbRadius = SS.ThumbSize * 0.5f;
	float BarRadius = SS.BarThickness * 0.5f;

	// Thumb brushes (circular via large corner radius)
	FSlateBrush ThumbNormal = MakeRoundedBoxBrush(SS.ThumbColor, ThumbRadius);
	ThumbNormal.SetImageSize(FVector2D(SS.ThumbSize, SS.ThumbSize));

	FSlateBrush ThumbHovered = MakeRoundedBoxBrush(SS.GetThumbHoveredColor(), ThumbRadius);
	ThumbHovered.SetImageSize(FVector2D(SS.ThumbSize, SS.ThumbSize));

	FSlateBrush ThumbDisabled = MakeRoundedBoxBrush(SS.DisabledColor, ThumbRadius);
	ThumbDisabled.SetImageSize(FVector2D(SS.ThumbSize, SS.ThumbSize));

	// Bar / track brushes (rounded rectangle)
	FSlateBrush BarNormal = MakeRoundedBoxBrush(SS.TrackColor, BarRadius);
	BarNormal.SetImageSize(FVector2D(SS.BarThickness, SS.BarThickness));

	FLinearColor BarHoverColor = FLinearColor::LerpUsingHSV(SS.TrackColor, SS.ActiveBarColor, 0.15f);
	FSlateBrush	 BarHovered = MakeRoundedBoxBrush(BarHoverColor, BarRadius);
	BarHovered.SetImageSize(FVector2D(SS.BarThickness, SS.BarThickness));

	FSlateBrush BarDisabled = MakeRoundedBoxBrush(SS.DisabledColor, BarRadius);
	BarDisabled.SetImageSize(FVector2D(SS.BarThickness, SS.BarThickness));

	// Assemble FSliderStyle
	FSliderStyle SliderStyle = FSliderStyle::GetDefault();
	SliderStyle.SetNormalBarImage(BarNormal);
	SliderStyle.SetHoveredBarImage(BarHovered);
	SliderStyle.SetDisabledBarImage(BarDisabled);
	SliderStyle.SetNormalThumbImage(ThumbNormal);
	SliderStyle.SetHoveredThumbImage(ThumbHovered);
	SliderStyle.SetDisabledThumbImage(ThumbDisabled);
	SliderStyle.SetBarThickness(SS.BarThickness);

	Slider->SetWidgetStyle(SliderStyle);

	// Tint the filled bar with the active color
	Slider->SetSliderBarColor(SS.ActiveBarColor);
	Slider->SetSliderHandleColor(FLinearColor::White);

	// Push to Slate if already constructed
	if (Slider->GetCachedWidget().IsValid())
	{
		Slider->SynchronizeProperties();
	}
}

void URammsUIStyle::ApplyCheckBoxStyle(UCheckBox* CheckBox, const FRammsCheckBoxStyle& CBS)
{
	if (!CheckBox)
		return;

	FCheckBoxStyle CBWidgetStyle = FCheckBoxStyle::GetDefault();

	const float		R = CBS.CornerRadius;
	const FVector2D BoxSize(CBS.Size, CBS.Size);

	// Unchecked states
	FSlateBrush Unchecked = MakeRoundedBoxBrush(CBS.UncheckedColor, R);
	Unchecked.SetImageSize(BoxSize);
	Unchecked.OutlineSettings.Color = FSlateColor(CBS.BorderColor);
	Unchecked.OutlineSettings.Width = 1.5f;
	Unchecked.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Unchecked.OutlineSettings.CornerRadii = FVector4(R, R, R, R);

	FSlateBrush	 UncheckedHovered = Unchecked;
	FLinearColor HoveredBg = CBS.UncheckedColor + CBS.HoveredTint;
	HoveredBg.A = FMath::Clamp(HoveredBg.A, 0.0f, 1.0f);
	UncheckedHovered.TintColor = FSlateColor(HoveredBg);

	// Checked states
	FSlateBrush Checked = MakeRoundedBoxBrush(CBS.CheckedColor, R);
	Checked.SetImageSize(BoxSize);

	FSlateBrush	 CheckedHovered = Checked;
	FLinearColor CheckedHovBg = CBS.CheckedColor + CBS.HoveredTint;
	CheckedHovBg.A = FMath::Clamp(CheckedHovBg.A, 0.0f, 1.0f);
	CheckedHovered.TintColor = FSlateColor(CheckedHovBg);

	// Undetermined states (use unchecked appearance)
	FSlateBrush Undetermined = Unchecked;

	CBWidgetStyle.SetUncheckedImage(Unchecked);
	CBWidgetStyle.SetUncheckedHoveredImage(UncheckedHovered);
	CBWidgetStyle.SetUncheckedPressedImage(UncheckedHovered);

	CBWidgetStyle.SetCheckedImage(Checked);
	CBWidgetStyle.SetCheckedHoveredImage(CheckedHovered);
	CBWidgetStyle.SetCheckedPressedImage(CheckedHovered);

	CBWidgetStyle.SetUndeterminedImage(Undetermined);
	CBWidgetStyle.SetUndeterminedHoveredImage(UncheckedHovered);
	CBWidgetStyle.SetUndeterminedPressedImage(UncheckedHovered);

	// Foreground (check mark) color
	CBWidgetStyle.ForegroundColor = FSlateColor(CBS.CheckMarkColor);

	// Padding inside the checkbox
	CBWidgetStyle.SetPadding(FMargin(2.0f));

	CheckBox->SetWidgetStyle(CBWidgetStyle);

	if (CheckBox->GetCachedWidget().IsValid())
	{
		CheckBox->SynchronizeProperties();
	}
}

// ── Built-in Theme Factories ──────────────────────────────────────

FRammsColorPalette URammsUIStyle::MakeLightPalette()
{
	FRammsColorPalette P;
	P.Primary = FLinearColor(0.0f, 0.4f, 0.75f, 1.0f);		   // Slightly deeper blue
	P.Secondary = FLinearColor(0.55f, 0.55f, 0.55f, 1.0f);	   // Medium gray
	P.Background = FLinearColor(0.95f, 0.95f, 0.95f, 1.0f);	   // Near-white
	P.Surface = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);		   // White
	P.TextPrimary = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);	   // Near-black
	P.TextSecondary = FLinearColor(0.35f, 0.35f, 0.35f, 1.0f); // Dark gray
	P.TextDisabled = FLinearColor(0.6f, 0.6f, 0.6f, 1.0f);	   // Light gray
	P.Border = FLinearColor(0.8f, 0.8f, 0.8f, 1.0f);		   // Light border
	P.Success = FLinearColor(0.0f, 0.65f, 0.25f, 1.0f);		   // Green
	P.Warning = FLinearColor(0.85f, 0.6f, 0.0f, 1.0f);		   // Orange
	P.Error = FLinearColor(0.8f, 0.15f, 0.15f, 1.0f);		   // Red
	P.Info = FLinearColor(0.0f, 0.55f, 0.8f, 1.0f);			   // Teal
	return P;
}

FRammsTypography URammsUIStyle::MakeDefaultTypography()
{
	FRammsTypography T;
	// Engine Roboto font for proportional text
	T.HeadingLarge = FCoreStyle::GetDefaultFontStyle("Bold", 32);
	T.HeadingMedium = FCoreStyle::GetDefaultFontStyle("Bold", 24);
	T.HeadingSmall = FCoreStyle::GetDefaultFontStyle("Bold", 18);
	T.Body = FCoreStyle::GetDefaultFontStyle("Regular", 14);
	T.Caption = FCoreStyle::GetDefaultFontStyle("Regular", 12);
	// Engine monospace font (DroidSansMono) via FCompositeFont
	TSharedPtr<FCompositeFont> MonoComposite = MakeShared<FCompositeFont>(
		FName("Regular"),
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansMono.ttf"),
		EFontHinting::Default,
		EFontLoadingPolicy::LazyLoad);
	T.Monospace = FSlateFontInfo(MonoComposite, 14);
	return T;
}

URammsUIStyle* URammsUIStyle::CreateDefaultDarkTheme()
{
	URammsUIStyle* Style = NewObject<URammsUIStyle>(
		GetTransientPackage(), NAME_None, RF_Transient);
	Style->StyleName = TEXT("Dark");
	Style->Typography = MakeDefaultTypography();
	return Style;
}

URammsUIStyle* URammsUIStyle::CreateDefaultLightTheme()
{
	URammsUIStyle* Style = NewObject<URammsUIStyle>(
		GetTransientPackage(), NAME_None, RF_Transient);
	Style->StyleName = TEXT("Light");
	Style->Colors = MakeLightPalette();
	Style->Typography = MakeDefaultTypography();

	// Adjust slider/scrollbar colors for light backgrounds
	Style->Slider.TrackColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);
	Style->Slider.ActiveBarColor = Style->Colors.Primary;
	Style->Slider.ThumbColor = Style->Colors.Primary;
	Style->Slider.DisabledColor = FLinearColor(0.75f, 0.75f, 0.75f, 1.0f);

	Style->ScrollBar.ThumbNormalColor = FLinearColor(0.7f, 0.7f, 0.7f, 0.5f);
	Style->ScrollBar.ThumbHoveredColor = FLinearColor(0.5f, 0.5f, 0.5f, 0.7f);
	Style->ScrollBar.ThumbDraggedColor = FLinearColor(0.4f, 0.4f, 0.4f, 0.9f);
	Style->ScrollBar.TrackColor = FLinearColor(0.9f, 0.9f, 0.9f, 0.3f);

	// Adjust checkbox for light backgrounds
	Style->CheckBox.UncheckedColor = FLinearColor(0.92f, 0.92f, 0.92f, 1.0f);
	Style->CheckBox.CheckedColor = Style->Colors.Primary;
	Style->CheckBox.BorderColor = FLinearColor(0.7f, 0.7f, 0.7f, 1.0f);
	Style->CheckBox.CheckMarkColor = FLinearColor::White;

	// Light-theme colour picker: dark thumb for contrast against bright gradients
	Style->ColorPicker.ThumbColor = Style->Colors.Primary;
	Style->ColorPicker.ThumbHoveredColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);
	Style->ColorPicker.ThumbOutlineColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.8f);
	Style->ColorPicker.SwatchBorderColor = FLinearColor(0.7f, 0.7f, 0.7f, 1.0f);

	return Style;
}

void URammsUIStyle::ResetToDarkDefaults()
{
	// Reset all fields to dark defaults (struct default constructors are dark)
	StyleName = TEXT("Dark");
	Colors = FRammsColorPalette();
	Typography = MakeDefaultTypography();
	Spacing = FRammsSpacing();
	Border = FRammsBorderStyle();
	ScrollBar = FRammsScrollBarStyle();
	Slider = FRammsSliderStyle();
	CheckBox = FRammsCheckBoxStyle();
	ColorPicker = FRammsColorPickerStyle();
	Interaction = FRammsInteractionStyle();
	FadeInCurve = FRammsAnimationCurve();
	FadeOutCurve = FRammsAnimationCurve();
	SlideCurve = FRammsAnimationCurve();
	ScaleCurve = FRammsAnimationCurve();
	ExpandCollapseCurve.Duration = 0.4f;
	ExpandCollapseCurve.Easing = ERammsUIEasing::EaseInOut;

	MarkPackageDirty();
}

void URammsUIStyle::ResetToLightDefaults()
{
	// Start from dark defaults then apply light overrides (same logic as CreateDefaultLightTheme)
	ResetToDarkDefaults();

	StyleName = TEXT("Light");
	Colors = MakeLightPalette();

	Slider.TrackColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);
	Slider.ActiveBarColor = Colors.Primary;
	Slider.ThumbColor = Colors.Primary;
	Slider.DisabledColor = FLinearColor(0.75f, 0.75f, 0.75f, 1.0f);

	ScrollBar.ThumbNormalColor = FLinearColor(0.7f, 0.7f, 0.7f, 0.5f);
	ScrollBar.ThumbHoveredColor = FLinearColor(0.5f, 0.5f, 0.5f, 0.7f);
	ScrollBar.ThumbDraggedColor = FLinearColor(0.4f, 0.4f, 0.4f, 0.9f);
	ScrollBar.TrackColor = FLinearColor(0.9f, 0.9f, 0.9f, 0.3f);

	CheckBox.UncheckedColor = FLinearColor(0.92f, 0.92f, 0.92f, 1.0f);
	CheckBox.CheckedColor = Colors.Primary;
	CheckBox.BorderColor = FLinearColor(0.7f, 0.7f, 0.7f, 1.0f);
	CheckBox.CheckMarkColor = FLinearColor::White;

	// Light-theme colour picker: dark thumb for contrast against bright gradients
	ColorPicker.ThumbColor = Colors.Primary;
	ColorPicker.ThumbHoveredColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);
	ColorPicker.ThumbOutlineColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.8f);
	ColorPicker.SwatchBorderColor = FLinearColor(0.7f, 0.7f, 0.7f, 1.0f);

	MarkPackageDirty();
}

void URammsUIStyle::CopyFrom(const URammsUIStyle* Source)
{
	if (!Source)
		return;

	StyleName = Source->StyleName;
	Colors = Source->Colors;
	Typography = Source->Typography;
	Spacing = Source->Spacing;
	Border = Source->Border;
	ScrollBar = Source->ScrollBar;
	Slider = Source->Slider;
	CheckBox = Source->CheckBox;
	ColorPicker = Source->ColorPicker;
	Interaction = Source->Interaction;
	FadeInCurve = Source->FadeInCurve;
	FadeOutCurve = Source->FadeOutCurve;
	SlideCurve = Source->SlideCurve;
	ScaleCurve = Source->ScaleCurve;
	ExpandCollapseCurve = Source->ExpandCollapseCurve;

	MarkPackageDirty();
}

#if WITH_EDITOR

URammsUIStyle* URammsUIStyle::DuplicateAsAsset(URammsUIStyle* Source, const FString& AssetPath)
{
	if (!Source)
	{
		UE_LOG(LogTemp, Warning, TEXT("DuplicateAsAsset: Source is null"));
		return nullptr;
	}

	const FString FullPath = TEXT("/Game/") + AssetPath;
	const FString AssetName = FPaths::GetBaseFilename(AssetPath);

	if (FindPackage(nullptr, *FullPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("DuplicateAsAsset: Asset already exists at '%s'"), *FullPath);
		return nullptr;
	}

	UPackage*	   Package = CreatePackage(*FullPath);
	URammsUIStyle* Clone = DuplicateObject<URammsUIStyle>(Source, Package, *AssetName);
	Clone->SetFlags(RF_Public | RF_Standalone);
	Clone->ClearFlags(RF_Transient);

	FAssetRegistryModule::AssetCreated(Clone);
	Package->MarkPackageDirty();

	const FString FilePath = FPackageName::LongPackageNameToFilename(
		FullPath, FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::Save(Package, Clone, *FilePath, SaveArgs);

	UE_LOG(LogTemp, Log, TEXT("DuplicateAsAsset: Saved '%s' to '%s'"),
		*Clone->StyleName, *FilePath);
	return Clone;
}

URammsUIStyle* URammsUIStyle::CreateAndSaveThemeAsset(const FString& AssetPath, bool bLight)
{
	const FString FullPath = TEXT("/Game/") + AssetPath;
	const FString AssetName = FPaths::GetBaseFilename(AssetPath);

	if (FindPackage(nullptr, *FullPath))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("CreateAndSaveThemeAsset: Asset already exists at '%s'"), *FullPath);
		return nullptr;
	}

	UPackage*	   Package = CreatePackage(*FullPath);
	URammsUIStyle* Style = NewObject<URammsUIStyle>(
		Package, *AssetName, RF_Public | RF_Standalone);

	Style->Typography = MakeDefaultTypography();

	if (bLight)
	{
		Style->StyleName = TEXT("Light");
		Style->Colors = MakeLightPalette();
		Style->Slider.TrackColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);
		Style->Slider.ActiveBarColor = Style->Colors.Primary;
		Style->Slider.ThumbColor = Style->Colors.Primary;
		Style->Slider.DisabledColor = FLinearColor(0.75f, 0.75f, 0.75f, 1.0f);
		Style->ScrollBar.ThumbNormalColor = FLinearColor(0.7f, 0.7f, 0.7f, 0.5f);
		Style->ScrollBar.ThumbHoveredColor = FLinearColor(0.5f, 0.5f, 0.5f, 0.7f);
		Style->ScrollBar.ThumbDraggedColor = FLinearColor(0.4f, 0.4f, 0.4f, 0.9f);
		Style->ScrollBar.TrackColor = FLinearColor(0.9f, 0.9f, 0.9f, 0.3f);
		Style->CheckBox.UncheckedColor = FLinearColor(0.92f, 0.92f, 0.92f, 1.0f);
		Style->CheckBox.CheckedColor = Style->Colors.Primary;
		Style->CheckBox.BorderColor = FLinearColor(0.7f, 0.7f, 0.7f, 1.0f);
		Style->CheckBox.CheckMarkColor = FLinearColor::White;
	}
	else
	{
		Style->StyleName = TEXT("Dark");
	}

	FAssetRegistryModule::AssetCreated(Style);
	Package->MarkPackageDirty();

	const FString FilePath = FPackageName::LongPackageNameToFilename(
		FullPath, FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::Save(Package, Style, *FilePath, SaveArgs);

	UE_LOG(LogTemp, Log, TEXT("CreateAndSaveThemeAsset: Saved %s theme to '%s'"),
		bLight ? TEXT("Light") : TEXT("Dark"), *FilePath);
	return Style;
}

#endif // WITH_EDITOR
