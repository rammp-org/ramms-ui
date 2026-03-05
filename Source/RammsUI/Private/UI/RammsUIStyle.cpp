// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsUIStyle.h"
#include "Components/Border.h"

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
