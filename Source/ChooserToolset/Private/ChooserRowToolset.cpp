// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#include "ChooserRowToolset.h"

#include "Chooser.h"
#include "ChooserToolsetUtils.h"
#include "IChooserColumn.h"

using namespace UE::ChooserToolset;

namespace
{
	/** Validates a row index that must address an existing row rather than the fallback row. */
	bool RequireRow(const TCHAR* Tool, const UChooserTable* Table, int32 RowIndex)
	{
		if (!Table->ResultsStructs.IsValidIndex(RowIndex))
		{
			FailTool(Tool, FString::Printf(TEXT("row %d does not exist, the table has %d rows."),
				RowIndex, Table->ResultsStructs.Num()));
			return false;
		}
		return true;
	}
}

int32 UChooserRowToolset::AddChooserRow(UChooserTable* Table, UObject* Result, bool bSoftReference, int32 InsertAt)
{
	static const TCHAR* Tool = TEXT("AddChooserRow");
	if (!PrepareTable(Tool, Table))
	{
		return INDEX_NONE;
	}

	FInstancedStruct ResultStruct;
	FString Error;
	if (!MakeResultStruct(Table, Result, bSoftReference, ResultStruct, Error))
	{
		FailTool(Tool, Error);
		return INDEX_NONE;
	}

	const int32 RowIndex = (InsertAt < 0 || InsertAt > Table->ResultsStructs.Num())
		? Table->ResultsStructs.Num()
		: InsertAt;

	NotifyTableChanged(Table);
	Table->ResultsStructs.Insert(MoveTemp(ResultStruct), RowIndex);
	Table->DisabledRows.Insert(false, RowIndex);

	for (FInstancedStruct& ColumnData : Table->ColumnsStructs)
	{
		if (ColumnData.IsValid())
		{
			ColumnData.GetMutable<FChooserColumnBase>().InsertRows(RowIndex, 1);
		}
	}

	Table->Compile(true);
	return RowIndex;
}

bool UChooserRowToolset::SetChooserRowResult(UChooserTable* Table, int32 RowIndex, UObject* Result, bool bSoftReference)
{
	static const TCHAR* Tool = TEXT("SetChooserRowResult");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}
	if (RowIndex != FallbackRowIndex && !RequireRow(Tool, Table, RowIndex))
	{
		return false;
	}

	FInstancedStruct ResultStruct;
	FString Error;
	if (!MakeResultStruct(Table, Result, bSoftReference, ResultStruct, Error))
	{
		FailTool(Tool, Error);
		return false;
	}

	NotifyTableChanged(Table);
	if (RowIndex == FallbackRowIndex)
	{
		Table->FallbackResult = MoveTemp(ResultStruct);
	}
	else
	{
		Table->ResultsStructs[RowIndex] = MoveTemp(ResultStruct);
	}

	Table->Compile(true);
	return true;
}

bool UChooserRowToolset::RemoveChooserRow(UChooserTable* Table, int32 RowIndex)
{
	static const TCHAR* Tool = TEXT("RemoveChooserRow");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}
	if (RowIndex != FallbackRowIndex && !RequireRow(Tool, Table, RowIndex))
	{
		return false;
	}

	NotifyTableChanged(Table);

	// Columns clear their own references while the row still exists, which is why they go first.
	TArray<int32> RowsToDelete = { RowIndex };
	for (FInstancedStruct& ColumnData : Table->ColumnsStructs)
	{
		if (ColumnData.IsValid())
		{
			ColumnData.GetMutable<FChooserColumnBase>().DeleteRows(RowsToDelete);
		}
	}

	if (RowIndex == FallbackRowIndex)
	{
		Table->FallbackResult.Reset();
	}
	else
	{
		Table->ResultsStructs.RemoveAt(RowIndex);
		Table->DisabledRows.RemoveAt(RowIndex);
	}

	Table->Compile(true);
	return true;
}

bool UChooserRowToolset::MoveChooserRow(UChooserTable* Table, int32 RowIndex, int32 TargetIndex)
{
	static const TCHAR* Tool = TEXT("MoveChooserRow");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}
	if (!RequireRow(Tool, Table, RowIndex))
	{
		return false;
	}
	if (TargetIndex < 0 || TargetIndex > Table->ResultsStructs.Num())
	{
		FailTool(Tool, FString::Printf(TEXT("TargetIndex %d is outside 0..%d."),
			TargetIndex, Table->ResultsStructs.Num()));
		return false;
	}
	if (TargetIndex == RowIndex)
	{
		return true;
	}

	NotifyTableChanged(Table);

	for (FInstancedStruct& ColumnData : Table->ColumnsStructs)
	{
		if (ColumnData.IsValid())
		{
			ColumnData.GetMutable<FChooserColumnBase>().MoveRow(RowIndex, TargetIndex);
		}
	}

	FInstancedStruct Result = MoveTemp(Table->ResultsStructs[RowIndex]);
	const bool bDisabled = Table->DisabledRows[RowIndex];
	Table->ResultsStructs.RemoveAt(RowIndex);
	Table->DisabledRows.RemoveAt(RowIndex);

	const int32 InsertIndex = RowIndex < TargetIndex ? TargetIndex - 1 : TargetIndex;
	Table->ResultsStructs.Insert(MoveTemp(Result), InsertIndex);
	Table->DisabledRows.Insert(bDisabled, InsertIndex);

	Table->Compile(true);
	return true;
}

int32 UChooserRowToolset::DuplicateChooserRow(UChooserTable* Table, int32 RowIndex)
{
	static const TCHAR* Tool = TEXT("DuplicateChooserRow");
	if (!PrepareTable(Tool, Table))
	{
		return INDEX_NONE;
	}
	if (!RequireRow(Tool, Table, RowIndex))
	{
		return INDEX_NONE;
	}

	const int32 NewRowIndex = RowIndex + 1;

	NotifyTableChanged(Table);

	// Copied out first: inserting an element of the array being grown is not allowed.
	FInstancedStruct ResultCopy = Table->ResultsStructs[RowIndex];
	const bool bDisabled = Table->DisabledRows[RowIndex];
	Table->ResultsStructs.Insert(MoveTemp(ResultCopy), NewRowIndex);
	Table->DisabledRows.Insert(bDisabled, NewRowIndex);

	for (FInstancedStruct& ColumnData : Table->ColumnsStructs)
	{
		if (!ColumnData.IsValid())
		{
			continue;
		}
		FChooserColumnBase& Column = ColumnData.GetMutable<FChooserColumnBase>();
		Column.InsertRows(NewRowIndex, 1);
		Column.CopyRow(Column, RowIndex, NewRowIndex);
	}

	Table->Compile(true);
	return NewRowIndex;
}

bool UChooserRowToolset::SetChooserRowDisabled(UChooserTable* Table, int32 RowIndex, bool bDisabled)
{
	static const TCHAR* Tool = TEXT("SetChooserRowDisabled");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}
	if (RowIndex == FallbackRowIndex)
	{
		FailTool(Tool, TEXT("the fallback row cannot be disabled, remove it instead."));
		return false;
	}
	if (!RequireRow(Tool, Table, RowIndex))
	{
		return false;
	}

	NotifyTableChanged(Table);
	Table->DisabledRows[RowIndex] = bDisabled;
	return true;
}
