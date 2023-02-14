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
#include "Misc/OutputDeviceFile.h"
#include "EditorDirectories.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeInfo.h"

static const FName VisionExporterTabName("VisionExporter");

#define LOCTEXT_NAMESPACE "FVisionExporterModule"

extern ENGINE_API class UWorldProxy GWorld;

class OBJFace
{
public:
	// index into OBJGeom::VertexData (local within OBJGeom)
	uint32 VertexIndex[3];
	/** List of vertices that make up this face. */

	/** The material that was applied to this face. */
	UMaterialInterface* Material;
};

class OBJVertex
{
public:
	// position
	FVector Vert;
	// texture coordiante
	FVector2D UV;
	// normal
	FVector Normal;
	//	FLinearColor Colors[3];
};

// A geometric object.  This will show up as a separate object when imported into a modeling program.
class OBJGeom
{
public:
	/** List of faces that make up this object. */
	TArray<OBJFace> Faces;

	/** Vertex positions that make up this object. */
	TArray<OBJVertex> VertexData;

	/** Name used when writing this object to the OBJ file. */
	FString Name;

	// Constructors.
	FORCEINLINE OBJGeom(const FString& InName)
		: Name(InName)
	{}
};

void OutputObjMesh(OBJGeom *object, FString TargetPath) {
	FString Filename = object->Name;
	FString TempFile = TargetPath + TEXT("/UnrealExportFile.tmp");
	TSharedPtr<FOutputDevice> FileAr = MakeShareable(new FOutputDeviceFile(*TempFile));
	FileAr->SetSuppressEventTag(true);
	FileAr->SetAutoEmitLineTerminator(false);
	FStringOutputDevice Ar;

	// Object header

	Ar.Logf(TEXT("g %s\n"), *object->Name);
	Ar.Logf(TEXT("\n"));

	// Verts

	for (int32 f = 0; f < object->VertexData.Num(); ++f)
	{
		const OBJVertex& vertex = object->VertexData[f];
		const FVector& vtx = vertex.Vert;

		Ar.Logf(TEXT("v %.4f %.4f %.4f\n"), vtx.X, vtx.Z, vtx.Y);
	}

	Ar.Logf(TEXT("\n"));

	// Texture coordinates

	for (int32 f = 0; f < object->VertexData.Num(); ++f)
	{
		const OBJVertex& face = object->VertexData[f];
		const FVector2D& uv = face.UV;

		Ar.Logf(TEXT("vt %.4f %.4f\n"), uv.X, 1.0f - uv.Y);
	}

	Ar.Logf(TEXT("\n"));

	// Normals

	for (int32 f = 0; f < object->VertexData.Num(); ++f)
	{
		const OBJVertex& face = object->VertexData[f];
		const FVector& Normal = face.Normal;

		Ar.Logf(TEXT("vn %.3f %.3f %.3f\n"), Normal.X, Normal.Z, Normal.Y);
	}
	Ar.Logf(TEXT("\n"));

	// Faces

	for (int32 f = 0; f < object->Faces.Num(); ++f)
	{
		const OBJFace& face = object->Faces[f];

		for (int32 v = 0; v < 3; ++v)
		{
			// +1 as Wavefront files are 1 index based
			uint32 VertexIndex = face.VertexIndex[v] + 1;
			Ar.Logf(TEXT("%d/%d/%d "), VertexIndex, VertexIndex, VertexIndex);
		}

		Ar.Logf(TEXT("\n"));
	}
	Ar.Logf(TEXT("\n"));

	FileAr->Flush();
	FileAr->Log(*Ar);
	FileAr->TearDown();
	IFileManager::Get().Move(*(TargetPath + TEXT("/") + Filename), *TempFile, 1, 1);
	return;
}

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

TSharedPtr<OBJGeom> FVisionExporterModule::ActorToObj(AActor* Actor) const noexcept {
	auto ret = MakeShareable(new OBJGeom(""));

	FMatrix LocalToWorld = Actor->ActorToWorld().ToMatrixWithScale();
	//ALandscape* Landscape = Cast<ALandscape>(Actor);
	//ULandscapeInfo* LandscapeInfo = Landscape ? Landscape->GetLandscapeInfo() : NULL;

	return ret;
}

TArray<AActor*> FVisionExporterModule::GetActors(bool bSelectedOnly) const noexcept {
	TArray<AActor*> ActorsToExport;
	for (FActorIterator It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		// only export selected actors if the flag is set
		if (!Actor || (bSelectedOnly && ! Actor->IsSelected()))
		{
			continue;
		}

		ActorsToExport.Add(Actor);
	}
	return ActorsToExport;
}

void FVisionExporterModule::ExportMeshes(UAssetExportTask* ExportTask) const noexcept {
	ExportMeshesToObj(ExportTask);
}

void FVisionExporterModule::ExportMeshesToObj(UAssetExportTask* ExportTask) const noexcept {
	FString TargetPath = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::UNR);
	OutputObjMesh(nullptr, TargetPath);
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

	ExportMeshes(ExportTask);


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