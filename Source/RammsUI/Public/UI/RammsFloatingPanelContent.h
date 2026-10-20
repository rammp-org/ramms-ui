// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RammsFloatingPanelContent.generated.h"

UINTERFACE(MinimalAPI)
class URammsFloatingPanelContent : public UInterface
{
	GENERATED_BODY()
};

/**
 * Optional, for a widget a URammsFloatingPanel hosts: the title its title bar
 * shows, and a say in its own chrome -- hosted, a panel drops its own frame and
 * title, which the floating panel's replace.
 */
class RAMMSUI_API IRammsFloatingPanelContent
{
	GENERATED_BODY()

public:
	/** The title for the title bar; empty keeps the floating panel's own. */
	virtual FText GetFloatingTitle() const { return FText::GetEmpty(); }

	/** Now hosted in a floating panel (or no longer). */
	virtual void SetHostedInFloatingPanel(bool bHosted) {}
};
