// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsColorPicker.h"
#include "Blueprint/WidgetTree.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"

// ─── Helpers ─────────────────────────────────────────────────────────────────

/** Convert H (0–360), S (0–1), V (0–1) to an FLinearColor. */
static FLinearColor HSVToLinear(float H, float S, float V)
{
	// FLinearColor stores HSV as (H=R, S=G, V=B) in HSV representation
	return FLinearColor(H, S, V).HSVToLinearRGB();
}

// ─── Widget Tree ─────────────────────────────────────────────────────────────

void URammsColorPicker::ResetCachedWidgets()
{
	ContainerBorder = nullptr;
	ContentVBox = nullptr;
	PickerLabel = nullptr;
	HexLabel = nullptr;
	SwatchImage = nullptr;
	SwatchSizeBoxWidget = nullptr;
	HueOverlay = nullptr;
	HueGradientImage = nullptr;
	HueSlider = nullptr;
	SatOverlay = nullptr;
	SatGradientImage = nullptr;
	SatSlider = nullptr;
	BriOverlay = nullptr;
	BriGradientImage = nullptr;
	BriSlider = nullptr;
	HueTexture = nullptr;
	SatTexture = nullptr;
	BriTexture = nullptr;
}

/** Resolve the effective colour picker style from the theme + per-widget overrides. */
FRammsColorPickerStyle URammsColorPicker::ResolvePickerStyle() const
{
	FRammsColorPickerStyle Resolved;
	if (Style)
		Resolved = Style->ColorPicker;

	if (bOverrideGradientHeight)
		Resolved.GradientHeight = GradientHeight;
	if (bOverrideSwatchSize)
		Resolved.SwatchSize = SwatchSize;

	return Resolved;
}

