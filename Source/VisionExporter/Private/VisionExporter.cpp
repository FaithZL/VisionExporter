// Copyright Epic Games, Inc. All Rights Reserved.

#include "VisionExporter.h"
#include "VisionExporterStyle.h"
#include "VisionExporterCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "AssetExportTask.h"
#include "UObject/GCObjectScopeGuard.h"

static const FName VisionExporterTabName("VisionExporter");

#define LOCTEXT_NAMESPACE "FVisionExporterModule"

extern ENGINE_API class UWorldProxy GWorld;

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

UWorld *FVisionExporterModule::GetWorld() const noexcept {
	return GWorld.GetReference();
}

UAssetExportTask *FVisionExporterModule::InitExportTask(FString Filename, bool bSelected) const noexcept {
	UAssetExportTask* ExportTask = NewObject<UAssetExportTask>();
	ExportTask->Object = GetWorld();
	ExportTask->Exporter = NULL;
	ExportTask->Filename = Filename;
	ExportTask->bSelected = bSelected;
	ExportTask->bSelected = false;
	ExportTask->bReplaceIdentical = true;
	ExportTask->bPrompt = false;
	ExportTask->bUseFileArchive = false;
	ExportTask->bWriteEmptyFiles = false;
	return ExportTask;
}

void FVisionExporterModule::ExportMeshes(UAssetExportTask* ExportTask) const noexcept {
	ExportMeshesToObj(ExportTask);
}

void FVisionExporterModule::ExportMeshesToObj(UAssetExportTask* ExportTask) const noexcept {

}

void FVisionExporterModule::ExportMeshesToGLTF(UAssetExportTask* ExportTask) const noexcept {

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

	UAssetExportTask* ExportTask = InitExportTask("Test.obj", false);
	FGCObjectScopeGuard ExportTaskGuard(ExportTask);



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