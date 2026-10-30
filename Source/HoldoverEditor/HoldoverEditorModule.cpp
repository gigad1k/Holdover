// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "HoldoverInputToolset.h"

class FHoldoverEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Project modules load before engine plugins, so the AIToolsetRegistrySubsystem
		// does not exist yet at StartupModule time — defer until the editor is up.
		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FHoldoverEditorModule::RegisterToolset);
	}

	virtual void ShutdownModule() override
	{
		if (UToolsetRegistry::IsAvailable() && UToolsetRegistry::IsToolsetClassRegistered(UHoldoverInputToolset::StaticClass()))
		{
			UToolsetRegistry::UnregisterToolsetClass(UHoldoverInputToolset::StaticClass());
		}
	}

private:
	void RegisterToolset()
	{
		UToolsetRegistry::RegisterToolsetClass(UHoldoverInputToolset::StaticClass());
	}
};

IMPLEMENT_MODULE(FHoldoverEditorModule, HoldoverEditor)