void URammsColorPicker::BuildWidgetTree()
{
	if (!WidgetTree || HueSlider)
		return;

	FRammsColorPickerStyle PS = ResolvePickerStyle();

	// Root border panel
	ContainerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ContainerBorder"));
	ContainerBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(
		FLinearColor(0.06f, 0.06f, 0.08f, 0.92f), 4.0f,
		FLinearColor(0.3f, 0.3f, 0.3f, 1.0f), 1.0f);
	ContainerBorder->SetPadding(PanelPadding);
	ContainerBorder->SetClipping(EWidgetClipping::ClipToBounds);
	WidgetTree->RootWidget = ContainerBorder;

	ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentVBox"));
	ContainerBorder->AddChild(ContentVBox);

	// ── Row 0: Label + Swatch ──
	UHorizontalBox*	  HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderRow"));
	UVerticalBoxSlot* HeaderSlot = ContentVBox->AddChildToVerticalBox(HeaderRow);
	if (HeaderSlot)
	{
		HeaderSlot->SetHorizontalAlignment(HAlign_Fill);
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	PickerLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PickerLabel"));
	PickerLabel->SetText(LabelText);
	UHorizontalBoxSlot* LabelSlot = HeaderRow->AddChildToHorizontalBox(PickerLabel);
	if (LabelSlot)
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetHorizontalAlignment(HAlign_Left);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Hex value label (optional)
	HexLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HexLabel"));
	HexLabel->SetVisibility(bShowHexValue ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	UHorizontalBoxSlot* HexSlot = HeaderRow->AddChildToHorizontalBox(HexLabel);
	if (HexSlot)
	{
		HexSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		HexSlot->SetHorizontalAlignment(HAlign_Right);
		HexSlot->SetVerticalAlignment(VAlign_Center);
		HexSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	}

	// Colour swatch — render as a rounded solid-colour image (no border nesting needed)
	SwatchSizeBoxWidget = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SwatchSizeBox"));
	SwatchSizeBoxWidget->SetWidthOverride(PS.SwatchSize);
	SwatchSizeBoxWidget->SetHeightOverride(PS.SwatchSize);
	UHorizontalBoxSlot* SwatchSlot = HeaderRow->AddChildToHorizontalBox(SwatchSizeBoxWidget);
	if (SwatchSlot)
	{
		SwatchSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		SwatchSlot->SetVerticalAlignment(VAlign_Center);
	}

	SwatchImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SwatchImage"));
	UpdateSwatchBrush(PS);
	SwatchSizeBoxWidget->AddChild(SwatchImage);

	// ── Helper lambda: build a gradient+slider overlay row ──
	auto BuildGradientRow = [&](const FString& Name, TObjectPtr<UOverlay>& OutOverlay,
								TObjectPtr<UImage>& OutImage, TObjectPtr<USlider>& OutSlider) {
		OutOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("%sOverlay"), *Name));

		// Wrap in a SizeBox to enforce the gradient height
		USizeBox* RowSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("%sSizeBox"), *Name));
		RowSizeBox->SetHeightOverride(PS.GradientHeight);

		UVerticalBoxSlot* RowSlot = ContentVBox->AddChildToVerticalBox(RowSizeBox);
		if (RowSlot)
		{
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
			RowSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 2.0f));
		}

		RowSizeBox->AddChild(OutOverlay);

		// Gradient image (bottom layer)
		UBorder* GradientBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("%sGradientBorder"), *Name));
		GradientBorder->Background = URammsUIStyle::MakeRoundedBoxBrush(FLinearColor::Transparent, PS.GradientCornerRadius);
		GradientBorder->SetClipping(EWidgetClipping::ClipToBounds);
		UOverlaySlot* ImageSlot = OutOverlay->AddChildToOverlay(GradientBorder);
		if (ImageSlot)
		{
			ImageSlot->SetHorizontalAlignment(HAlign_Fill);
			ImageSlot->SetVerticalAlignment(VAlign_Fill);
		}

		OutImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("%sGradientImage"), *Name));
		GradientBorder->AddChild(OutImage);

		// Slider (top layer — transparent bar, visible thumb)
		OutSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), *FString::Printf(TEXT("%sSlider"), *Name));
		OutSlider->SetMinValue(0.0f);
		OutSlider->SetMaxValue(1.0f);
		OutSlider->SetStepSize(0.0f);
		UOverlaySlot* SliderSlot = OutOverlay->AddChildToOverlay(OutSlider);
		if (SliderSlot)
		{
			SliderSlot->SetHorizontalAlignment(HAlign_Fill);
			SliderSlot->SetVerticalAlignment(VAlign_Fill);
		}
	};

	// ── Hue row ──
	BuildGradientRow(TEXT("Hue"), HueOverlay, HueGradientImage, HueSlider);

	// ── Saturation row ──
	BuildGradientRow(TEXT("Sat"), SatOverlay, SatGradientImage, SatSlider);

	// ── Brightness row ──
	BuildGradientRow(TEXT("Bri"), BriOverlay, BriGradientImage, BriSlider);

	// Generate gradient textures
	HueTexture = GenerateHueGradient();
	SatTexture = GenerateSaturationGradient();
	BriTexture = GenerateBrightnessGradient();

	// Apply textures to images via brush
	if (HueGradientImage && HueTexture)
	{
		HueGradientImage->SetBrushFromTexture(HueTexture, true);
	}
	if (SatGradientImage && SatTexture)
	{
		SatGradientImage->SetBrushFromTexture(SatTexture, true);
	}
	if (BriGradientImage && BriTexture)
	{
		BriGradientImage->SetBrushFromTexture(BriTexture, true);
	}

	// Initial slider positions
	HueSlider->SetValue(Hue / 360.0f);
	SatSlider->SetValue(Saturation);
	BriSlider->SetValue(Brightness);

	// Visibility
	if (SatOverlay)
	{
		// The SizeBox parent is what we need to collapse
		if (UWidget* SatParent = SatOverlay->GetParent())
			SatParent->SetVisibility(bShowSaturation ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (BriOverlay)
	{
		if (UWidget* BriParent = BriOverlay->GetParent())
			BriParent->SetVisibility(bShowBrightness ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Hide panel if not wanted
	if (!bShowPanel)
	{
		ContainerBorder->Background = FSlateBrush();
		ContainerBorder->Background.DrawAs = ESlateBrushDrawType::NoDrawType;
		ContainerBorder->SetPadding(FMargin(0.0f));
	}

	// Make slider bars transparent
	MakeSliderBarTransparent(HueSlider);
	MakeSliderBarTransparent(SatSlider);
	MakeSliderBarTransparent(BriSlider);
}

// ─── Lifecycle ───────────────────────────────────────────────────────────────

void URammsColorPicker::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsColorPicker::NativeConstruct()
{
	Super::NativeConstruct();

	if (HueSlider)
	{
		HueSlider->SetValue(Hue / 360.0f);
		HueSlider->OnValueChanged.AddUniqueDynamic(this, &URammsColorPicker::OnHueSliderChanged);
	}
	if (SatSlider)
	{
		SatSlider->SetValue(Saturation);
		SatSlider->OnValueChanged.AddUniqueDynamic(this, &URammsColorPicker::OnSaturationSliderChanged);
	}
	if (BriSlider)
	{
		BriSlider->SetValue(Brightness);
		BriSlider->OnValueChanged.AddUniqueDynamic(this, &URammsColorPicker::OnBrightnessSliderChanged);
	}

	RefreshDependentGradients();
	UpdateSwatchAndLabels();
}

// ─── Style ───────────────────────────────────────────────────────────────────

void URammsColorPicker::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	FRammsColorPickerStyle PS = ResolvePickerStyle();

	float Radius = Style->Border.CornerRadiusMedium;
	float BorderW = Style->Border.BorderWidth;

	// Panel
	if (ContainerBorder && bShowPanel)
	{
		FLinearColor Bg = Style->Colors.Background;
		Bg.A = 0.92f;
		FSlateBrush Brush = bShowPanelBorder
			? URammsUIStyle::MakeRoundedBoxBrush(Bg, Radius, Style->Colors.Border, BorderW)
			: URammsUIStyle::MakeRoundedBoxBrush(Bg, Radius);
		URammsUIStyle::ApplyRoundedBrushToBorder(ContainerBorder, Brush);
		ContainerBorder->SetPadding(PanelPadding);
	}
	else if (ContainerBorder && !bShowPanel)
	{
		FSlateBrush NoBrush;
		NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		URammsUIStyle::ApplyRoundedBrushToBorder(ContainerBorder, NoBrush);
		ContainerBorder->SetPadding(FMargin(0.0f));
	}

	// Labels
	if (PickerLabel)
	{
		PickerLabel->SetFont(Style->Typography.Body);
		PickerLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextPrimary));
	}
	if (HexLabel)
	{
		HexLabel->SetFont(Style->Typography.Monospace);
		HexLabel->SetColorAndOpacity(FSlateColor(Style->Colors.TextSecondary));
	}

	// Swatch sizing + appearance
	if (SwatchSizeBoxWidget)
	{
		SwatchSizeBoxWidget->SetWidthOverride(PS.SwatchSize);
		SwatchSizeBoxWidget->SetHeightOverride(PS.SwatchSize);
	}
	UpdateSwatchBrush(PS);

	// Slider thumbs
	MakeSliderBarTransparent(HueSlider);
	MakeSliderBarTransparent(SatSlider);
	MakeSliderBarTransparent(BriSlider);
}

