// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "Chooser.h"
#include "ChooserTableToolset.h"
#include "ChooserToolsetTestTypes.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

namespace ChooserToolsetTest
{
	const auto Flags =
		EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::ProductFilter |
		EAutomationTestFlags::CriticalPriority;

	/** Shared mount point for transient test assets. */
	static const FString TestMountPoint = TEXT("/Automation/ChooserToolsetTest/");

	/** Registers a transient mount point for test assets. */
	inline void RegisterTestMountPoint()
	{
		FPackageName::RegisterMountPoint(*TestMountPoint, FPaths::AutomationTransientDir());
	}

	/**
	 * Unregisters the transient mount point and lets the test assets go.
	 *
	 * A created asset keeps RF_Standalone, so without dropping it the packages pile up in memory for
	 * the rest of the run, and every editor pass over loaded packages then warns that their path no
	 * longer resolves once the mount point is gone.
	 */
	inline void UnregisterTestMountPoint()
	{
		for (TObjectIterator<UPackage> It; It; ++It)
		{
			UPackage* Package = *It;
			if (!Package->GetName().StartsWith(TestMountPoint))
			{
				continue;
			}

			Package->SetDirtyFlag(false);
			ForEachObjectWithPackage(Package,
				[](UObject* Object)
				{
					Object->ClearFlags(RF_Public | RF_Standalone);
					return true;
				});
		}

		FPackageName::UnRegisterMountPoint(*TestMountPoint, FPaths::AutomationTransientDir());
		if (GEngine)
		{
			GEngine->ForceGarbageCollection(true);
		}
	}

	/** A table returning plain objects, with the test object and struct parameters already added. */
	inline UChooserTable* MakeTable(const FString& AssetName)
	{
		UChooserTable* Table = UChooserTableToolset::CreateChooserTable(
			TestMountPoint, AssetName, UObject::StaticClass(), EObjectChooserResultType::ObjectResult);
		if (Table)
		{
			UChooserTableToolset::AddChooserObjectParameter(
				Table, UChooserToolsetTestContext::StaticClass(), EContextObjectDirection::Read);
			UChooserTableToolset::AddChooserStructParameter(
				Table, FChooserToolsetTestParameters::StaticStruct(), EContextObjectDirection::ReadWrite);
		}
		return Table;
	}

	/** An object the tests can hand to a table as a result or as a parameter value. */
	inline UChooserToolsetTestContext* MakeContext()
	{
		return NewObject<UChooserToolsetTestContext>(GetTransientPackage());
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
