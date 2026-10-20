// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "RammsSlateHostWidget.generated.h"

class SBox;

/**
 * A UMG widget that shows a Slate widget made elsewhere: how the HUD hosts a
 * panel a component offers as Slate (IRammsHUDPanelProvider) in a
 * URammsFloatingPanel like any other.
 */
UCLASS()
class RAMMSUI_API URammsSlateHostWidget : public UWidget
{
	GENERATED_BODY()

public:
	void SetSlateContent(TSharedPtr<SWidget> InContent);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void				ReleaseSlateResources(bool bReleaseChildren) override;

private:
	TSharedPtr<SWidget> Content;
	TSharedPtr<SBox>	Box;
};
