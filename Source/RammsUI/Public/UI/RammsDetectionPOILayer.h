// RammsDetectionPOILayer.h
//
// A full-screen layer of POI widgets driven straight by the bridge.
//
// Place it over the view, give it a POI class that implements IRammsDetectionPOI, and it keeps
// exactly one widget per detected object: reusing the one an object already had (by instance_id),
// and removing the widgets of objects the push no longer lists.
//
// That last half is the point. A layer that only adds shows objects that are no longer there: an
// empty "objects" array leaves the last set on screen, and a new set stacks on top of the old. Every
// push is the full set, so what a push leaves out is over.
//
// It subscribes to RammsBridgeSubsystem::OnDetectedObjects, the payload already parsed into
// FRammsBridgeDetectedObject, so nothing here parses JSON a second time and a POI gets the same
// struct the rest of the UI sees -- including Source, which is what a tap sends back as its stream.

#pragma once

#include "CoreMinimal.h"
#include "RammsBridgeSubsystem.h"
#include "RammsCameraStreamSettings.h"
#include "UI/RammsBaseWidget.h"
#include "RammsDetectionPOILayer.generated.h"

class UCanvasPanel;

UCLASS(meta = (DisplayName = "Ramms Detection POI Layer"))
class RAMMSUI_API URammsDetectionPOILayer : public URammsBaseWidget
{
	GENERATED_BODY()

public:
	URammsDetectionPOILayer();

	/**
	 * Widget placed on each object. Must implement Ramms Detection POI (Class Settings >
	 * Interfaces > Add); one that does not is refused, with a warning, rather than placed blank.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	TSubclassOf<UUserWidget> POIWidgetClass;

	/** Follow the bridge's pushes. Turn off to drive the layer yourself with Set Detected Objects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	bool bReceiveBridgeDetections = true;

	/**
	 * Which camera's objects this layer shows. Scene / Engine overlay: that set's DetectionSources
	 * from Project Settings > Ramms Camera Streams, in place of AcceptedSources, read when the
	 * widget is constructed -- the same selector URammsCameraWidget uses, so a layer and the camera
	 * it sits over cannot drift apart. Custom: this layer's own AcceptedSources.
	 *
	 * A layer per camera is the normal setup. Detection coordinates are normalized to the image the
	 * object was found in, so a layer must cover that camera's image rect and show only that
	 * camera's objects; one layer showing every camera would place another camera's objects against
	 * the wrong rectangle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RAMMS|Detection")
	ERammsCameraStreamSet StreamSet = ERammsCameraStreamSet::Custom;

	/**
	 * Only show objects whose "source" is listed, case-insensitively; used when StreamSet is Custom,
	 * and overwritten by the set's sources otherwise. Empty shows every source, including an empty
	 * one -- fine for a single full-screen layer, wrong as soon as there is more than one.
	 *
	 * Filtering does not stop a clear: an empty "objects" array has nothing to filter, so the layer
	 * still empties.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	TArray<FString> AcceptedSources;

	/**
	 * Frame size that pixel coordinates are divided by when the camera that produced them cannot
	 * be found in the level -- the same fallback, and the same default, as
	 * URammsCameraWidget::FallbackDetectionImageSize.
	 *
	 * A detection whose values are all within 0-1 is taken as normalized and is not divided at
	 * all, so this only matters for a producer sending pixels. When the object's `source` names a
	 * camera that is present, that camera's own frame size wins over this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	FVector2D FallbackDetectionImageSize = FVector2D(640.0, 480.0);

	/**
	 * Show objects that arrive with no instance_id. Nothing can key a widget on them, so they are
	 * reused in order and a tap on one has no id to send.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	bool bShowObjectsWithoutIds = true;

	/** Match the layer to one payload: reuse, create and remove. An empty one empties the layer. */
	UFUNCTION(BlueprintCallable, Category = "RAMMS|Detection")
	void SetDetectedObjects(const FRammsBridgeDetectedObjects& InObjects);

	/** Remove every POI. */
	UFUNCTION(BlueprintCallable, Category = "RAMMS|Detection")
	void ClearPOIs();

	UFUNCTION(BlueprintPure, Category = "RAMMS|Detection")
	int32 GetPOICount() const;

	/** The canvas the POIs are on, for anything that needs to measure or add to it. */
	UFUNCTION(BlueprintPure, Category = "RAMMS|Detection")
	UCanvasPanel* GetPOICanvas() const { return POICanvas; }

protected:
	virtual void	 BuildWidgetTree() override;
	virtual void	 NativeConstruct() override;
	virtual void	 NativeDestruct() override;
	virtual void	 NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void	 ResetCachedWidgets() override;
	virtual UWidget* GetRootWidgetForValidation() override;

private:
	UFUNCTION()
	void HandleDetectedObjects(const FRammsBridgeDetectedObjects& InObjects);

	bool IsAcceptedSource(const FString& Source) const;

	/**
	 * The frame size that pixel coordinates are divided by: this layer's own camera, found once
	 * and remembered, else FallbackDetectionImageSize.
	 */
	FVector2D	 ResolvePixelDivisor();
	UUserWidget* CreatePOI();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> POICanvas;

	/** One POI per instance_id, kept across pushes so a POI is not rebuilt every frame. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UUserWidget>> POIsById;

	/** POIs for objects with no instance_id, reused in the order they arrive. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidget>> UnkeyedPOIs;

	/** So a POI class that does not implement the interface is reported once, not once per push. */
	bool bWarnedAboutClass = false;

	/** Likewise for a missing canvas, which is permanent once it happens. */
	bool bWarnedAboutCanvas = false;

	/** Likewise for a duplicate instance_id. */
	bool bWarnedAboutDuplicateId = false;

	/** The divisor last reported, so a change is said rather than swallowed by a one-shot flag. */
	FVector2D LastReportedDivisor = FVector2D::ZeroVector;

	/** A frame size found from a provider. Zero until one is found, so a late provider still is. */
	FVector2D ResolvedImageSize = FVector2D::ZeroVector;

	/** The last payload, re-applied when the layer's size changes (see NativeTick). */
	UPROPERTY(Transient)
	FRammsBridgeDetectedObjects LastObjects;

	/** The size the POIs were last placed for. */
	FVector2D LastLayoutSize = FVector2D::ZeroVector;

	/** The class the pooled POIs were made from, so changing POIWidgetClass can reset the pool. */
	UPROPERTY(Transient)
	TSubclassOf<UUserWidget> PooledClass;
};
