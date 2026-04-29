// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "LevelEditor.h"
#include "SCarrierLabControlPanel.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "CarrierLabEditor"

namespace
{
	const FName CarrierLabControlPanelTabName(TEXT("CarrierLabControlPanel"));
}

class FCarrierLabEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			CarrierLabControlPanelTabName,
			FOnSpawnTab::CreateRaw(this, &FCarrierLabEditorModule::SpawnControlPanelTab))
			.SetDisplayName(LOCTEXT("CarrierLabControlPanelTab", "CarrierLab Control Panel"))
			.SetTooltipText(LOCTEXT("CarrierLabControlPanelTooltip", "Open the CarrierLab clean-room carrier control panel."))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FCarrierLabEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		if (UToolMenus::IsToolMenuUIEnabled())
		{
			UToolMenus::UnRegisterStartupCallback(this);
			UToolMenus::UnregisterOwner(this);
		}
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(CarrierLabControlPanelTabName);
	}

private:
	TSharedRef<SDockTab> SpawnControlPanelTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SCarrierLabControlPanel)
			];
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
		Section.AddMenuEntry(
			"CarrierLabControlPanel",
			LOCTEXT("CarrierLabControlPanelMenu", "CarrierLab Control Panel"),
			LOCTEXT("CarrierLabControlPanelMenuTooltip", "Open the CarrierLab control panel tab."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FCarrierLabEditorModule::OpenControlPanelTab)));
	}

	void OpenControlPanelTab()
	{
		FGlobalTabmanager::Get()->TryInvokeTab(CarrierLabControlPanelTabName);
	}
};

IMPLEMENT_MODULE(FCarrierLabEditorModule, CarrierLabEditor)

#undef LOCTEXT_NAMESPACE
