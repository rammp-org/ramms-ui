// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RammsUI.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FRammsUIModule"

void FRammsUIModule::StartupModule()
{
	// Register shader directory so materials can #include "/RammsUI/..."
	FString PluginShaderDir = FPaths::Combine(
		IPluginManager::Get().FindPlugin(TEXT("RammsUI"))->GetBaseDir(),
		TEXT("Shaders"));

	if (FPaths::DirectoryExists(PluginShaderDir))
	{
		AddShaderSourceDirectoryMapping(TEXT("/RammsUI"), PluginShaderDir);
		UE_LOG(LogTemp, Log, TEXT("RammsUI: Registered shader directory: %s"), *PluginShaderDir);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("RammsUI: Shader directory not found: %s"), *PluginShaderDir);
	}
}

void FRammsUIModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRammsUIModule, RammsUI)
