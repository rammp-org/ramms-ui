// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RammsUI : ModuleRules
{
	public RammsUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"RHI", "RenderCore", // Needed to use the RenderRequest / RHI framework
				"Projects",
				"UMG", // For UUserWidget and UMG components
				"Slate", "SlateCore", // For Slate/UMG styling (FSlateFontInfo, etc.)
				"RammsStreaming", // Public: RammsStreamProtocol.h exposed via RammsStreamCameraBridge.h
				"RammsControl", // control-surface model the panels render / drive
				"DeveloperSettings", // URammsControlHUDSettings
				"ProceduralMeshComponent"
				// ... add other public dependencies that you statically link with here ...
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Json"           // FJsonObject/FJsonSerializer used by StreamCameraBridge
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
