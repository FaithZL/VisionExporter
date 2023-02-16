// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Exporters/Exporter.h"

class FToolBarBuilder;
class FMenuBuilder;
class UAssetExportTask;
class OBJGeom;

class FVisionExporterModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command (by default it will bring up plugin window) */
	void PluginButtonClicked();
	
private:

	void RegisterMenus();

	[[nodiscard]] UWorld* GetWorld() const noexcept;
	[[nodiscard]] TArray<AActor*> GetActors(bool bSelectedOnly) const noexcept;
	[[nodiscard]] TArray<TSharedPtr<OBJGeom>> GetOBJGeoms(bool bSelectedOnly) const noexcept;
	[[nodiscard]] TArray<TSharedPtr<OBJGeom>> ActorToObjs(AActor* Actor, bool bSelectedOnly) const noexcept;
	void ExportMeshes(UAssetExportTask*) const noexcept;
	void ExportMeshesToObj(UAssetExportTask*) const noexcept;
	void ExportMeshesToGLTF(UAssetExportTask*) const noexcept;

	[[nodiscard]] UAssetExportTask* InitExportTask(FString Filename, bool bSelected) const noexcept;

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<class FUICommandList> PluginCommands;

};
