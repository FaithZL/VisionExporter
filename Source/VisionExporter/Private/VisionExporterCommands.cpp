// Copyright Epic Games, Inc. All Rights Reserved.

#include "VisionExporterCommands.h"

#define LOCTEXT_NAMESPACE "FVisionExporterModule"

void FVisionExporterCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "VisionExporter", "Bring up VisionExporter window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
