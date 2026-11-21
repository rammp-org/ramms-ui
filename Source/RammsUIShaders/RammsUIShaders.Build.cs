// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

// RammsUI's shader directory (/RammsUI), mapped at PostConfigInit: before any RammsUI class is
// constructed, so a material a constructor loads (URammsCameraWidget's default M_CameraImage) finds
// its #include.
public class RammsUIShaders : ModuleRules
{
	public RammsUIShaders(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Projects",
				"RenderCore",
			}
			);
	}
}
