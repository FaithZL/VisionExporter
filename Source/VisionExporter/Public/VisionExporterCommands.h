// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "VisionExporterStyle.h"

class FVisionExporterCommands : public TCommands<FVisionExporterCommands>
{
public:

	FVisionExporterCommands()
		: TCommands<FVisionExporterCommands>(TEXT("VisionExporter"), NSLOCTEXT("Contexts", "VisionExporter", "VisionExporter Plugin"), NAME_None, FVisionExporterStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};