// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "BoolColumn.h"
#include "Chooser.h"
#include "ChooserCellToolset.h"
#include "ChooserColumnToolset.h"
#include "ChooserEvaluationToolset.h"
#include "ChooserRowToolset.h"
#include "ChooserTableToolset.h"
#include "ChooserToolsetTestFlags.h"
#include "ChooserToolsetTestTypes.h"
#include "EnumColumn.h"
#include "FloatRangeColumn.h"
#include "GameplayTagColumn.h"
#include "GameplayTagsManager.h"
#include "ObjectClassColumn.h"
#include "ObjectColumn.h"
#include "OutputFloatColumn.h"
#include "RandomizeColumn.h"

BEGIN_DEFINE_SPEC(FChooserToolsetTest_CellsAndEvaluation,
	"AI.Toolsets.ChooserToolset.CellsAndEvaluation", ChooserToolsetTest::Flags)
	int32 TestCounter = 0;

	/** Makes a unique asset name so tests never collide inside the transient mount point. */
	FString NextName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%d"), *Prefix, ++TestCounter);
	}

	/** A table with one bool filter column on the struct parameter, and one row per given result. */
	UChooserTable* MakeBoolTable(const FString& AssetName, const TArray<UObject*>& Results)
	{
		UChooserTable* Table = ChooserToolsetTest::MakeTable(AssetName);
		if (!Table)
		{
			return nullptr;
		}
		UChooserColumnToolset::AddChooserColumn(Table, FBoolColumn::StaticStruct(), 1, TEXT("bIsCrouching"));
		for (UObject* Result : Results)
		{
			UChooserRowToolset::AddChooserRow(Table, Result, false, -1);
		}
		return Table;
	}

	/** One evaluation parameter carrying a single struct field value. */
	FChooserToolsetEvaluationParameter MakeStructParameter(int32 ParameterIndex, const FString& Field, const FString& Value)
	{
		FChooserToolsetEvaluationParameter Parameter;
		Parameter.ParameterIndex = ParameterIndex;
		Parameter.Fields.Add(Field, Value);
		return Parameter;
	}
END_DEFINE_SPEC(FChooserToolsetTest_CellsAndEvaluation)

