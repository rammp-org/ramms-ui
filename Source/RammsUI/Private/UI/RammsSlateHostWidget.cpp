// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RammsSlateHostWidget.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

void URammsSlateHostWidget::SetSlateContent(TSharedPtr<SWidget> InContent)
{
	Content = InContent;
	if (Box.IsValid())
	{
		Box->SetContent(Content.IsValid() ? Content.ToSharedRef() : SNullWidget::NullWidget);
	}
}

TSharedRef<SWidget> URammsSlateHostWidget::RebuildWidget()
{
	Box = SNew(SBox).Padding(FMargin(6.0f, 4.0f))[Content.IsValid() ? Content.ToSharedRef() : SNullWidget::NullWidget];
	return Box.ToSharedRef();
}

void URammsSlateHostWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Box.Reset();
}
