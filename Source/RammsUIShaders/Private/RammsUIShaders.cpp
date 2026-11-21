// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

DEFINE_LOG_CATEGORY_STATIC(LogRammsUIShaders, Log, All);

/**
 * Maps RammsUI's shader directory to /RammsUI, so materials can #include "/RammsUI/...".
 *
 * A module of its own, loaded at PostConfigInit: the RammsUI module is loaded later, and its classes'
 * constructors run before its StartupModule -- URammsCameraWidget's loads M_CameraImage, whose custom
 * node includes /RammsUI/Private/RammsRoundedCorners.ush, and a material loaded before its include's
 * mapping exists drops the include from its cached data ("Expression include file path ... is invalid").
 */
class FRammsUIShadersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("RammsUI"));
		const FString			  ShaderDir = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders")) : FString();
		if (!ShaderDir.IsEmpty() && FPaths::DirectoryExists(ShaderDir))
		{
			AddShaderSourceDirectoryMapping(TEXT("/RammsUI"), ShaderDir);
			UE_LOG(LogRammsUIShaders, Log, TEXT("RammsUI: registered shader directory %s"), *ShaderDir);
		}
		else
		{
			UE_LOG(LogRammsUIShaders, Warning, TEXT("RammsUI: shader directory not found (%s)"), *ShaderDir);
		}
	}
};

IMPLEMENT_MODULE(FRammsUIShadersModule, RammsUIShaders)
