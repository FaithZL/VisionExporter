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
#include "LandscapeDataAccess.h"

static const FName VisionExporterTabName("VisionExporter");

#define LOCTEXT_NAMESPACE "FVisionExporterModule"

extern ENGINE_API class UWorldProxy GWorld;

#pragma optimize( "", off )

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
	OBJGeom(const FString& InName)
		: Name(InName)
	{}
};

void OutputObjMesh(OBJGeom *object, FString TargetPath) {
	FString Filename = object->Name + TEXT(".obj");
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
		Ar.Logf(TEXT("f "));
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


UWorld *FVisionExporterModule::GetWorld() const noexcept {
	return GWorld.GetReference();
}

UAssetExportTask *FVisionExporterModule::InitExportTask(FString Filename, bool bSelected) const noexcept {
	UAssetExportTask* ExportTask = NewObject<UAssetExportTask>();
	ExportTask->Object = GetWorld();
	ExportTask->Exporter = NULL;
	ExportTask->Filename = Filename;
	ExportTask->bSelected = bSelected;
	ExportTask->bReplaceIdentical = true;
	ExportTask->bPrompt = false;
	ExportTask->bUseFileArchive = false;
	ExportTask->bWriteEmptyFiles = false;
	return ExportTask;
}

TArray<TSharedPtr<OBJGeom>> FVisionExporterModule::ActorToObjs(AActor* Actor, bool bSelectedOnly) const noexcept {
	TArray<TSharedPtr<OBJGeom>> Objects;
	
	FMatrix LocalToWorld = Actor->ActorToWorld().ToMatrixWithScale();
	ALandscape* Landscape = Cast<ALandscape>(Actor);
	ULandscapeInfo* LandscapeInfo = Landscape ? Landscape->GetLandscapeInfo() : NULL;
	if (Landscape && LandscapeInfo) {
		auto SelectedComponents = LandscapeInfo->GetSelectedComponents();
		// Export data for each component
		for (auto It = Landscape->GetLandscapeInfo()->XYtoComponentMap.CreateIterator(); It; ++It)
		{
			if (bSelectedOnly && SelectedComponents.Num() && !SelectedComponents.Contains(It.Value()))
			{
				continue;
			}
			ULandscapeComponent* Component = It.Value();
			if (!Component->IsVisibleInEditor()) {
				continue;
			}
			FLandscapeComponentDataInterface CDI(Component, Landscape->ExportLOD);
			const int32 ComponentSizeQuads = ((Component->ComponentSizeQuads + 1) >> Landscape->ExportLOD) - 1;
			const int32 SubsectionSizeQuads = ((Component->SubsectionSizeQuads + 1) >> Landscape->ExportLOD) - 1;
			const float ScaleFactor = (float)Component->ComponentSizeQuads / (float)ComponentSizeQuads;

			TSharedPtr<OBJGeom> objGeom = MakeShareable(new OBJGeom(Component->GetName()));
			objGeom->VertexData.AddZeroed(FMath::Square(ComponentSizeQuads + 1));
			objGeom->Faces.AddZeroed(FMath::Square(ComponentSizeQuads) * 2);

			// Check if there are any holes
			TArray64<uint8> RawVisData;
			uint8* VisDataMap = NULL;
			int32 TexIndex = INDEX_NONE;
			int32 WeightMapSize = (SubsectionSizeQuads + 1) * Component->NumSubsections;
			int32 ChannelOffsets[4] = { (int32)STRUCT_OFFSET(FColor,R),(int32)STRUCT_OFFSET(FColor,G),(int32)STRUCT_OFFSET(FColor,B),(int32)STRUCT_OFFSET(FColor,A) };

			const TArray<FWeightmapLayerAllocationInfo>& ComponentWeightmapLayerAllocations = Component->GetWeightmapLayerAllocations();
			const TArray<UTexture2D*>& ComponentWeightmapTextures = Component->GetWeightmapTextures();

			for (int32 AllocIdx = 0; AllocIdx < ComponentWeightmapLayerAllocations.Num(); AllocIdx++)
			{
				const FWeightmapLayerAllocationInfo& AllocInfo = ComponentWeightmapLayerAllocations[AllocIdx];
				if (AllocInfo.LayerInfo == ALandscapeProxy::VisibilityLayer)
				{
					TexIndex = AllocInfo.WeightmapTextureIndex;

					ComponentWeightmapTextures[TexIndex]->Source.GetMipData(RawVisData, 0);
					VisDataMap = RawVisData.GetData() + ChannelOffsets[AllocInfo.WeightmapTextureChannel];
				}
			}

			// Export verts
			OBJVertex* Vert = objGeom->VertexData.GetData();
			for (int32 y = 0; y < ComponentSizeQuads + 1; y++)
			{
				for (int32 x = 0; x < ComponentSizeQuads + 1; x++)
				{
					FVector WorldPos, WorldTangentX, WorldTangentY, WorldTangentZ;

					CDI.GetWorldPositionTangents(x, y, WorldPos, WorldTangentX, WorldTangentY, WorldTangentZ);

					Vert->Vert = WorldPos;
					Vert->UV = FVector2D(Component->GetSectionBase().X + x * ScaleFactor, Component->GetSectionBase().Y + y * ScaleFactor);
					Vert->Normal = WorldTangentZ;
					Vert++;
				}
			}

			int32 VisThreshold = 170;
			int32 SubNumX, SubNumY, SubX, SubY;

			OBJFace* Face = objGeom->Faces.GetData();
			for (int32 y = 0; y < ComponentSizeQuads; y++)
			{
				for (int32 x = 0; x < ComponentSizeQuads; x++)
				{
					CDI.ComponentXYToSubsectionXY(x, y, SubNumX, SubNumY, SubX, SubY);
					int32 WeightIndex = SubX + SubNumX * (SubsectionSizeQuads + 1) + (SubY + SubNumY * (SubsectionSizeQuads + 1)) * WeightMapSize;

					bool bInvisible = VisDataMap && VisDataMap[WeightIndex * sizeof(FColor)] >= VisThreshold;
					// triangulation matches FLandscapeIndexBuffer constructor
					Face->VertexIndex[0] = (x + 0) + (y + 0) * (ComponentSizeQuads + 1);
					Face->VertexIndex[1] = bInvisible ? Face->VertexIndex[0] : (x + 1) + (y + 1) * (ComponentSizeQuads + 1);
					Face->VertexIndex[2] = bInvisible ? Face->VertexIndex[0] : (x + 1) + (y + 0) * (ComponentSizeQuads + 1);
					Face++;

					Face->VertexIndex[0] = (x + 0) + (y + 0) * (ComponentSizeQuads + 1);
					Face->VertexIndex[1] = bInvisible ? Face->VertexIndex[0] : (x + 0) + (y + 1) * (ComponentSizeQuads + 1);
					Face->VertexIndex[2] = bInvisible ? Face->VertexIndex[0] : (x + 1) + (y + 1) * (ComponentSizeQuads + 1);
					Face++;
				}
			}

			Objects.Add(objGeom);
		}
	}

	// Static mesh components

	UStaticMeshComponent* StaticMeshComponent = NULL;
	UStaticMesh* StaticMesh = NULL;

	TInlineComponentArray<UStaticMeshComponent*> StaticMeshComponents;
	Actor->GetComponents(StaticMeshComponents);

	for (int32 j = 0; j < StaticMeshComponents.Num(); j++)
	{
		// If its a static mesh component, with a static mesh
		StaticMeshComponent = StaticMeshComponents[j];
		if (!StaticMeshComponent->IsVisibleInEditor()) {
			continue;
		}
		if (StaticMeshComponent->IsRegistered() && StaticMeshComponent->GetStaticMesh()
			&& StaticMeshComponent->GetStaticMesh()->HasValidRenderData())
		{
			LocalToWorld = StaticMeshComponent->GetComponentTransform().ToMatrixWithScale();
			StaticMesh = StaticMeshComponent->GetStaticMesh();
			if (StaticMesh)
			{
				// make room for the faces
				TSharedPtr<OBJGeom> objGeom = MakeShareable(new OBJGeom(StaticMeshComponents.Num() > 1 ? StaticMesh->GetName() : Actor->GetName()));

				FStaticMeshLODResources* RenderData = &StaticMesh->GetRenderData()->LODResources[0];
				FIndexArrayView Indices = RenderData->IndexBuffer.GetArrayView();
				uint32 NumIndices = Indices.Num();

				// 3 indices for each triangle
				check(NumIndices % 3 == 0);
				uint32 TriangleCount = NumIndices / 3;
				objGeom->Faces.AddUninitialized(TriangleCount);

				uint32 VertexCount = RenderData->VertexBuffers.PositionVertexBuffer.GetNumVertices();
				objGeom->VertexData.AddUninitialized(VertexCount);
				OBJVertex* VerticesOut = objGeom->VertexData.GetData();

				check(VertexCount == RenderData->VertexBuffers.StaticMeshVertexBuffer.GetNumVertices());

				FMatrix LocalToWorldInverseTranspose = LocalToWorld.InverseFast().GetTransposed();
				for (uint32 i = 0; i < VertexCount; i++)
				{
					// Vertices
					VerticesOut[i].Vert = ((FVector)RenderData->VertexBuffers.PositionVertexBuffer.VertexPosition(i));
					// UVs from channel 0
					VerticesOut[i].UV = FVector2D(RenderData->VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(i, 0));
					// Normal
					VerticesOut[i].Normal = ((FVector4)RenderData->VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(i));
				}

				bool bFlipCullMode = LocalToWorld.RotDeterminant() < 0.0f;

				uint32 CurrentTriangleId = 0;
				for (int32 SectionIndex = 0; SectionIndex < RenderData->Sections.Num(); ++SectionIndex)
				{
					FStaticMeshSection& Section = RenderData->Sections[SectionIndex];
					UMaterialInterface* Material = 0;

					// Get the material for this triangle by first looking at the material overrides array and if that is NULL by looking at the material array in the original static mesh
					Material = StaticMeshComponent->GetMaterial(Section.MaterialIndex);

					// cache the set of needed materials if desired
				//	if (Materials && Material)
				//	{
				//		Materials->Add(Material);
				//	}

					for (uint32 i = 0; i < Section.NumTriangles; i++)
					{
						OBJFace& objFace = objGeom->Faces[CurrentTriangleId++];

						uint32 a = Indices[Section.FirstIndex + i * 3 + 0];
						uint32 b = Indices[Section.FirstIndex + i * 3 + 1];
						uint32 c = Indices[Section.FirstIndex + i * 3 + 2];

						if (bFlipCullMode)
						{
							Swap(a, c);
						}

						objFace.VertexIndex[0] = a;
						objFace.VertexIndex[1] = b;
						objFace.VertexIndex[2] = c;

						// Material
					//	objFace.Material = Material;
					}
				}

				Objects.Add(objGeom);
			}
		}
	}

	return Objects;
}

TArray<TSharedPtr<OBJGeom>> FVisionExporterModule::GetOBJGeoms(bool bSelectedOnly) const noexcept {
	TArray<TSharedPtr<OBJGeom>> ret;

	TArray<AActor*> actors = GetActors(bSelectedOnly);

	for (int i = 0; i < actors.Num(); ++i) {
		auto objects = ActorToObjs(actors[i], bSelectedOnly);
		for (size_t j = 0; j < objects.Num(); j++) {
			ret.Add(objects[j]);
		}
	}

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

	TArray<TSharedPtr<OBJGeom>> objGeoms = GetOBJGeoms(ExportTask->bSelected);

	for (size_t i = 0; i < objGeoms.Num(); i++)
	{
		OutputObjMesh(objGeoms[i].Get(), TargetPath);
	}
}

void FVisionExporterModule::ExportMeshesToGLTF(UAssetExportTask* ExportTask) const noexcept {

}

TSharedRef<SDockTab> FVisionExporterModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{

	UAssetExportTask* ExportTask = InitExportTask("Test.obj", false);
	FGCObjectScopeGuard ExportTaskGuard(ExportTask);

	//ExportMeshes(ExportTask);

	auto checkBox = SNew(SCheckBox).Style(FCoreStyle::Get(), "RadioButton")
		[
			SNew(STextBlock)
			.Text(LOCTEXT("", "export to vision"))
		];

	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab).Label(LOCTEXT("", "vision exporter"))
		[
			SNew(SBox).HAlign(HAlign_Left).VAlign(VAlign_Top)
			[
				checkBox
			]
		];
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

#pragma optimize( "", on )