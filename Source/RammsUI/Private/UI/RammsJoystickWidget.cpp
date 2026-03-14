// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsJoystickWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Styling/SlateBrush.h"
#include "Interfaces/IRammsRobotController.h"

URammsJoystickWidget::URammsJoystickWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoFindRobotController = true;
}

void URammsJoystickWidget::ResetCachedWidgets()
{
	JoystickCanvas = nullptr;
	BackgroundImage = nullptr;
	ThumbImage = nullptr;
}

void URammsJoystickWidget::BuildWidgetTree()
{
	if (!WidgetTree || JoystickCanvas)
		return;

	// Root: Canvas panel for absolute positioning of thumb
	JoystickCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("JoystickCanvas"));
	WidgetTree->RootWidget = JoystickCanvas;

	// Background circle (using RoundedBox brush for circular shape)
	BackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BackgroundImage"));
	{
		FSlateBrush BgBrush;
		BgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		BgBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		BgBrush.TintColor = FSlateColor(BackgroundColor);
		BackgroundImage->SetBrush(BgBrush);
	}
	BackgroundImage->SetColorAndOpacity(FLinearColor::White);
	UCanvasPanelSlot* BgSlot = JoystickCanvas->AddChildToCanvas(BackgroundImage);
	if (BgSlot)
	{
		float BgSize = JoystickRadius * 2.0f;
		BgSlot->SetPosition(FVector2D(0, 0));
		BgSlot->SetSize(FVector2D(BgSize, BgSize));
	}

	// Thumb (knob) — circular shape
	ThumbImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ThumbImage"));
	{
		FSlateBrush ThumbBrush;
		ThumbBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		ThumbBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		ThumbBrush.TintColor = FSlateColor(ThumbColor);
		ThumbImage->SetBrush(ThumbBrush);
	}
	ThumbImage->SetColorAndOpacity(FLinearColor::White);
	UCanvasPanelSlot* ThumbSlot = JoystickCanvas->AddChildToCanvas(ThumbImage);
	if (ThumbSlot)
	{
		float ThumbSize = ThumbRadius * 2.0f;
		// Center the thumb
		float CenterOffset = JoystickRadius - ThumbRadius;
		ThumbSlot->SetPosition(FVector2D(CenterOffset, CenterOffset));
		ThumbSlot->SetSize(FVector2D(ThumbSize, ThumbSize));
	}

	JoystickCenter = FVector2D(JoystickRadius, JoystickRadius);
}

void URammsJoystickWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void URammsJoystickWidget::NativeConstruct()
{
	SetIsFocusable(true);
	Super::NativeConstruct();
}

void URammsJoystickWidget::ApplyStyle_Implementation()
{
	if (!Style)
		return;

	// Use style colors if available
	ThumbColor = Style->Colors.Primary;
	ThumbActiveColor = Style->Colors.Info;
	BackgroundColor = Style->Colors.Surface;
	BackgroundColor.A = 0.5f;

	if (BackgroundImage)
	{
		FSlateBrush BgBrush = BackgroundImage->GetBrush();
		BgBrush.TintColor = FSlateColor(BackgroundColor);
		BackgroundImage->SetBrush(BgBrush);
	}
	if (ThumbImage && !bIsActive)
	{
		FSlateBrush ThBrush = ThumbImage->GetBrush();
		ThBrush.TintColor = FSlateColor(ThumbColor);
		ThumbImage->SetBrush(ThBrush);
	}
}

void URammsJoystickWidget::ResetToCenter()
{
	CurrentValue = FVector2D::ZeroVector;
	SetThumbOffset(FVector2D::ZeroVector);
	OnValueChanged.Broadcast(CurrentValue);
}

FReply URammsJoystickWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		bIsActive = true;

		if (ThumbImage)
		{
			FSlateBrush Brush = ThumbImage->GetBrush();
			Brush.TintColor = FSlateColor(ThumbActiveColor);
			ThumbImage->SetBrush(Brush);
		}

		UpdateThumbPosition(InGeometry, InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));

		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URammsJoystickWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsActive)
	{
		bIsActive = false;

		if (ThumbImage)
		{
			FSlateBrush Brush = ThumbImage->GetBrush();
			Brush.TintColor = FSlateColor(ThumbColor);
			ThumbImage->SetBrush(Brush);
		}

		if (bAutoCenter)
		{
			ResetToCenter();
		}

		OnReleased.Broadcast();
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply URammsJoystickWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsActive)
	{
		UpdateThumbPosition(InGeometry, InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
		return FReply::Handled();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void URammsJoystickWidget::UpdateThumbPosition(const FGeometry& InGeometry, FVector2D LocalPos)
{
	// Calculate offset from center
	FVector2D Offset = LocalPos - JoystickCenter;

	// Clamp to radius
	float Distance = Offset.Size();
	float MaxDistance = JoystickRadius - ThumbRadius;

	if (Distance > MaxDistance && MaxDistance > 0.0f)
	{
		Offset = Offset.GetSafeNormal() * MaxDistance;
		Distance = MaxDistance;
	}

	// Calculate normalized value (-1 to 1)
	FVector2D NormalizedValue = FVector2D::ZeroVector;
	if (MaxDistance > 0.0f)
	{
		NormalizedValue = Offset / MaxDistance;
	}

	// Apply dead zone
	float NormalizedDistance = NormalizedValue.Size();
	if (NormalizedDistance < DeadZone)
	{
		NormalizedValue = FVector2D::ZeroVector;
	}
	else
	{
		// Remap from [DeadZone, 1] to [0, 1]
		float RemappedDistance = (NormalizedDistance - DeadZone) / (1.0f - DeadZone);
		NormalizedValue = NormalizedValue.GetSafeNormal() * RemappedDistance;
	}

	// Lock to axis if enabled
	if (bLockToAxis && NormalizedValue.SizeSquared() > 0.0f)
	{
		if (FMath::Abs(NormalizedValue.X) > FMath::Abs(NormalizedValue.Y))
			NormalizedValue.Y = 0.0f;
		else
			NormalizedValue.X = 0.0f;
	}

	// Clamp final value
	NormalizedValue.X = FMath::Clamp(NormalizedValue.X, -1.0f, 1.0f);
	NormalizedValue.Y = FMath::Clamp(NormalizedValue.Y, -1.0f, 1.0f);

	CurrentValue = NormalizedValue;
	SetThumbOffset(Offset);
	OnValueChanged.Broadcast(CurrentValue);

	// Forward movement input to robot controller
	if (ResolvedControllerActor.IsValid())
	{
		IRammsRobotController::Execute_SendMovementInput(ResolvedControllerActor.Get(), CurrentValue);
	}
}

void URammsJoystickWidget::SetThumbOffset(FVector2D Offset)
{
	if (!ThumbImage)
		return;

	UCanvasPanelSlot* ThumbSlot = Cast<UCanvasPanelSlot>(ThumbImage->Slot);
	if (ThumbSlot)
	{
		FVector2D CenterPos = JoystickCenter - FVector2D(ThumbRadius, ThumbRadius);
		ThumbSlot->SetPosition(CenterPos + Offset);
	}
}

void URammsJoystickWidget::OnRobotControllerResolved(AActor* ControllerActor)
{
	UE_LOG(LogTemp, Log, TEXT("URammsJoystickWidget: Resolved robot controller '%s'"), *ControllerActor->GetName());
}
