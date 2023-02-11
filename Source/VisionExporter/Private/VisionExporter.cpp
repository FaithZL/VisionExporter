// Copyright Epic Games, Inc. All Rights Reserved.

#include "VisionExporter.h"
#include "VisionExporterStyle.h"
#include "VisionExporterCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"

static const FName VisionExporterTabName("VisionExporter");

#define LOCTEXT_NAMESPACE "FVisionExporterModule"

void FVisionExporterModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FVisionExporterStyle::Initialize();
	FVisionExporterStyle::ReloadTextures();

	FVisionExporterCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FVisionExporterCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FVisionExporterModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FVisionExporterModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(VisionExporterTabName, FOnSpawnTab::CreateRaw(this, &FVisionExporterModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FVisionExporterTabTitle", "VisionExporter"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FVisionExporterModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FVisionExporterStyle::Shutdown();

	FVisionExporterCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(VisionExporterTabName);
}

TSharedRef<SDockTab> FVisionExporterModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	FText WidgetText = FText::Format(
		LOCTEXT("WindowWidgetText", "Add code to {0} in {1} to override this window's contents"),
		FText::FromString(TEXT("FVisionExporterModule::OnSpawnPluginTab")),
		FText::FromString(TEXT("VisionExporter.cpp"))
		);

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			// Put your tab content here!
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(WidgetText)
			]
		];
}

void FVisionExporterModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(VisionExporterTabName);
}

void FVisionExporterModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FVisionExporterCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FVisionExporterCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FVisionExporterModule, VisionExporter)