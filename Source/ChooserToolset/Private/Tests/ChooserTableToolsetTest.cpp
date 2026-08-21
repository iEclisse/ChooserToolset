// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "BoolColumn.h"
#include "Chooser.h"
#include "ChooserColumnToolset.h"
#include "ChooserPropertyAccess.h"
#include "ChooserRowToolset.h"
#include "ChooserTableToolset.h"
#include "ChooserToolsetTestFlags.h"
#include "ChooserToolsetTestTypes.h"
#include "ChooserTypes.h"
#include "Animation/AnimationAsset.h"

BEGIN_DEFINE_SPEC(FChooserToolsetTest_Table,
	"AI.Toolsets.ChooserToolset.Table", ChooserToolsetTest::Flags)
	int32 TestCounter = 0;

	/** Makes a unique asset name so tests never collide inside the transient mount point. */
	FString NextName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%d"), *Prefix, ++TestCounter);
	}
END_DEFINE_SPEC(FChooserToolsetTest_Table)

void FChooserToolsetTest_Table::Define()
{
	using namespace ChooserToolsetTest;

	BeforeEach([this]()
		{
			RegisterTestMountPoint();
		});

	AfterEach([this]()
		{
			UnregisterTestMountPoint();
		});

	Describe("CreateChooserTable", [this]()
		{
			It("creates a table with the requested signature", [this]()
				{
					const FString AssetName = NextName(TEXT("CT_Create"));
					UChooserTable* Table = UChooserTableToolset::CreateChooserTable(
						TestMountPoint, AssetName, UObject::StaticClass(), EObjectChooserResultType::ObjectResult);

					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestEqual("result class assigned", Table->OutputObjectType.Get(), UObject::StaticClass());
					TestTrue("result type assigned", Table->ResultType == EObjectChooserResultType::ObjectResult);
					TestEqual("version stamped", static_cast<int32>(Table->Version), UChooserTable::CurrentVersion);
				});

			It("returns null when no result class is given", [this]()
				{
					UChooserTable* Table = UChooserTableToolset::CreateChooserTable(
						TestMountPoint, NextName(TEXT("CT_NoClass")), nullptr, EObjectChooserResultType::ObjectResult);
					TestNull("no table without a result class", Table);
				});

			It("allows no result class when the table returns nothing", [this]()
				{
					UChooserTable* Table = UChooserTableToolset::CreateChooserTable(
						TestMountPoint, NextName(TEXT("CT_NoResult")), nullptr, EObjectChooserResultType::NoPrimaryResult);
					TestNotNull("output only table created", Table);
				});

			It("returns null when an asset already exists at the path", [this]()
				{
					const FString AssetName = NextName(TEXT("CT_Taken"));
					UChooserTableToolset::CreateChooserTable(
						TestMountPoint, AssetName, UObject::StaticClass(), EObjectChooserResultType::ObjectResult);

					UChooserTable* Second = UChooserTableToolset::CreateChooserTable(
						TestMountPoint, AssetName, UObject::StaticClass(), EObjectChooserResultType::ObjectResult);
					TestNull("second table refused", Second);
				});

			It("returns null when the asset name is empty", [this]()
				{
					UChooserTable* Table = UChooserTableToolset::CreateChooserTable(
						TestMountPoint, TEXT(""), UObject::StaticClass(), EObjectChooserResultType::ObjectResult);
					TestNull("no table without a name", Table);
				});
		});

	Describe("CreateAnimationChooserTable", [this]()
		{
			It("sets up the anim instance and settings parameters", [this]()
				{
					UChooserTable* Table = UChooserTableToolset::CreateAnimationChooserTable(
						TestMountPoint, NextName(TEXT("CT_Anim")), nullptr);
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					TestEqual("two parameters", Table->ContextData.Num(), 2);
					TestEqual("returns animation assets", Table->OutputObjectType.Get(), UAnimationAsset::StaticClass());

					const FContextObjectTypeStruct* Settings = Table->ContextData[1].GetPtr<FContextObjectTypeStruct>();
					if (TestNotNull("second parameter is a struct", Settings))
					{
						TestEqual("player settings struct", Settings->Struct.Get(), FChooserPlayerSettings::StaticStruct());
						TestTrue("settings are written to", Settings->Direction == EContextObjectDirection::Write);
					}
				});

			It("returns null when the asset name is empty", [this]()
				{
					UChooserTable* Table = UChooserTableToolset::CreateAnimationChooserTable(
						TestMountPoint, TEXT(""), nullptr);
					TestNull("no table without a name", Table);
				});
		});

	Describe("ListChooserTables", [this]()
		{
			It("finds a table in the folder it was created in", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_List")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					const TArray<UChooserTable*> Tables = UChooserTableToolset::ListChooserTables(TestMountPoint);
					TestTrue("created table listed", Tables.Contains(Table));
				});

			It("returns nothing for a folder with no tables", [this]()
				{
					const TArray<UChooserTable*> Tables =
						UChooserTableToolset::ListChooserTables(TEXT("/Automation/ChooserToolsetEmpty"));
					TestTrue("no tables found", Tables.IsEmpty());
				});
		});

	Describe("DescribeChooserTable", [this]()
		{
			It("reports the signature, parameters, columns and rows", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_Describe")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(
						Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);

					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					TestEqual("describes the table", Info.Table.Get(), Table);
					TestEqual("two parameters", Info.Parameters.Num(), 2);
					TestEqual("one column", Info.Columns.Num(), 1);
					TestEqual("one row", Info.Rows.Num(), 1);
					if (Info.Rows.Num() == 1)
					{
						TestEqual("row reports one cell per column", Info.Rows[0].Cells.Num(), 1);
						TestEqual("row result type named", Info.Rows[0].ResultType, FString(TEXT("Asset")));
					}
					if (Info.Columns.Num() == 1)
					{
						TestEqual("column bound to the struct parameter", Info.Columns[0].ParameterIndex, 1);
						TestEqual("column reports its property", Info.Columns[0].PropertyPath, FString(TEXT("bIsCrouching")));
					}
				});

			It("returns an empty description for no table", [this]()
				{
					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(nullptr);
					TestNull("no table described", Info.Table.Get());
				});
		});

	Describe("SetChooserResultType", [this]()
		{
			It("changes what the table returns", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_ResultType")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					TestTrue("result type changed", UChooserTableToolset::SetChooserResultType(
						Table, UChooserToolsetTestContext::StaticClass(), EObjectChooserResultType::ClassResult));
					TestEqual("result class applied", Table->OutputObjectType.Get(), UChooserToolsetTestContext::StaticClass());
					TestTrue("result type applied", Table->ResultType == EObjectChooserResultType::ClassResult);
				});

			It("fails without a result class", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_ResultTypeBad")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused without a class", UChooserTableToolset::SetChooserResultType(
						Table, nullptr, EObjectChooserResultType::ObjectResult));
				});

			It("fails without a table", [this]()
				{
					TestFalse("refused without a table", UChooserTableToolset::SetChooserResultType(
						nullptr, UObject::StaticClass(), EObjectChooserResultType::ObjectResult));
				});
		});

	Describe("parameters", [this]()
		{
			It("adds object and struct parameters in order", [this]()
				{
					UChooserTable* Table = UChooserTableToolset::CreateChooserTable(
						TestMountPoint, NextName(TEXT("CT_Params")), UObject::StaticClass(),
						EObjectChooserResultType::ObjectResult);
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					const int32 ObjectIndex = UChooserTableToolset::AddChooserObjectParameter(
						Table, UChooserToolsetTestContext::StaticClass(), EContextObjectDirection::Read);
					const int32 StructIndex = UChooserTableToolset::AddChooserStructParameter(
						Table, FChooserToolsetTestParameters::StaticStruct(), EContextObjectDirection::Write);

					TestEqual("object parameter is first", ObjectIndex, 0);
					TestEqual("struct parameter is second", StructIndex, 1);
					TestEqual("both stored", Table->ContextData.Num(), 2);
				});

			It("refuses an object parameter with no class", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_ParamNoClass")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestEqual("no parameter added", UChooserTableToolset::AddChooserObjectParameter(
						Table, nullptr, EContextObjectDirection::Read), INDEX_NONE);
				});

			It("refuses a struct parameter with no struct", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_ParamNoStruct")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestEqual("no parameter added", UChooserTableToolset::AddChooserStructParameter(
						Table, nullptr, EContextObjectDirection::Read), INDEX_NONE);
				});

			It("removes a parameter", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_ParamRemove")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					TestTrue("parameter removed", UChooserTableToolset::RemoveChooserParameter(Table, 0));
					TestEqual("one parameter left", Table->ContextData.Num(), 1);
				});

			It("refuses to remove a parameter that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_ParamRemoveBad")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserTableToolset::RemoveChooserParameter(Table, 7));
				});

			It("changes a parameter's direction", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_ParamDirection")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					TestTrue("direction changed", UChooserTableToolset::SetChooserParameterDirection(
						Table, 0, EContextObjectDirection::ReadWrite));
					TestTrue("direction applied",
						Table->ContextData[0].Get<FContextObjectTypeBase>().Direction == EContextObjectDirection::ReadWrite);
				});

			It("refuses to change the direction of a parameter that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_ParamDirectionBad")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserTableToolset::SetChooserParameterDirection(
						Table, 7, EContextObjectDirection::Write));
				});
		});

	Describe("ListBindableProperties", [this]()
		{
			It("finds direct members and nested paths", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_Bindable")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					const TArray<FChooserToolsetBindingInfo> Bindings =
						UChooserTableToolset::ListBindableProperties(Table, 0, TEXT(""), 2);

					bool bFoundHealth = false;
					bool bFoundNested = false;
					for (const FChooserToolsetBindingInfo& Binding : Bindings)
					{
						bFoundHealth |= Binding.PropertyPath == TEXT("Health")
							&& Binding.ValueKind == EChooserToolsetValueKind::Float;
						bFoundNested |= Binding.PropertyPath == TEXT("Movement.Speed");
					}
					TestTrue("direct property found", bFoundHealth);
					TestTrue("nested property found", bFoundNested);
				});

			It("filters by name", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_BindableFilter")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					const TArray<FChooserToolsetBindingInfo> Bindings =
						UChooserTableToolset::ListBindableProperties(Table, 0, TEXT("Health"), 2);
					TestFalse("something matched", Bindings.IsEmpty());
					for (const FChooserToolsetBindingInfo& Binding : Bindings)
					{
						TestTrue("every result matches the filter", Binding.PropertyPath.Contains(TEXT("Health")));
					}
				});

			It("returns nothing for a parameter that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_BindableBad")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestTrue("no bindings", UChooserTableToolset::ListBindableProperties(Table, 7, TEXT(""), 2).IsEmpty());
				});

			It("returns nothing when the depth is below one", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_BindableDepth")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestTrue("no bindings", UChooserTableToolset::ListBindableProperties(Table, 0, TEXT(""), 0).IsEmpty());
				});
		});

	Describe("nested tables", [this]()
		{
			It("creates a nested table that shares its parent's parameters", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_Nested")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserTable* Nested = UChooserTableToolset::CreateNestedChooser(Table, TEXT("Crouched"));
					if (!TestNotNull("nested table created", Nested))
					{
						return;
					}

					TestEqual("nested table knows its root", Nested->GetRootChooser(), Table);
					TestEqual("nested table shares parameters", Nested->GetContextData().Num(), 2);
					TestTrue("root lists the nested table", Table->NestedObjects.Contains(Nested));
				});

			It("refuses to create a nested table with no name", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_NestedNoName")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestNull("no nested table", UChooserTableToolset::CreateNestedChooser(Table, TEXT("")));
				});

			It("deletes a nested table", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_NestedDelete")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserTable* Nested = UChooserTableToolset::CreateNestedChooser(Table, TEXT("Sprinting"));
					if (!TestNotNull("nested table created", Nested))
					{
						return;
					}

					TestTrue("nested table deleted", UChooserTableToolset::DeleteNestedChooser(Table, Nested));
					TestFalse("root no longer lists it", Table->NestedObjects.Contains(Nested));
				});

			It("refuses to delete a table that is not nested here", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_NestedForeign")));
					UChooserTable* Other = MakeTable(NextName(TEXT("CT_NestedForeignOther")));
					if (!TestNotNull("table created", Table) || !TestNotNull("other table created", Other))
					{
						return;
					}
					TestFalse("refused", UChooserTableToolset::DeleteNestedChooser(Table, Other));
				});

			It("refuses to delete nothing", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_NestedDeleteNull")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserTableToolset::DeleteNestedChooser(Table, nullptr));
				});
		});

	Describe("ValidateChooserTable", [this]()
		{
			It("passes a table whose columns and rows are complete", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_Valid")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);

					const FChooserToolsetValidationResult Result = UChooserTableToolset::ValidateChooserTable(Table);
					TestTrue("table is valid", Result.bSuccess);
					TestTrue("no errors", Result.Errors.IsEmpty());
				});

			It("reports a column that was never bound", [this]()
				{
					AddExpectedError(TEXT("Missing property binding"), EAutomationExpectedErrorFlags::Contains, 0);

					UChooserTable* Table = MakeTable(NextName(TEXT("CT_Unbound")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), -1, TEXT(""));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);

					const FChooserToolsetValidationResult Result = UChooserTableToolset::ValidateChooserTable(Table);
					TestFalse("table is not valid", Result.bSuccess);
					TestFalse("an error was reported", Result.Errors.IsEmpty());
				});

			It("warns about a table with no rows", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_NoRows")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					const FChooserToolsetValidationResult Result = UChooserTableToolset::ValidateChooserTable(Table);
					TestTrue("still valid", Result.bSuccess);
					TestFalse("a warning was reported", Result.Warnings.IsEmpty());
				});

			It("reports the missing table when given none", [this]()
				{
					const FChooserToolsetValidationResult Result = UChooserTableToolset::ValidateChooserTable(nullptr);
					TestFalse("not valid", Result.bSuccess);
				});
		});

	Describe("ReplaceChooserBindingNames", [this]()
		{
			It("renames a property inside a binding", [this]()
				{
					// The renamed property does not exist on the parameter, which the chooser reports as it compiles.
					AddExpectedError(TEXT("not Found on Class/Struct"), EAutomationExpectedErrorFlags::Contains, 0);

					UChooserTable* Table = MakeTable(NextName(TEXT("CT_Rename")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));

					const int32 Changed = UChooserTableToolset::ReplaceChooserBindingNames(
						Table, TEXT("bIsCrouching"), TEXT("bIsSliding"), true, true);
					TestEqual("one binding changed", Changed, 1);

					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					if (TestEqual("still one column", Info.Columns.Num(), 1))
					{
						TestEqual("binding renamed", Info.Columns[0].PropertyPath, FString(TEXT("bIsSliding")));
					}
				});

			It("changes nothing when the name does not appear", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_RenameMiss")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					TestEqual("nothing changed", UChooserTableToolset::ReplaceChooserBindingNames(
						Table, TEXT("NotAProperty"), TEXT("Whatever"), true, true), 0);
				});

			It("refuses an empty search name", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_RenameEmpty")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestEqual("nothing changed", UChooserTableToolset::ReplaceChooserBindingNames(
						Table, TEXT(""), TEXT("Whatever"), true, true), 0);
				});
		});

	Describe("RemoveDisabledChooserData", [this]()
		{
			It("deletes disabled rows and columns", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_Strip")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);
					UChooserRowToolset::SetChooserRowDisabled(Table, 0, true);
					UChooserColumnToolset::SetChooserColumnDisabled(Table, 0, true);

					TestTrue("data stripped", UChooserTableToolset::RemoveDisabledChooserData(Table));
					TestEqual("one row left", Table->ResultsStructs.Num(), 1);
					TestEqual("no columns left", Table->ColumnsStructs.Num(), 0);
				});

			It("fails without a table", [this]()
				{
					TestFalse("refused", UChooserTableToolset::RemoveDisabledChooserData(nullptr));
				});
		});

	Describe("chooser editor", [this]()
		{
			It("opens and closes the editor for a table", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_Editor")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					TestTrue("editor opened", UChooserTableToolset::OpenChooserEditor(Table));
					TestTrue("editor closed", UChooserTableToolset::CloseChooserEditor(Table));
				});

			It("fails to open without a table", [this]()
				{
					TestFalse("refused", UChooserTableToolset::OpenChooserEditor(nullptr));
				});

			It("fails to close without a table", [this]()
				{
					TestFalse("refused", UChooserTableToolset::CloseChooserEditor(nullptr));
				});
		});
}

#endif // WITH_DEV_AUTOMATION_TESTS