void URammsColorPicker::MakeSliderBarTransparent(USlider* Slider) const
{
	if (!Slider)
		return;

	FRammsColorPickerStyle PS = ResolvePickerStyle();

	float ThumbDiameter = PS.GetEffectiveThumbSize(PS.GradientHeight);
	float ThumbRadius = ThumbDiameter * 0.5f;

	// Transparent bar brushes
	FSlateBrush TransparentBar;
	TransparentBar.DrawAs = ESlateBrushDrawType::NoDrawType;

	// Thumb brushes with outline for visibility against any gradient colour
	FSlateBrush ThumbNormal = URammsUIStyle::MakeRoundedBoxBrush(
		PS.ThumbColor, ThumbRadius,
		PS.ThumbOutlineColor, PS.ThumbOutlineWidth);
	ThumbNormal.SetImageSize(FVector2D(ThumbDiameter, ThumbDiameter));

	FSlateBrush ThumbHovered = URammsUIStyle::MakeRoundedBoxBrush(
		PS.ThumbHoveredColor, ThumbRadius,
		PS.ThumbOutlineColor, PS.ThumbOutlineWidth + 0.5f);
	ThumbHovered.SetImageSize(FVector2D(ThumbDiameter, ThumbDiameter));

	FSliderStyle SliderStyle = FSliderStyle::GetDefault();
	SliderStyle.SetNormalBarImage(TransparentBar);
	SliderStyle.SetHoveredBarImage(TransparentBar);
	SliderStyle.SetDisabledBarImage(TransparentBar);
	SliderStyle.SetNormalThumbImage(ThumbNormal);
	SliderStyle.SetHoveredThumbImage(ThumbHovered);
	SliderStyle.SetDisabledThumbImage(ThumbNormal);
	SliderStyle.SetBarThickness(PS.GradientHeight);

	Slider->SetWidgetStyle(SliderStyle);
	Slider->SetSliderBarColor(FLinearColor::Transparent);
	Slider->SetSliderHandleColor(FLinearColor::White);

	if (Slider->GetCachedWidget().IsValid())
	{
		Slider->SynchronizeProperties();
	}
}

// ─── Gradient Texture Generation ─────────────────────────────────────────────

