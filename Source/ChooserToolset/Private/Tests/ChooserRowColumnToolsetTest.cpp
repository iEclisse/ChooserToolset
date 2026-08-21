// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "BoolColumn.h"
#include "Chooser.h"
#include "ChooserCellToolset.h"
#include "ChooserColumnToolset.h"
#include "ChooserRowToolset.h"
#include "ChooserTableToolset.h"
#include "ChooserToolsetTestFlags.h"
#include "ChooserToolsetTestTypes.h"
#include "FloatDistanceColumn.h"
#include "FloatRangeColumn.h"
#include "EnumColumn.h"
#include "OutputFloatColumn.h"
#include "OutputStructColumn.h"
#include "RandomizeColumn.h"

BEGIN_DEFINE_SPEC(FChooserToolsetTest_RowsAndColumns,
	"AI.Toolsets.ChooserToolset.RowsAndColumns", ChooserToolsetTest::Flags)
	int32 TestCounter = 0;

	/** Makes a unique asset name so tests never collide inside the transient mount point. */
	FString NextName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%d"), *Prefix, ++TestCounter);
	}
END_DEFINE_SPEC(FChooserToolsetTest_RowsAndColumns)

void FChooserToolsetTest_RowsAndColumns::Define()
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

	Describe("AddChooserRow", [this]()
		{
			It("appends a row holding the given object", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_AddRow")));
					UObject* Result = MakeContext();
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					const int32 RowIndex = UChooserRowToolset::AddChooserRow(Table, Result, false, -1);
					TestEqual("first row is index 0", RowIndex, 0);
					TestEqual("row stored", Table->ResultsStructs.Num(), 1);

					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					if (TestEqual("one row described", Info.Rows.Num(), 1))
					{
						TestEqual("row holds the object", Info.Rows[0].Result.Get(), Result);
					}
				});

			It("inserts before an existing row when asked", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_InsertRow")));
					UObject* First = MakeContext();
					UObject* Inserted = MakeContext();
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserRowToolset::AddChooserRow(Table, First, false, -1);
					const int32 RowIndex = UChooserRowToolset::AddChooserRow(Table, Inserted, false, 0);

					TestEqual("inserted at the front", RowIndex, 0);
					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					if (TestEqual("two rows", Info.Rows.Num(), 2))
					{
						TestEqual("inserted row is first", Info.Rows[0].Result.Get(), Inserted);
						TestEqual("original row moved down", Info.Rows[1].Result.Get(), First);
					}
				});

			It("stores an asset as a soft reference when asked", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_SoftRow")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserRowToolset::AddChooserRow(Table, MakeContext(), true, -1);
					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					if (TestEqual("one row", Info.Rows.Num(), 1))
					{
						TestEqual("stored as a soft reference", Info.Rows[0].ResultType,
							FString(TEXT("Asset (Soft Reference)")));
					}
				});

			It("gives every column a cell for the new row", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_RowCells")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);

					const FBoolColumn* Column = Table->ColumnsStructs[0].GetPtr<FBoolColumn>();
					if (TestNotNull("bool column added", Column))
					{
						TestEqual("column has one cell", Column->RowValuesWithAny.Num(), 1);
					}
				});

			It("adds a row with no result to a table that returns nothing", [this]()
				{
					UChooserTable* Table = UChooserTableToolset::CreateChooserTable(
						TestMountPoint, NextName(TEXT("CT_OutputOnlyRow")), nullptr,
						EObjectChooserResultType::NoPrimaryResult);
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					TestEqual("row added", UChooserRowToolset::AddChooserRow(Table, nullptr, false, -1), 0);
					TestEqual("row stored", Table->ResultsStructs.Num(), 1);
				});

			It("refuses a row with no result on a table that returns objects", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_RowNoResult")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestEqual("no row added", UChooserRowToolset::AddChooserRow(Table, nullptr, false, -1), INDEX_NONE);
				});

			It("refuses a result that is not the table's result class", [this]()
				{
					UChooserTable* Table = UChooserTableToolset::CreateChooserTable(
						TestMountPoint, NextName(TEXT("CT_RowWrongClass")),
						UChooserToolsetTestContext::StaticClass(), EObjectChooserResultType::ObjectResult);
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestEqual("no row added", UChooserRowToolset::AddChooserRow(
						Table, GetTransientPackage(), false, -1), INDEX_NONE);
				});

			It("refuses to add a row without a table", [this]()
				{
					TestEqual("no row added", UChooserRowToolset::AddChooserRow(nullptr, nullptr, false, -1), INDEX_NONE);
				});
		});

	Describe("SetChooserRowResult", [this]()
		{
			It("replaces what a row selects", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_SetResult")));
					UObject* Replacement = MakeContext();
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);
					TestTrue("result replaced", UChooserRowToolset::SetChooserRowResult(Table, 0, Replacement, false));

					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					TestEqual("row holds the replacement", Info.Rows[0].Result.Get(), Replacement);
				});

			It("creates the fallback row at index -2", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_Fallback")));
					UObject* Fallback = MakeContext();
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					TestTrue("fallback assigned", UChooserRowToolset::SetChooserRowResult(Table, -2, Fallback, false));
					TestTrue("fallback stored", Table->FallbackResult.IsValid());

					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					TestEqual("fallback described", Info.FallbackRow.Result.Get(), Fallback);
				});

			It("points a row at another chooser table", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_EvaluateRow")));
					UChooserTable* Other = MakeTable(NextName(TEXT("CT_EvaluateTarget")));
					if (!TestNotNull("table created", Table) || !TestNotNull("other table created", Other))
					{
						return;
					}

					UChooserRowToolset::AddChooserRow(Table, Other, false, -1);
					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					TestEqual("stored as an evaluate chooser", Info.Rows[0].ResultType, FString(TEXT("Evaluate Chooser")));
				});

			It("points a row at a nested table", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_NestedRow")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserTable* Nested = UChooserTableToolset::CreateNestedChooser(Table, TEXT("Inner"));
					if (!TestNotNull("nested table created", Nested))
					{
						return;
					}

					UChooserRowToolset::AddChooserRow(Table, Nested, false, -1);
					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					TestEqual("stored as a nested chooser", Info.Rows[0].ResultType, FString(TEXT("Nested Chooser")));
				});

			It("refuses a row that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_SetResultBad")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserRowToolset::SetChooserRowResult(Table, 3, MakeContext(), false));
				});
		});

	Describe("RemoveChooserRow", [this]()
		{
			It("removes a row and its cells", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_RemoveRow")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);

					TestTrue("row removed", UChooserRowToolset::RemoveChooserRow(Table, 0));
					TestEqual("one row left", Table->ResultsStructs.Num(), 1);

					const FBoolColumn* Column = Table->ColumnsStructs[0].GetPtr<FBoolColumn>();
					if (TestNotNull("column still there", Column))
					{
						TestEqual("column has one cell left", Column->RowValuesWithAny.Num(), 1);
					}
				});

			It("clears the fallback row at index -2", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_RemoveFallback")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserRowToolset::SetChooserRowResult(Table, -2, MakeContext(), false);
					TestTrue("fallback removed", UChooserRowToolset::RemoveChooserRow(Table, -2));
					TestFalse("fallback gone", Table->FallbackResult.IsValid());
				});

			It("refuses a row that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_RemoveRowBad")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserRowToolset::RemoveChooserRow(Table, 0));
				});
		});

	Describe("MoveChooserRow", [this]()
		{
			It("reorders rows and carries their cells", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_MoveRow")));
					UObject* First = MakeContext();
					UObject* Second = MakeContext();
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					UChooserRowToolset::AddChooserRow(Table, First, false, -1);
					UChooserRowToolset::AddChooserRow(Table, Second, false, -1);
					UChooserCellToolset::SetChooserCell(Table, 0, 0, TEXT("MatchTrue"));

					TestTrue("row moved", UChooserRowToolset::MoveChooserRow(Table, 0, 2));

					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					TestEqual("moved row is last", Info.Rows[1].Result.Get(), First);
					TestEqual("cell travelled with the row", Info.Rows[1].Cells[0], FString(TEXT("MatchTrue")));
				});

			It("refuses a row that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_MoveRowBad")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserRowToolset::MoveChooserRow(Table, 0, 0));
				});

			It("refuses a target outside the table", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_MoveRowFar")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);
					TestFalse("refused", UChooserRowToolset::MoveChooserRow(Table, 0, 9));
				});
		});

	Describe("DuplicateChooserRow", [this]()
		{
			It("copies a row and its cells below it", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_DupRow")));
					UObject* Result = MakeContext();
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					UChooserRowToolset::AddChooserRow(Table, Result, false, -1);
					UChooserCellToolset::SetChooserCell(Table, 0, 0, TEXT("MatchFalse"));

					const int32 NewRow = UChooserRowToolset::DuplicateChooserRow(Table, 0);
					TestEqual("copy sits below the original", NewRow, 1);

					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					if (TestEqual("two rows", Info.Rows.Num(), 2))
					{
						TestEqual("copy holds the same object", Info.Rows[1].Result.Get(), Result);
						TestEqual("copy holds the same cell", Info.Rows[1].Cells[0], FString(TEXT("MatchFalse")));
					}
				});

			It("refuses a row that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_DupRowBad")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestEqual("refused", UChooserRowToolset::DuplicateChooserRow(Table, 4), INDEX_NONE);
				});
		});

	Describe("SetChooserRowDisabled", [this]()
		{
			It("disables and re-enables a row", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_DisableRow")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);
					TestTrue("row disabled", UChooserRowToolset::SetChooserRowDisabled(Table, 0, true));
					TestTrue("table agrees", Table->IsRowDisabled(0));

					TestTrue("row re-enabled", UChooserRowToolset::SetChooserRowDisabled(Table, 0, false));
					TestFalse("table agrees", Table->IsRowDisabled(0));
				});

			It("refuses to disable the fallback row", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_DisableFallback")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserRowToolset::SetChooserRowDisabled(Table, -2, true));
				});

			It("refuses a row that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_DisableRowBad")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserRowToolset::SetChooserRowDisabled(Table, 2, true));
				});
		});

	Describe("ListChooserColumnTypes", [this]()
		{
			It("describes the built-in column types", [this]()
				{
					const TArray<FChooserToolsetColumnTypeInfo> Types = UChooserColumnToolset::ListChooserColumnTypes(TEXT(""));
					TestFalse("something was listed", Types.IsEmpty());

					const FChooserToolsetColumnTypeInfo* BoolType = Types.FindByPredicate(
						[](const FChooserToolsetColumnTypeInfo& Info)
						{
							return Info.ColumnType == FBoolColumn::StaticStruct();
						});

					if (TestNotNull("bool column listed", BoolType))
					{
						TestEqual("named as the editor names it", BoolType->DisplayName, FString(TEXT("Bool")));
						TestEqual("filed under filters", BoolType->Category, FString(TEXT("Filter")));
						TestTrue("binds to a bool", BoolType->ValueKind == EChooserToolsetValueKind::Bool);
						TestTrue("filters rows", BoolType->bHasFilters);
						TestFalse("writes nothing", BoolType->bHasOutputs);
						TestEqual("cell format lists the values a cell accepts", BoolType->CellFormat,
							FString(TEXT("MatchFalse|MatchTrue|MatchAny")));
					}

					const FChooserToolsetColumnTypeInfo* RangeType = Types.FindByPredicate(
						[](const FChooserToolsetColumnTypeInfo& Info)
						{
							return Info.ColumnType == FFloatRangeColumn::StaticStruct();
						});

					if (TestNotNull("float range column listed", RangeType))
					{
						TestTrue("cell format names the fields of a range cell",
							RangeType->CellFormat.Contains(TEXT("Min=")) && RangeType->CellFormat.Contains(TEXT("Max=")));
					}
				});

			It("names the type an instanced struct cell needs", [this]()
				{
					const TArray<FChooserToolsetColumnTypeInfo> Types =
						UChooserColumnToolset::ListChooserColumnTypes(TEXT("Output Struct"));

					const FChooserToolsetColumnTypeInfo* StructType = Types.FindByPredicate(
						[](const FChooserToolsetColumnTypeInfo& Info)
						{
							return Info.ColumnType == FOutputStructColumn::StaticStruct();
						});

					if (TestNotNull("output struct column listed", StructType))
					{
						TestTrue("cell format shows the type path a cell carries",
							StructType->CellFormat.Contains(TEXT("/Script/")));
					}
				});

			It("filters by name", [this]()
				{
					const TArray<FChooserToolsetColumnTypeInfo> Types =
						UChooserColumnToolset::ListChooserColumnTypes(TEXT("Output"));
					TestFalse("something matched", Types.IsEmpty());
					for (const FChooserToolsetColumnTypeInfo& Info : Types)
					{
						TestTrue("every result matches the filter",
							Info.DisplayName.Contains(TEXT("Output")) || Info.ColumnType->GetName().Contains(TEXT("Output")));
					}
				});

			It("returns nothing when nothing matches", [this]()
				{
					TestTrue("no types", UChooserColumnToolset::ListChooserColumnTypes(TEXT("NotAColumnType")).IsEmpty());
				});
		});

	Describe("AddChooserColumn", [this]()
		{
			It("adds a column bound to a property in one call", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_AddColumn")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					const int32 ColumnIndex = UChooserColumnToolset::AddChooserColumn(
						Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					TestEqual("first column is index 0", ColumnIndex, 0);

					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					if (TestEqual("one column", Info.Columns.Num(), 1))
					{
						TestEqual("bound to the struct parameter", Info.Columns[0].ParameterIndex, 1);
						TestEqual("bound to the property", Info.Columns[0].PropertyPath, FString(TEXT("bIsCrouching")));
						TestTrue("binding compiles", Info.Columns[0].CompileError.IsEmpty());
					}
				});

			It("adds a column unbound when no parameter is given", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_AddUnbound")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					TestEqual("column added", UChooserColumnToolset::AddChooserColumn(
						Table, FBoolColumn::StaticStruct(), -1, TEXT("")), 0);
				});

			It("keeps output columns after filter columns", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_ColumnOrder")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FOutputFloatColumn::StaticStruct(), 1, TEXT("ChosenSpeed"));
					const int32 FilterIndex = UChooserColumnToolset::AddChooserColumn(
						Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));

					TestEqual("filter column goes first", FilterIndex, 0);
					TestTrue("output column is second",
						Table->ColumnsStructs[1].GetPtr<FOutputFloatColumn>() != nullptr);
				});

			It("keeps the randomize column last", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_RandomOrder")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FRandomizeColumn::StaticStruct(), -1, TEXT(""));
					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));

					TestEqual("two columns", Table->ColumnsStructs.Num(), 2);
					TestTrue("randomize stayed last",
						Table->ColumnsStructs[1].Get<FChooserColumnBase>().IsRandomizeColumn());
				});

			It("refuses a second randomize column", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_TwoRandom")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FRandomizeColumn::StaticStruct(), -1, TEXT(""));
					TestEqual("second refused", UChooserColumnToolset::AddChooserColumn(
						Table, FRandomizeColumn::StaticStruct(), -1, TEXT("")), INDEX_NONE);
				});

			It("refuses a struct that is not a column type", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_BadColumnType")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestEqual("refused", UChooserColumnToolset::AddChooserColumn(
						Table, FChooserToolsetTestParameters::StaticStruct(), -1, TEXT("")), INDEX_NONE);
				});

			It("refuses no column type at all", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_NoColumnType")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestEqual("refused", UChooserColumnToolset::AddChooserColumn(Table, nullptr, -1, TEXT("")), INDEX_NONE);
				});

			It("adds no column when the binding cannot resolve", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_BadBinding")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					TestEqual("refused", UChooserColumnToolset::AddChooserColumn(
						Table, FBoolColumn::StaticStruct(), 1, TEXT("NotAProperty")), INDEX_NONE);
					TestEqual("no column left behind", Table->ColumnsStructs.Num(), 0);
				});
		});

	Describe("SetChooserColumnBinding", [this]()
		{
			It("points a column at a different property", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_Rebind")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), -1, TEXT(""));
					TestTrue("bound", UChooserColumnToolset::SetChooserColumnBinding(Table, 0, 1, TEXT("bIsCrouching")));

					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					TestEqual("binding applied", Info.Columns[0].PropertyPath, FString(TEXT("bIsCrouching")));
				});

			It("binds a nested path through an object parameter", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_NestedBind")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), -1, TEXT(""));
					TestTrue("bound", UChooserColumnToolset::SetChooserColumnBinding(
						Table, 0, 0, TEXT("Movement.bIsCrouching")));

					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					TestEqual("nested binding applied", Info.Columns[0].PropertyPath,
						FString(TEXT("Movement.bIsCrouching")));
					TestTrue("binding compiles", Info.Columns[0].CompileError.IsEmpty());
				});

			It("refuses a column that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_RebindBadColumn")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserColumnToolset::SetChooserColumnBinding(Table, 0, 1, TEXT("bIsCrouching")));
				});

			It("refuses a parameter that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_RebindBadParam")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), -1, TEXT(""));
					TestFalse("refused", UChooserColumnToolset::SetChooserColumnBinding(Table, 0, 7, TEXT("bIsCrouching")));
				});

			It("refuses a property that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_RebindBadPath")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), -1, TEXT(""));
					TestFalse("refused", UChooserColumnToolset::SetChooserColumnBinding(Table, 0, 1, TEXT("NotAProperty")));
				});
		});

	Describe("column template cells", [this]()
		{
			It("starts a new filter cell matching anything", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_TemplateAny")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FEnumColumn::StaticStruct(), 1, TEXT("State"));
					UChooserColumnToolset::AddChooserColumn(Table, FFloatRangeColumn::StaticStruct(), 1, TEXT("Speed"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);

					TestTrue("a new enum cell compares against nothing",
						UChooserCellToolset::GetChooserCell(Table, 0, 0).Contains(TEXT("MatchAny")));
					TestTrue("a new range cell is unbounded",
						UChooserCellToolset::GetChooserCell(Table, 1, 0).Contains(TEXT("bNoMin=True")));
				});

			It("reports the template cell of every column", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_TemplateReport")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					TestEqual("template cell reported", Info.Columns[0].DefaultCell, FString(TEXT("MatchAny")));
				});

			It("changes what a new row starts from", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_TemplateSet")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					TestTrue("template cell written",
						UChooserColumnToolset::SetChooserColumnDefaultCell(Table, 0, TEXT("MatchTrue")));

					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);
					TestEqual("the new row started from the template",
						UChooserCellToolset::GetChooserCell(Table, 0, 0), FString(TEXT("MatchTrue")));
				});

			It("refuses a template cell it cannot parse", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_TemplateBadText")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FFloatRangeColumn::StaticStruct(), 1, TEXT("Speed"));
					TestFalse("refused", UChooserColumnToolset::SetChooserColumnDefaultCell(Table, 0, TEXT("not a range")));
				});

			It("refuses a column that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_TemplateNoColumn")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserColumnToolset::SetChooserColumnDefaultCell(Table, 0, TEXT("MatchAny")));
				});
		});

	Describe("output struct columns", [this]()
		{
			It("takes its struct type from the parameter it is bound to", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_OutputStruct")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FOutputStructColumn::StaticStruct(), 1, TEXT(""));

					const FOutputStructColumn* Column = Table->ColumnsStructs[0].GetPtr<FOutputStructColumn>();
					if (TestNotNull("column is an output struct column", Column))
					{
						TestTrue("template cell has the bound struct type",
							Column->DefaultRowValue.GetScriptStruct() == FChooserToolsetTestParameters::StaticStruct());
					}
				});

			It("keeps its cells when the table compiles", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_OutputStructCompile")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FOutputStructColumn::StaticStruct(), 1, TEXT(""));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);
					UChooserTableToolset::ValidateChooserTable(Table);

					const FOutputStructColumn* Column = Table->ColumnsStructs[0].GetPtr<FOutputStructColumn>();
					if (TestNotNull("column is an output struct column", Column))
					{
						TestTrue("the cell survived the compile",
							Column->RowValues[0].GetScriptStruct() == FChooserToolsetTestParameters::StaticStruct());
					}
				});

			It("carries its cell into a duplicated row", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_OutputStructDup")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FOutputStructColumn::StaticStruct(), 1, TEXT(""));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);
					TestEqual("copy sits below the original", UChooserRowToolset::DuplicateChooserRow(Table, 0), 1);

					const FOutputStructColumn* Column = Table->ColumnsStructs[0].GetPtr<FOutputStructColumn>();
					if (TestNotNull("column is an output struct column", Column))
					{
						TestTrue("the copied cell kept its type",
							Column->RowValues[1].GetScriptStruct() == FChooserToolsetTestParameters::StaticStruct());
					}
				});
		});

	Describe("SetChooserColumnSettings", [this]()
		{
			It("applies a column-wide setting", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_Settings")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FFloatDistanceColumn::StaticStruct(), 1, TEXT("Speed"));

					TMap<FString, FString> Settings;
					Settings.Add(TEXT("MaxDistance"), TEXT("250.0"));
					TestTrue("settings applied", UChooserColumnToolset::SetChooserColumnSettings(Table, 0, Settings));

					const FFloatDistanceColumn* Column = Table->ColumnsStructs[0].GetPtr<FFloatDistanceColumn>();
					if (TestNotNull("column is a float difference column", Column))
					{
						TestEqual("max distance applied", Column->MaxDistance, 250.0);
					}
				});

			It("reports settings back through DescribeChooserTable", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_SettingsRead")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FFloatDistanceColumn::StaticStruct(), 1, TEXT("Speed"));
					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					TestTrue("max distance listed", Info.Columns[0].Settings.Contains(TEXT("MaxDistance")));
					TestFalse("the disabled flag is not a setting, it has its own tool",
						Info.Columns[0].Settings.Contains(TEXT("bDisabled")));
				});

			It("refuses to write cells through the settings", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_SettingsCells")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));

					TMap<FString, FString> Settings;
					Settings.Add(TEXT("RowValuesWithAny"), TEXT("(MatchTrue)"));
					TestFalse("refused", UChooserColumnToolset::SetChooserColumnSettings(Table, 0, Settings));
				});

			It("refuses a setting the column does not have", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_SettingsBad")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));

					TMap<FString, FString> Settings;
					Settings.Add(TEXT("NotASetting"), TEXT("1"));
					TestFalse("refused", UChooserColumnToolset::SetChooserColumnSettings(Table, 0, Settings));
				});

			It("refuses a column that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_SettingsNoColumn")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserColumnToolset::SetChooserColumnSettings(Table, 0, TMap<FString, FString>()));
				});
		});

	Describe("RemoveChooserColumn", [this]()
		{
			It("removes a column", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_RemoveColumn")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					TestTrue("column removed", UChooserColumnToolset::RemoveChooserColumn(Table, 0));
					TestEqual("no columns left", Table->ColumnsStructs.Num(), 0);
				});

			It("refuses a column that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_RemoveColumnBad")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserColumnToolset::RemoveChooserColumn(Table, 0));
				});
		});

	Describe("MoveChooserColumn", [this]()
		{
			It("reorders columns", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_MoveColumn")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					UChooserColumnToolset::AddChooserColumn(Table, FFloatDistanceColumn::StaticStruct(), 1, TEXT("Speed"));

					TestTrue("column moved", UChooserColumnToolset::MoveChooserColumn(Table, 0, 2));
					TestTrue("float difference column is now first",
						Table->ColumnsStructs[0].GetPtr<FFloatDistanceColumn>() != nullptr);
				});

			It("refuses to move the randomize column", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_MoveRandom")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					UChooserColumnToolset::AddChooserColumn(Table, FRandomizeColumn::StaticStruct(), -1, TEXT(""));

					TestFalse("refused", UChooserColumnToolset::MoveChooserColumn(Table, 1, 0));
				});

			It("refuses to move a column past the randomize column", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_MovePastRandom")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					UChooserColumnToolset::AddChooserColumn(Table, FRandomizeColumn::StaticStruct(), -1, TEXT(""));

					TestFalse("refused", UChooserColumnToolset::MoveChooserColumn(Table, 0, 2));
				});

			It("refuses a column that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_MoveColumnBad")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserColumnToolset::MoveChooserColumn(Table, 0, 0));
				});
		});

	Describe("DuplicateChooserColumn", [this]()
		{
			It("copies a column and its binding", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_DupColumn")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					const int32 NewColumn = UChooserColumnToolset::DuplicateChooserColumn(Table, 0);
					TestEqual("copy sits to the right", NewColumn, 1);

					const FChooserToolsetTableInfo Info = UChooserTableToolset::DescribeChooserTable(Table);
					if (TestEqual("two columns", Info.Columns.Num(), 2))
					{
						TestEqual("copy kept the binding", Info.Columns[1].PropertyPath, FString(TEXT("bIsCrouching")));
					}
				});

			It("refuses to duplicate the randomize column", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_DupRandom")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FRandomizeColumn::StaticStruct(), -1, TEXT(""));
					TestEqual("refused", UChooserColumnToolset::DuplicateChooserColumn(Table, 0), INDEX_NONE);
				});

			It("refuses a column that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_DupColumnBad")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestEqual("refused", UChooserColumnToolset::DuplicateChooserColumn(Table, 0), INDEX_NONE);
				});
		});

	Describe("SetChooserColumnDisabled", [this]()
		{
			It("disables and re-enables a column", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_DisableColumn")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					TestTrue("column disabled", UChooserColumnToolset::SetChooserColumnDisabled(Table, 0, true));
					TestTrue("table agrees", Table->ColumnsStructs[0].Get<FChooserColumnBase>().bDisabled);

					TestTrue("column re-enabled", UChooserColumnToolset::SetChooserColumnDisabled(Table, 0, false));
					TestFalse("table agrees", Table->ColumnsStructs[0].Get<FChooserColumnBase>().bDisabled);
				});

			It("refuses a column that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_DisableColumnBad")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserColumnToolset::SetChooserColumnDisabled(Table, 0, true));
				});
		});

	Describe("AutoPopulateChooserColumn", [this]()
		{
			It("fills cells from the row results", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_AutoPopulate")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FFloatDistanceColumn::StaticStruct(), 1, TEXT("Speed"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);

					TMap<FString, FString> Settings;
					Settings.Add(TEXT("AutoPopulator"), UChooserToolsetTestAutoPopulator::StaticClass()->GetPathName());
					if (!TestTrue("auto populator assigned",
						UChooserColumnToolset::SetChooserColumnSettings(Table, 0, Settings)))
					{
						return;
					}

					TestTrue("column populated", UChooserColumnToolset::AutoPopulateChooserColumn(Table, 0));

					const FFloatDistanceColumn* Column = Table->ColumnsStructs[0].GetPtr<FFloatDistanceColumn>();
					if (TestNotNull("column still there", Column))
					{
						TestEqual("cell filled by the populator", Column->RowValues[0].Value,
							UChooserToolsetTestAutoPopulator::PopulatedValue);
					}
				});

			It("refuses a column that cannot populate itself", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_AutoPopulateBad")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
					TestFalse("refused", UChooserColumnToolset::AutoPopulateChooserColumn(Table, 0));
				});

			It("refuses a column that does not exist", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_AutoPopulateNone")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserColumnToolset::AutoPopulateChooserColumn(Table, 0));
				});
		});
}

#endif // WITH_DEV_AUTOMATION_TESTS
