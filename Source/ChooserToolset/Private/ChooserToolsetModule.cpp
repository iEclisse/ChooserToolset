// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#include "ChooserToolsetModule.h"

#include "ChooserCellToolset.h"
#include "ChooserColumnToolset.h"
#include "ChooserEvaluationToolset.h"
#include "ChooserRowToolset.h"
#include "ChooserTableToolset.h"
#include "Misc/CoreDelegates.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#define LOCTEXT_NAMESPACE "ChooserToolset"

void FChooserToolsetModule::StartupModule()
{
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(this, &FChooserToolsetModule::RegisterToolsets);
	FCoreDelegates::OnPreExit.AddRaw(this, &FChooserToolsetModule::UnregisterToolsets);
}

void FChooserToolsetModule::ShutdownModule()
{
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.RemoveAll(this);
	FCoreDelegates::OnPreExit.RemoveAll(this);
}

void FChooserToolsetModule::RegisterToolsets()
{
	UToolsetRegistry::RegisterToolsetClass(UChooserTableToolset::StaticClass());
	UToolsetRegistry::RegisterToolsetClass(UChooserRowToolset::StaticClass());
	UToolsetRegistry::RegisterToolsetClass(UChooserColumnToolset::StaticClass());
	UToolsetRegistry::RegisterToolsetClass(UChooserCellToolset::StaticClass());
	UToolsetRegistry::RegisterToolsetClass(UChooserEvaluationToolset::StaticClass());
}

void FChooserToolsetModule::UnregisterToolsets()
{
	UToolsetRegistry::UnregisterToolsetClass(UChooserTableToolset::StaticClass());
	UToolsetRegistry::UnregisterToolsetClass(UChooserRowToolset::StaticClass());
	UToolsetRegistry::UnregisterToolsetClass(UChooserColumnToolset::StaticClass());
	UToolsetRegistry::UnregisterToolsetClass(UChooserCellToolset::StaticClass());
	UToolsetRegistry::UnregisterToolsetClass(UChooserEvaluationToolset::StaticClass());
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FChooserToolsetModule, ChooserToolset)
