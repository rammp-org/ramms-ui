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
	 * Only show objects whose "source" is listed, case-insensitively. EMPTY SHOWS EVERY SOURCE,
	 * including an empty one -- unlike URammsCameraWidget::BridgeDetectionSources, which falls back
	 * to its own stream. This layer is not one camera's, and the bridge's clears carry no stream_id.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	TArray<FString> AcceptedSources;

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
	virtual void	 ResetCachedWidgets() override;
	virtual UWidget* GetRootWidgetForValidation() override;

private:
	UFUNCTION()
	void HandleDetectedObjects(const FRammsBridgeDetectedObjects& InObjects);

	bool		 IsAcceptedSource(const FString& Source) const;
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
};