UTexture2D* URammsColorPicker::GenerateHueGradient() const
{
	UTexture2D* Tex = UTexture2D::CreateTransient(GRADIENT_WIDTH, 1, PF_B8G8R8A8);
	if (!Tex)
		return nullptr;

	Tex->Filter = TF_Bilinear;
	Tex->SRGB = true;
	Tex->AddressX = TA_Clamp;
	Tex->AddressY = TA_Clamp;

	FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
	void*			  Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FColor*			  Pixels = static_cast<FColor*>(Data);

	for (int32 X = 0; X < GRADIENT_WIDTH; ++X)
	{
		float		 H = (static_cast<float>(X) / (GRADIENT_WIDTH - 1)) * 360.0f;
		FLinearColor Col = HSVToLinear(H, 1.0f, 1.0f);
		Pixels[X] = Col.ToFColor(true);
	}

	Mip.BulkData.Unlock();
	Tex->UpdateResource();
	return Tex;
}

UTexture2D* URammsColorPicker::GenerateSaturationGradient() const
{
	UTexture2D* Tex = UTexture2D::CreateTransient(GRADIENT_WIDTH, 1, PF_B8G8R8A8);
	if (!Tex)
		return nullptr;

	Tex->Filter = TF_Bilinear;
	Tex->SRGB = true;
	Tex->AddressX = TA_Clamp;
	Tex->AddressY = TA_Clamp;

	FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
	void*			  Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FColor*			  Pixels = static_cast<FColor*>(Data);

	for (int32 X = 0; X < GRADIENT_WIDTH; ++X)
	{
		float		 S = static_cast<float>(X) / (GRADIENT_WIDTH - 1);
		FLinearColor Col = HSVToLinear(Hue, S, 1.0f);
		Pixels[X] = Col.ToFColor(true);
	}

	Mip.BulkData.Unlock();
	Tex->UpdateResource();
	return Tex;
}

UTexture2D* URammsColorPicker::GenerateBrightnessGradient() const
{
	UTexture2D* Tex = UTexture2D::CreateTransient(GRADIENT_WIDTH, 1, PF_B8G8R8A8);
	if (!Tex)
		return nullptr;

	Tex->Filter = TF_Bilinear;
	Tex->SRGB = true;
	Tex->AddressX = TA_Clamp;
	Tex->AddressY = TA_Clamp;

	FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
	void*			  Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FColor*			  Pixels = static_cast<FColor*>(Data);

	for (int32 X = 0; X < GRADIENT_WIDTH; ++X)
	{
		float		 V = static_cast<float>(X) / (GRADIENT_WIDTH - 1);
		FLinearColor Col = HSVToLinear(Hue, Saturation, V);
		Pixels[X] = Col.ToFColor(true);
	}

	Mip.BulkData.Unlock();
	Tex->UpdateResource();
	return Tex;
}

void URammsColorPicker::UpdateTextureData(UTexture2D* Texture, const TArray<FColor>& Pixels)
{
	if (!Texture || Pixels.Num() != GRADIENT_WIDTH)
		return;

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void*			  Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
}

void URammsColorPicker::RefreshSaturationGradient()
{
	if (!SatTexture)
		return;

	TArray<FColor> Pixels;
	Pixels.SetNum(GRADIENT_WIDTH);
	for (int32 X = 0; X < GRADIENT_WIDTH; ++X)
	{
		float S = static_cast<float>(X) / (GRADIENT_WIDTH - 1);
		Pixels[X] = HSVToLinear(Hue, S, 1.0f).ToFColor(true);
	}
	UpdateTextureData(SatTexture, Pixels);
}

void URammsColorPicker::RefreshBrightnessGradient()
{
	if (!BriTexture)
		return;

	TArray<FColor> Pixels;
	Pixels.SetNum(GRADIENT_WIDTH);
	for (int32 X = 0; X < GRADIENT_WIDTH; ++X)
	{
		float V = static_cast<float>(X) / (GRADIENT_WIDTH - 1);
		Pixels[X] = HSVToLinear(Hue, Saturation, V).ToFColor(true);
	}
	UpdateTextureData(BriTexture, Pixels);
}

void URammsColorPicker::RefreshDependentGradients()
{
	RefreshSaturationGradient();
	RefreshBrightnessGradient();
}

void URammsColorPicker::UpdateSwatchBrush(const FRammsColorPickerStyle& PS)
{
	if (!SwatchImage)
		return;

	FLinearColor Col = GetColor();
	FSlateBrush	 Brush = URammsUIStyle::MakeRoundedBoxBrush(
		 Col, PS.SwatchCornerRadius,
		 PS.SwatchBorderColor, 1.0f);
	SwatchImage->SetBrush(Brush);
	SwatchImage->SetColorAndOpacity(FLinearColor::White);
}

