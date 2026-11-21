// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RammsUI.h"
#include "RammsTouchLinux.h"
#include "Core.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FRammsUIModule"

void FRammsUIModule::StartupModule()
{
	// (The shader directory, /RammsUI, is mapped by the RammsUIShaders module, at PostConfigInit.)

	// Linux: with a touchscreen, keep relative mouse mode and text input off; end the touches SDL cancels;
	// trace touch delivery on request.
	RammsTouchLinux::Startup();
}

void FRammsUIModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	RammsTouchLinux::Shutdown();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRammsUIModule, RammsUI)
