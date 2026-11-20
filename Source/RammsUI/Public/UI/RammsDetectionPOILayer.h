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
class IRammsCameraProvider;
class URammsDetectionPOILayer;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FRammsDetectionPOITapped, UUserWidget*, POI, const FRammsBridgeDetectedObject&, DetectedObject);

/**
 * Listens to one POI's selection dispatcher on the layer's behalf, so the layer learns WHICH POI
 * fired: a dispatcher bound straight to the layer could not say who sent it.
 *
 * The handler takes no parameters on purpose. The dispatcher is a Blueprint one, found by name, and
 * its signature is whatever the widget gives it; a parameterless UFUNCTION reads nothing from the
 * broadcast's parameter block, so it is safe against any signature and the widget can change its
 * dispatcher's inputs without breaking this.
 */
UCLASS(Transient)
class RAMMSUI_API URammsDetectionPOITapListener : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleSelected();

	TWeakObjectPtr<URammsDetectionPOILayer> Layer;
	TWeakObjectPtr<UUserWidget>				POI;

	/** The object this POI was last given, so a tap can say what was tapped. */
	FRammsBridgeDetectedObject Object;
};

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
	 * Frame size that pixel coordinates are divided by when this layer's camera cannot be found --
	 * the same fallback, and the same default, as URammsCameraWidget::FallbackDetectionImageSize.
	 *
	 * A detection whose values are all within 0-1 is taken as normalized and is not divided at
	 * all, so this only matters for a producer sending pixels.
	 *
	 * The camera is this layer's StreamSet, NOT the object's `source`: source is a detection alias
	 * ("", "scene") for a stream registered under another name ("camera/scene/color"), so looking a
	 * provider up by it never matches. A layer left on Custom has no stream to ask about and always
	 * uses this value; it says so once, at Warning, the first time it has to.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	FVector2D FallbackDetectionImageSize = FVector2D(640.0, 480.0);

	/**
	 * Show objects that arrive with no instance_id. Nothing can key a widget on them, so they are
	 * reused in order and a tap on one has no id to send.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	bool bShowObjectsWithoutIds = true;

	/**
	 * Leave out every object that has a world marker (an ARammsDetectionWorldMarker for the same
	 * instance_id, in this layer's world). The marker's box and its label (RammsDetectionWorldLabels)
	 * already show that object in the 3D view this layer floats over; a POI beside it says the same
	 * thing twice, in another style, and the two drift apart as the view moves. Objects with no marker
	 * -- 2D only, occluded, no depth at their centre -- keep their POI. Re-checked every tick, so a
	 * POI steps aside as soon as its object's marker appears and comes back if the marker goes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	bool bHideObjectsWithWorldMarkers = true;

	/**
	 * The dispatcher on POIWidgetClass that means "this POI was tapped" (RAMMS_POI: OnSelected).
	 * The layer binds to it once per POI it creates and unbinds when the POI leaves the pool. Its
	 * inputs do not matter: the layer already knows which POI and object it is. None turns this off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection")
	FName POISelectedDispatcherName = TEXT("OnSelected");

	/**
	 * Opacity of every POI except the last tapped one, while the last tapped object (by instance_id)
	 * is on this layer. With nothing tapped, or the tapped object no longer listed, every POI is
	 * fully opaque. Applied through the POI's Color And Opacity, which multiplies with the Render
	 * Opacity the POI sets for its own edge fade, so the two do not overwrite each other. 1 turns
	 * the dimming off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAMMS|Detection", meta = (ClampMin = "0", ClampMax = "1"))
	float UnselectedPOIOpacity = 0.4f;

	/** A POI on this layer was tapped (its selection dispatcher fired). */
	UPROPERTY(BlueprintAssignable, Category = "RAMMS|Detection")
	FRammsDetectionPOITapped OnPOITapped;

	/**
	 * The POI tapped most recently, or None if there has not been one or it has since been removed
	 * (its object left the push). Use Get Last Tapped Object for what it showed.
	 */
	UFUNCTION(BlueprintPure, Category = "RAMMS|Detection")
	UUserWidget* GetLastTappedPOI() const { return LastTappedPOI; }

	/**
	 * The object the last tapped POI was showing when it was tapped. Kept after the POI goes, so a
	 * tap is not forgotten just because the next push no longer lists the object. False if nothing
	 * has been tapped since this layer was constructed or the last Clear Last Tapped.
	 */
	UFUNCTION(BlueprintPure, Category = "RAMMS|Detection")
	bool GetLastTappedObject(FRammsBridgeDetectedObject& OutObject) const;

	/** Forget the last tap. */
	UFUNCTION(BlueprintCallable, Category = "RAMMS|Detection")
	void ClearLastTapped();

	/** Called by a POI's listener. */
	void NotifyPOITapped(UUserWidget* POI, const FRammsBridgeDetectedObject& Object);

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
	FVector2D ResolvePixelDivisor();

	/** This layer's camera, found once and remembered so the sweep is not repeated per push. */
	IRammsCameraProvider* FindStreamProvider(const FString& StreamID);
	UUserWidget*		  CreatePOI();

	/** Bind a new POI's selection dispatcher to a listener of its own. */
	void BindPOI(UUserWidget* POI);

	/** Take a POI out of the pool: unbind it, take it off the canvas, forget it as last tapped. */
	void ReleasePOI(UUserWidget* POI);

	/** Full opacity for the last tapped POI, UnselectedPOIOpacity for the rest, or full for all. */
	void ApplySelectionOpacity();

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

	/** Likewise for a POI class with no dispatcher called POISelectedDispatcherName. */
	bool bWarnedAboutDispatcher = false;

	/** One listener per pooled POI, so each is bound once and unbound when it leaves. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UUserWidget>, TObjectPtr<URammsDetectionPOITapListener>> TapListeners;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LastTappedPOI;

	UPROPERTY(Transient)
	FRammsBridgeDetectedObject LastTappedObject;

	bool bHasLastTapped = false;

	/** Likewise for a duplicate instance_id. */
	bool bWarnedAboutDuplicateId = false;

	/** The divisor last reported, so a change is said rather than swallowed by a one-shot flag. */
	FVector2D LastReportedDivisor = FVector2D::ZeroVector;

	/**
	 * The provider serving this layer's stream, not its size: the size is read live from the
	 * stream's texture each time, the way URammsCameraWidget::GetImageSize does, so a producer
	 * that changes resolution does not leave the two surfaces disagreeing.
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> CachedProvider;

	/** So a Custom layer says once why it is on the fallback, rather than silently being wrong. */
	bool bWarnedAboutCustomStreamSet = false;

	/** The last payload, re-applied when the layer's size changes (see NativeTick). */
	UPROPERTY(Transient)
	FRammsBridgeDetectedObjects LastObjects;

	/** The instance ids that had a world marker when the POIs were last placed (see NativeTick). */
	TSet<FName> LastMarkedIds;

	/** Instance ids of the world markers in this layer's world. */
	TSet<FName> CollectWorldMarkedIds() const;

	/** The size the POIs were last placed for. */
	FVector2D LastLayoutSize = FVector2D::ZeroVector;

	/** The class the pooled POIs were made from, so changing POIWidgetClass can reset the pool. */
	UPROPERTY(Transient)
	TSubclassOf<UUserWidget> PooledClass;
};
