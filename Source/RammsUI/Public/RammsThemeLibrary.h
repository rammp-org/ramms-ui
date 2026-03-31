// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/RammsUIStyle.h"
#include "RammsThemeLibrary.generated.h"

/**
 * Static Blueprint function library for theme asset utilities.
 * Callable from any Blueprint (including Editor Utility Widgets) — no world context needed.
 */
UCLASS()
class RAMMSUI_API URammsThemeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/**
	 * Create a DataAsset pre-filled with dark theme defaults and save to Content/.
	 * @param AssetPath  Path relative to /Game/, e.g. "UI/DA_DarkTheme"
	 * @return The created asset, or nullptr if path already exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Theme")
	static URammsUIStyle* CreateDarkThemeAsset(const FString& AssetPath);

	/**
	 * Create a DataAsset pre-filled with light theme defaults and save to Content/.
	 * @param AssetPath  Path relative to /Game/, e.g. "UI/DA_LightTheme"
	 * @return The created asset, or nullptr if path already exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Theme")
	static URammsUIStyle* CreateLightThemeAsset(const FString& AssetPath);

	/**
	 * Clone any existing theme into a new saveable DataAsset.
	 * @param Source     The style to copy.
	 * @param AssetPath  Path relative to /Game/, e.g. "UI/DA_MyTheme"
	 * @return The cloned asset, or nullptr on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Theme")
	static URammsUIStyle* DuplicateThemeAsAsset(URammsUIStyle* Source, const FString& AssetPath);
#endif

	/**
	 * Copy all style data from Source into Target (colors, fonts, spacing, etc.).
	 * Target keeps its own identity (name, outer) — only values are overwritten.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ramms|Theme")
	static void CopyTheme(URammsUIStyle* Source, URammsUIStyle* Target);
};
