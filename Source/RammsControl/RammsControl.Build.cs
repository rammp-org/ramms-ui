// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

// Dependency-light "control surface" model shared by the UI (RammsUI) and by
// robot implementations (RammsCore in ramms-sim, the HMI's RMSS bridge): how a
// robot describes its controls and how any input source drives them. No UMG,
// no physics — so a project can implement or render a control surface without
// pulling in either side.
public class RammsControl : ModuleRules
{
	public RammsControl(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"EnhancedInput", // URammsControlInputMap references UInputAction / UInputMappingContext
		});
	}
}
