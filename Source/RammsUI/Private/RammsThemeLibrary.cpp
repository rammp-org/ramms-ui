// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsThemeLibrary.h"

#if WITH_EDITOR

URammsUIStyle* URammsThemeLibrary::CreateDarkThemeAsset(const FString& AssetPath)
{
	return URammsUIStyle::CreateAndSaveThemeAsset(AssetPath, /*bLight=*/false);
}

URammsUIStyle* URammsThemeLibrary::CreateLightThemeAsset(const FString& AssetPath)
{
	return URammsUIStyle::CreateAndSaveThemeAsset(AssetPath, /*bLight=*/true);
}

URammsUIStyle* URammsThemeLibrary::DuplicateThemeAsAsset(URammsUIStyle* Source, const FString& AssetPath)
{
	return URammsUIStyle::DuplicateAsAsset(Source, AssetPath);
}

#endif // WITH_EDITOR

void URammsThemeLibrary::CopyTheme(URammsUIStyle* Source, URammsUIStyle* Target)
{
	if (Source && Target)
	{
		Target->CopyFrom(Source);
	}
}
