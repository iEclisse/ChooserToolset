// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Registers the Chooser toolsets with the ToolsetRegistry so that MCP clients can author chooser
 * tables - their parameters, rows, columns and cells - without opening the chooser editor.
 */
class FChooserToolsetModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterToolsets();
	void UnregisterToolsets();
};