void FChooserToolsetTest_CellsAndEvaluation::Define()
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

	Describe("GetChooserCell", [this]()
		{
			It("reads a cell at its default value", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_ReadCell")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestEqual("new bool cells match anything",
						UChooserCellToolset::GetChooserCell(Table, 0, 0), FString(TEXT("MatchAny")));
				});

			It("returns nothing for a column that does not exist", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_ReadCellBadColumn")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestTrue("no value", UChooserCellToolset::GetChooserCell(Table, 5, 0).IsEmpty());
				});

			It("returns nothing for a row that does not exist", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_ReadCellBadRow")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestTrue("no value", UChooserCellToolset::GetChooserCell(Table, 0, 4).IsEmpty());
				});

			It("returns nothing without a table", [this]()
				{
					TestTrue("no value", UChooserCellToolset::GetChooserCell(nullptr, 0, 0).IsEmpty());
				});
		});

	Describe("SetChooserCell", [this]()
		{
			It("writes a bool cell", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_WriteBool")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					TestTrue("cell written", UChooserCellToolset::SetChooserCell(Table, 0, 0, TEXT("MatchTrue")));
					TestEqual("cell reads back", UChooserCellToolset::GetChooserCell(Table, 0, 0), FString(TEXT("MatchTrue")));
				});

			It("writes a structured cell", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_WriteRange")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FFloatRangeColumn::StaticStruct(), 1, TEXT("Speed"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);

					TestTrue("cell written", UChooserCellToolset::SetChooserCell(
						Table, 0, 0, TEXT("(Min=100.000000,Max=400.000000,bNoMin=False,bNoMax=False)")));

					const FFloatRangeColumn* Column = Table->ColumnsStructs[0].GetPtr<FFloatRangeColumn>();
					if (TestNotNull("column is a float range column", Column))
					{
						TestEqual("min applied", Column->RowValues[0].Min, 100.0f);
						TestEqual("max applied", Column->RowValues[0].Max, 400.0f);
					}
				});

			It("writes an output column's fallback cell", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_WriteFallbackCell")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FOutputFloatColumn::StaticStruct(), 1, TEXT("ChosenSpeed"));
					TestTrue("fallback cell written", UChooserCellToolset::SetChooserCell(Table, 0, -2, TEXT("3.5")));
					TestEqual("fallback cell reads back",
						FCString::Atod(*UChooserCellToolset::GetChooserCell(Table, 0, -2)), 3.5);
				});

			It("refuses a fallback cell on a filter column", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_FilterFallback")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserCellToolset::SetChooserCell(Table, 0, -2, TEXT("MatchTrue")));
				});

			It("refuses a value the cell cannot parse", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_BadCellText")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FFloatRangeColumn::StaticStruct(), 1, TEXT("Speed"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);
					TestFalse("refused", UChooserCellToolset::SetChooserCell(Table, 0, 0, TEXT("not a range")));
				});

			It("refuses a column that does not exist", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_WriteCellBad")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserCellToolset::SetChooserCell(Table, 5, 0, TEXT("MatchTrue")));
				});

			It("refuses to write without a table", [this]()
				{
					TestFalse("refused", UChooserCellToolset::SetChooserCell(nullptr, 0, 0, TEXT("MatchTrue")));
				});
		});

	Describe("SetChooserEnumCell", [this]()
		{
			It("writes an enum cell by value name", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_EnumCell")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FEnumColumn::StaticStruct(), 1, TEXT("State"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);

					TestTrue("cell written", UChooserCellToolset::SetChooserEnumCell(
						Table, 0, 0, { TEXT("Running") }, EChooserToolsetComparison::Equal));

					const FEnumColumn* Column = Table->ColumnsStructs[0].GetPtr<FEnumColumn>();
					if (TestNotNull("column is an enum column", Column))
					{
						TestEqual("value applied", static_cast<int32>(Column->RowValues[0].Value),
							static_cast<int32>(EChooserToolsetTestState::Running));
						TestTrue("comparison applied",
							Column->RowValues[0].Comparison == EEnumColumnCellValueComparison::MatchEqual);
					}
				});

			It("refuses a value the enum does not have", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_EnumCellBadValue")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FEnumColumn::StaticStruct(), 1, TEXT("State"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);
					TestFalse("refused", UChooserCellToolset::SetChooserEnumCell(
						Table, 0, 0, { TEXT("Flying") }, EChooserToolsetComparison::Equal));
				});

			It("refuses an empty list of values", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_EnumCellEmpty")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FEnumColumn::StaticStruct(), 1, TEXT("State"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);
					TestFalse("refused", UChooserCellToolset::SetChooserEnumCell(
						Table, 0, 0, {}, EChooserToolsetComparison::Equal));
				});

			It("refuses a column that is not enum based", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_EnumCellWrongColumn")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserCellToolset::SetChooserEnumCell(
						Table, 0, 0, { TEXT("Running") }, EChooserToolsetComparison::Equal));
				});
		});

	Describe("SetChooserGameplayTagCell", [this]()
		{
			It("writes tags into a gameplay tag cell", [this]()
				{
					const FGameplayTag TestTag = UGameplayTagsManager::Get().AddNativeGameplayTag(
						FName(TEXT("ChooserToolsetTest.Crouched")));
					if (!TestTrue("test tag registered", TestTag.IsValid()))
					{
						return;
					}

					UChooserTable* Table = MakeTable(NextName(TEXT("CT_TagCell")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FGameplayTagColumn::StaticStruct(), 1, TEXT("Tags"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);

					TestTrue("cell written", UChooserCellToolset::SetChooserGameplayTagCell(
						Table, 0, 0, { TEXT("ChooserToolsetTest.Crouched") }));

					const FGameplayTagColumn* Column = Table->ColumnsStructs[0].GetPtr<FGameplayTagColumn>();
					if (TestNotNull("column is a gameplay tag column", Column))
					{
						TestTrue("tag applied", Column->RowValues[0].HasTagExact(TestTag));
					}
				});

			It("refuses a tag that is not registered", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_TagCellBad")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FGameplayTagColumn::StaticStruct(), 1, TEXT("Tags"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);
					TestFalse("refused", UChooserCellToolset::SetChooserGameplayTagCell(
						Table, 0, 0, { TEXT("ChooserToolsetTest.NotARegisteredTag") }));
				});

			It("refuses a column that does not hold tags", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_TagCellWrongColumn")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserCellToolset::SetChooserGameplayTagCell(Table, 0, 0, {}));
				});
		});

	Describe("SetChooserObjectCell", [this]()
		{
			It("writes an object cell", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_ObjectCell")));
					UObject* Value = MakeContext();
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FObjectColumn::StaticStruct(), 1, TEXT("Target"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);

					TestTrue("cell written", UChooserCellToolset::SetChooserObjectCell(
						Table, 0, 0, Value, EChooserToolsetComparison::NotEqual));

					const FObjectColumn* Column = Table->ColumnsStructs[0].GetPtr<FObjectColumn>();
					if (TestNotNull("column is an object column", Column))
					{
						TestEqual("object applied", Column->RowValues[0].Value.Get(), Value);
						TestTrue("comparison applied",
							Column->RowValues[0].Comparison == EObjectColumnCellValueComparison::MatchNotEqual);
					}
				});

			It("refuses a column that does not hold objects", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_ObjectCellWrong")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserCellToolset::SetChooserObjectCell(
						Table, 0, 0, MakeContext(), EChooserToolsetComparison::Equal));
				});

			It("refuses to write without a table", [this]()
				{
					TestFalse("refused", UChooserCellToolset::SetChooserObjectCell(
						nullptr, 0, 0, nullptr, EChooserToolsetComparison::Equal));
				});
		});

	Describe("SetChooserClassCell", [this]()
		{
			It("writes a class cell", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_ClassCell")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FObjectClassColumn::StaticStruct(), 1, TEXT("Target"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);

					TestTrue("cell written", UChooserCellToolset::SetChooserClassCell(
						Table, 0, 0, UChooserToolsetTestContext::StaticClass(),
						EObjectClassColumnCellValueComparison::SubClassOf));

					const FObjectClassColumn* Column = Table->ColumnsStructs[0].GetPtr<FObjectClassColumn>();
					if (TestNotNull("column is an object class column", Column))
					{
						TestEqual("class applied", Column->RowValues[0].Value.Get(),
							UChooserToolsetTestContext::StaticClass());
					}
				});

			It("refuses a column that is not an object class column", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_ClassCellWrong")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestFalse("refused", UChooserCellToolset::SetChooserClassCell(
						Table, 0, 0, UObject::StaticClass(), EObjectClassColumnCellValueComparison::SubClassOf));
				});
		});

	Describe("EvaluateChooserTable", [this]()
		{
			It("returns the first row that passes every filter", [this]()
				{
					UObject* Crouched = MakeContext();
					UObject* Standing = MakeContext();
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_Eval")), { Crouched, Standing });
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserCellToolset::SetChooserCell(Table, 0, 0, TEXT("MatchTrue"));
					UChooserCellToolset::SetChooserCell(Table, 0, 1, TEXT("MatchFalse"));

					FChooserToolsetEvaluationParameter Context;
					Context.ParameterIndex = 0;
					Context.Object = MakeContext();

					const FChooserToolsetEvaluationResult Result = UChooserEvaluationToolset::EvaluateChooserTable(
						Table, { Context, MakeStructParameter(1, TEXT("bIsCrouching"), TEXT("true")) }, false, -1);

					if (TestEqual("one result", Result.Results.Num(), 1))
					{
						TestEqual("crouched row selected", Result.Results[0].Get(), Crouched);
					}
					if (TestEqual("one row reported", Result.SelectedRows.Num(), 1))
					{
						TestEqual("row 0 selected", Result.SelectedRows[0], 0);
					}
				});

			It("falls back when no row passes", [this]()
				{
					UObject* Crouched = MakeContext();
					UObject* Fallback = MakeContext();
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_EvalFallback")), { Crouched });
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserCellToolset::SetChooserCell(Table, 0, 0, TEXT("MatchTrue"));
					UChooserRowToolset::SetChooserRowResult(Table, -2, Fallback, false);

					FChooserToolsetEvaluationParameter Context;
					Context.ParameterIndex = 0;
					Context.Object = MakeContext();

					const FChooserToolsetEvaluationResult Result = UChooserEvaluationToolset::EvaluateChooserTable(
						Table, { Context, MakeStructParameter(1, TEXT("bIsCrouching"), TEXT("false")) }, false, -1);

					if (TestEqual("one result", Result.Results.Num(), 1))
					{
						TestEqual("fallback selected", Result.Results[0].Get(), Fallback);
					}
					if (TestEqual("one row reported", Result.SelectedRows.Num(), 1))
					{
						TestEqual("fallback row reported as -2", Result.SelectedRows[0], -2);
					}
				});

			It("collects every matching row when asked", [this]()
				{
					UChooserTable* Table = MakeBoolTable(
						NextName(TEXT("CT_EvalMulti")), { MakeContext(), MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					FChooserToolsetEvaluationParameter Context;
					Context.ParameterIndex = 0;
					Context.Object = MakeContext();

					const FChooserToolsetEvaluationResult Result = UChooserEvaluationToolset::EvaluateChooserTable(
						Table, { Context, MakeStructParameter(1, TEXT("bIsCrouching"), TEXT("true")) }, true, -1);
					TestEqual("both rows returned", Result.Results.Num(), 2);
				});

			It("reports what an output column wrote", [this]()
				{
					UChooserTable* Table = MakeTable(NextName(TEXT("CT_EvalOutput")));
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FOutputFloatColumn::StaticStruct(), 1, TEXT("ChosenSpeed"));
					UChooserRowToolset::AddChooserRow(Table, MakeContext(), false, -1);
					UChooserCellToolset::SetChooserCell(Table, 0, 0, TEXT("42.0"));

					FChooserToolsetEvaluationParameter Context;
					Context.ParameterIndex = 0;
					Context.Object = MakeContext();

					const FChooserToolsetEvaluationResult Result = UChooserEvaluationToolset::EvaluateChooserTable(
						Table, { Context }, false, -1);

					const FString* Written = Result.Outputs.Find(TEXT("1.ChosenSpeed"));
					if (TestNotNull("output reported", Written))
					{
						TestEqual("output value written", FCString::Atod(**Written), 42.0);
					}
				});

			It("gives the same answer twice for the same random seed", [this]()
				{
					UChooserTable* Table = MakeBoolTable(
						NextName(TEXT("CT_EvalSeed")), { MakeContext(), MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserColumnToolset::AddChooserColumn(Table, FRandomizeColumn::StaticStruct(), -1, TEXT(""));

					FChooserToolsetEvaluationParameter Context;
					Context.ParameterIndex = 0;
					Context.Object = MakeContext();
					const TArray<FChooserToolsetEvaluationParameter> Parameters = { Context };

					const FChooserToolsetEvaluationResult First =
						UChooserEvaluationToolset::EvaluateChooserTable(Table, Parameters, false, 1234);
					const FChooserToolsetEvaluationResult Second =
						UChooserEvaluationToolset::EvaluateChooserTable(Table, Parameters, false, 1234);

					if (TestEqual("first run picked something", First.Results.Num(), 1)
						&& TestEqual("second run picked something", Second.Results.Num(), 1))
					{
						TestEqual("same row both times", First.Results[0].Get(), Second.Results[0].Get());
					}
				});

			It("refuses a parameter index the table does not have", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_EvalBadParam")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					FChooserToolsetEvaluationParameter Context;
					Context.ParameterIndex = 7;

					const FChooserToolsetEvaluationResult Result =
						UChooserEvaluationToolset::EvaluateChooserTable(Table, { Context }, false, -1);
					TestTrue("nothing selected", Result.Results.IsEmpty());
				});

			It("refuses an object of the wrong class", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_EvalWrongObject")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					FChooserToolsetEvaluationParameter Context;
					Context.ParameterIndex = 0;
					Context.Object = Table;

					const FChooserToolsetEvaluationResult Result =
						UChooserEvaluationToolset::EvaluateChooserTable(Table, { Context }, false, -1);
					TestTrue("nothing selected", Result.Results.IsEmpty());
				});

			It("refuses a field the struct parameter does not have", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_EvalBadField")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					const FChooserToolsetEvaluationResult Result = UChooserEvaluationToolset::EvaluateChooserTable(
						Table, { MakeStructParameter(1, TEXT("NotAField"), TEXT("1")) }, false, -1);
					TestTrue("nothing selected", Result.Results.IsEmpty());
				});

			It("returns nothing without a table", [this]()
				{
					const FChooserToolsetEvaluationResult Result =
						UChooserEvaluationToolset::EvaluateChooserTable(nullptr, {}, false, -1);
					TestTrue("nothing selected", Result.Results.IsEmpty());
				});
		});

	Describe("debug targets", [this]()
		{
			It("lists the objects that recently evaluated the table", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_DebugList")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					UChooserToolsetTestContext* Evaluator = MakeContext();
					FChooserToolsetEvaluationParameter Context;
					Context.ParameterIndex = 0;
					Context.Object = Evaluator;
					UChooserEvaluationToolset::EvaluateChooserTable(Table, { Context }, false, -1);

					const TArray<FString> Targets = UChooserEvaluationToolset::ListChooserDebugTargets(Table);
					TestTrue("the evaluating object is listed", Targets.Contains(Evaluator->GetName()));
				});

			It("returns nothing for a table nothing has evaluated", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_DebugNone")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}
					TestTrue("no targets", UChooserEvaluationToolset::ListChooserDebugTargets(Table).IsEmpty());
				});

			It("returns nothing without a table", [this]()
				{
					TestTrue("no targets", UChooserEvaluationToolset::ListChooserDebugTargets(nullptr).IsEmpty());
				});

			It("follows a debug target and stops following it", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_DebugTarget")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					TestTrue("target set", UChooserEvaluationToolset::SetChooserDebugTarget(Table, TEXT("SomeActor")));
					TestEqual("target stored", Table->GetDebugTargetName(), FString(TEXT("SomeActor")));

					TestTrue("target cleared", UChooserEvaluationToolset::SetChooserDebugTarget(Table, TEXT("")));
					TestFalse("nothing followed", Table->HasDebugTarget());
				});

			It("refuses to set a debug target without a table", [this]()
				{
					TestFalse("refused", UChooserEvaluationToolset::SetChooserDebugTarget(nullptr, TEXT("SomeActor")));
				});

			It("reports the rows the last evaluation selected", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_DebugRows")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					FChooserToolsetEvaluationParameter Context;
					Context.ParameterIndex = 0;
					Context.Object = MakeContext();
					UChooserEvaluationToolset::EvaluateChooserTable(Table, { Context }, false, -1);

					const TArray<int32> Rows = UChooserEvaluationToolset::GetChooserDebugSelectedRows(Table);
					if (TestEqual("one row selected", Rows.Num(), 1))
					{
						TestEqual("row 0 selected", Rows[0], 0);
					}
				});

			It("leaves a debug target the caller set alone", [this]()
				{
					UChooserTable* Table = MakeBoolTable(NextName(TEXT("CT_DebugKeep")), { MakeContext() });
					if (!TestNotNull("table created", Table))
					{
						return;
					}

					// Evaluating points the debug target at the caller's own context object so the
					// chooser records the row it picked, but a target set for Play In Editor has to
					// survive that.
					UChooserEvaluationToolset::SetChooserDebugTarget(Table, TEXT("SomeActor"));

					FChooserToolsetEvaluationParameter Context;
					Context.ParameterIndex = 0;
					Context.Object = MakeContext();
					UChooserEvaluationToolset::EvaluateChooserTable(Table, { Context }, false, -1);

					TestEqual("the target still stands", Table->GetDebugTargetName(), FString(TEXT("SomeActor")));
				});

			It("reports nothing without a table", [this]()
				{
					TestTrue("no rows", UChooserEvaluationToolset::GetChooserDebugSelectedRows(nullptr).IsEmpty());
				});
		});
}

#endif // WITH_DEV_AUTOMATION_TESTS
