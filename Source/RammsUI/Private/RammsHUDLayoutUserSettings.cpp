// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsHUDLayoutUserSettings.h"

#include "Misc/ConfigCacheIni.h"

bool URammsHUDLayoutUserSettings::Find(FName Id, FRammsFloatingPanelState& OutState) const
{
	if (const FRammsFloatingPanelState* Found = Panels.FindByPredicate([Id](const FRammsFloatingPanelState& S) { return S.Id == Id; }))
	{
		OutState = *Found;
		return true;
	}
	return false;
}

void URammsHUDLayoutUserSettings::Store(const FRammsFloatingPanelState& State)
{
	if (State.Id.IsNone())
	{
		return;
	}
	if (FRammsFloatingPanelState* Found = Panels.FindByPredicate([&State](const FRammsFloatingPanelState& S) { return S.Id == State.Id; }))
	{
		*Found = State;
	}
	else
	{
		Panels.Add(State);
	}
	Write();
}

void URammsHUDLayoutUserSettings::ResetAll()
{
	Panels.Reset();
	Write();
}

void URammsHUDLayoutUserSettings::Write()
{
	// To GameUserSettings.ini by its name, as UGameUserSettings::SaveSettings
	// does: a bare SaveConfig() writes it in the editor but not in a game.
	SaveConfig(CPF_Config, *GGameUserSettingsIni);
	GConfig->Flush(/*bRemoveFromCache=*/false, GGameUserSettingsIni);
}