void URammsColorPicker::UpdateSwatchAndLabels()
{
	UpdateSwatchBrush(ResolvePickerStyle());

	if (HexLabel && bShowHexValue)
	{
		FLinearColor CurrentColor = GetColor();
		FColor		 Quantized = CurrentColor.ToFColor(true);
		FString		 Hex = FString::Printf(TEXT("#%02X%02X%02X"), Quantized.R, Quantized.G, Quantized.B);
		HexLabel->SetText(FText::FromString(Hex));
	}
}

// ─── Slider Callbacks ────────────────────────────────────────────────────────

void URammsColorPicker::OnHueSliderChanged(float NewValue)
{
	Hue = NewValue * 360.0f;
	RefreshDependentGradients();
	UpdateSwatchAndLabels();
	OnColorChanged.Broadcast(GetColor());
	OnHueChanged.Broadcast(Hue);
}

void URammsColorPicker::OnSaturationSliderChanged(float NewValue)
{
	Saturation = NewValue;
	RefreshBrightnessGradient();
	UpdateSwatchAndLabels();
	OnColorChanged.Broadcast(GetColor());
}

void URammsColorPicker::OnBrightnessSliderChanged(float NewValue)
{
	Brightness = NewValue;
	UpdateSwatchAndLabels();
	OnColorChanged.Broadcast(GetColor());
}

void URammsColorPicker::BroadcastChange()
{
	OnColorChanged.Broadcast(GetColor());
}

// ─── Public API ──────────────────────────────────────────────────────────────

FLinearColor URammsColorPicker::GetColor() const
{
	return HSVToLinear(Hue, Saturation, Brightness);
}

void URammsColorPicker::SetColor(FLinearColor Color)
{
	FLinearColor HSV = Color.LinearRGBToHSV();
	Hue = HSV.R;
	Saturation = HSV.G;
	Brightness = HSV.B;

	if (HueSlider)
		HueSlider->SetValue(Hue / 360.0f);
	if (SatSlider)
		SatSlider->SetValue(Saturation);
	if (BriSlider)
		BriSlider->SetValue(Brightness);

	RefreshDependentGradients();
	UpdateSwatchAndLabels();
}

void URammsColorPicker::SetHue(float NewHue)
{
	Hue = FMath::Clamp(NewHue, 0.0f, 360.0f);
	if (HueSlider)
		HueSlider->SetValue(Hue / 360.0f);
	RefreshDependentGradients();
	UpdateSwatchAndLabels();
}

void URammsColorPicker::SetSaturation(float NewSaturation)
{
	Saturation = FMath::Clamp(NewSaturation, 0.0f, 1.0f);
	if (SatSlider)
		SatSlider->SetValue(Saturation);
	RefreshDependentGradients();
	UpdateSwatchAndLabels();
}

void URammsColorPicker::SetBrightness(float NewBrightness)
{
	Brightness = FMath::Clamp(NewBrightness, 0.0f, 1.0f);
	if (BriSlider)
		BriSlider->SetValue(Brightness);
	UpdateSwatchAndLabels();
}

void URammsColorPicker::SetLabel(FText Label)
{
	LabelText = Label;
	if (PickerLabel)
		PickerLabel->SetText(LabelText);
}

void URammsColorPicker::SetShowSaturation(bool bShow)
{
	bShowSaturation = bShow;
	if (SatOverlay)
	{
		if (UWidget* Parent = SatOverlay->GetParent())
			Parent->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void URammsColorPicker::SetShowBrightness(bool bShow)
{
	bShowBrightness = bShow;
	if (BriOverlay)
	{
		if (UWidget* Parent = BriOverlay->GetParent())
			Parent->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void URammsColorPicker::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (PickerLabel)
		PickerLabel->SetText(LabelText);

	if (HueSlider)
		HueSlider->SetValue(Hue / 360.0f);
	if (SatSlider)
		SatSlider->SetValue(Saturation);
	if (BriSlider)
		BriSlider->SetValue(Brightness);

	if (HexLabel)
		HexLabel->SetVisibility(bShowHexValue ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	SetShowSaturation(bShowSaturation);
	SetShowBrightness(bShowBrightness);

	RefreshDependentGradients();
	UpdateSwatchAndLabels();

	MakeSliderBarTransparent(HueSlider);
	MakeSliderBarTransparent(SatSlider);
	MakeSliderBarTransparent(BriSlider);
}
