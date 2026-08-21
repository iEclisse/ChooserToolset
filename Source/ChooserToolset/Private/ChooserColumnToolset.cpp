// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#include "ChooserColumnToolset.h"

#include "Chooser.h"
#include "ChooserToolsetUtils.h"
#include "IChooserColumn.h"
#include "Misc/StringOutputDevice.h"

using namespace UE::ChooserToolset;

namespace
{
	/** True when the column at ColumnIndex is a Randomize column, which must stay last. */
	bool IsRandomizeColumn(const UChooserTable* Table, int32 ColumnIndex)
	{
		if (!Table->ColumnsStructs.IsValidIndex(ColumnIndex))
		{
			return false;
		}
		const FChooserColumnBase* Column = Table->ColumnsStructs[ColumnIndex].GetPtr<FChooserColumnBase>();
		return Column && Column->IsRandomizeColumn();
	}

	/** Where a new column of this kind belongs, mirroring the order the chooser editor keeps. */
	int32 FindInsertIndex(const UChooserTable* Table, const FChooserColumnBase& NewColumn)
	{
		const int32 ColumnCount = Table->ColumnsStructs.Num();
		if (NewColumn.IsRandomizeColumn())
		{
			return ColumnCount;
		}

		int32 InsertIndex = ColumnCount;
		if (InsertIndex > 0 && IsRandomizeColumn(Table, InsertIndex - 1))
		{
			--InsertIndex;
		}
		if (NewColumn.HasOutputs())
		{
			return InsertIndex;
		}

		int32 FilterEnd = 0;
		while (FilterEnd < InsertIndex)
		{
			const FChooserColumnBase& Column = Table->ColumnsStructs[FilterEnd].Get<FChooserColumnBase>();
			if (Column.HasOutputs())
			{
				break;
			}
			++FilterEnd;
		}
		return FilterEnd;
	}
}

TArray<FChooserToolsetColumnTypeInfo> UChooserColumnToolset::ListChooserColumnTypes(const FString& NameFilter)
{
	TArray<FChooserToolsetColumnTypeInfo> Result;
	for (const UScriptStruct* ColumnType : FindColumnTypes())
	{
		FChooserToolsetColumnTypeInfo Info = MakeColumnTypeInfo(ColumnType);
		if (NameFilter.IsEmpty()
			|| Info.DisplayName.Contains(NameFilter, ESearchCase::IgnoreCase)
			|| ColumnType->GetName().Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			Result.Add(MoveTemp(Info));
		}
	}
	return Result;
}

int32 UChooserColumnToolset::AddChooserColumn(
	UChooserTable* Table, UScriptStruct* ColumnType, int32 ParameterIndex, const FString& PropertyPath)
{
	static const TCHAR* Tool = TEXT("AddChooserColumn");
	if (!PrepareTable(Tool, Table))
	{
		return INDEX_NONE;
	}
	if (!ColumnType)
	{
		FailTool(Tool, TEXT("ColumnType is required, use ListChooserColumnTypes to find one."));
		return INDEX_NONE;
	}
	if (!ColumnType->IsChildOf(FChooserColumnBase::StaticStruct()) || ColumnType == FChooserColumnBase::StaticStruct())
	{
		FailTool(Tool, FString::Printf(TEXT("'%s' is not a chooser column type."), *ColumnType->GetName()));
		return INDEX_NONE;
	}

	FInstancedStruct NewColumnData;
	NewColumnData.InitializeAs(ColumnType);
	FChooserColumnBase& NewColumn = NewColumnData.GetMutable<FChooserColumnBase>();

	if (NewColumn.IsRandomizeColumn())
	{
		for (int32 ColumnIndex = 0; ColumnIndex < Table->ColumnsStructs.Num(); ++ColumnIndex)
		{
			if (IsRandomizeColumn(Table, ColumnIndex))
			{
				FailTool(Tool, TEXT("the table already has a Randomize column, and only one is allowed."));
				return INDEX_NONE;
			}
		}
	}

	NotifyTableChanged(Table);
	NewColumn.Initialize(Table);
	RelaxNewColumnDefaultCell(NewColumnData);

	const int32 InsertIndex = FindInsertIndex(Table, NewColumn);
	Table->ColumnsStructs.Insert(MoveTemp(NewColumnData), InsertIndex);
	SyncRowArrays(Table);

	if (ParameterIndex >= 0)
	{
		FString Error;
		if (!BindColumn(Table, InsertIndex, ParameterIndex, PropertyPath, Error))
		{
			Table->ColumnsStructs.RemoveAt(InsertIndex);
			FailTool(Tool, Error);
			return INDEX_NONE;
		}
	}

	return InsertIndex;
}

bool UChooserColumnToolset::SetChooserColumnBinding(
	UChooserTable* Table, int32 ColumnIndex, int32 ParameterIndex, const FString& PropertyPath)
{
	static const TCHAR* Tool = TEXT("SetChooserColumnBinding");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}

	FString Error;
	if (!BindColumn(Table, ColumnIndex, ParameterIndex, PropertyPath, Error))
	{
		FailTool(Tool, Error);
		return false;
	}
	return true;
}

bool UChooserColumnToolset::SetChooserColumnSettings(UChooserTable* Table, int32 ColumnIndex, const TMap<FString, FString>& Settings)
{
	static const TCHAR* Tool = TEXT("SetChooserColumnSettings");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}

	FString Error;
	FInstancedStruct* ColumnData = GetColumnStruct(Table, ColumnIndex, Error);
	if (!ColumnData)
	{
		FailTool(Tool, Error);
		return false;
	}

	FChooserColumnBase& Column = ColumnData->GetMutable<FChooserColumnBase>();
	const TArray<FString> Allowed = GetSettingNames(ColumnData->GetScriptStruct(), Column.RowValuesPropertyName());
	for (const TPair<FString, FString>& Setting : Settings)
	{
		const bool bIsSetting = Allowed.ContainsByPredicate(
			[&Setting](const FString& Name)
			{
				return Name.Equals(Setting.Key, ESearchCase::IgnoreCase);
			});
		if (!bIsSetting)
		{
			FailTool(Tool, FString::Printf(TEXT("column %d has no setting named '%s'. Its settings are: %s."),
				ColumnIndex, *Setting.Key,
				Allowed.IsEmpty() ? TEXT("none") : *FString::Join(Allowed, TEXT(", "))));
			return false;
		}
	}

	NotifyTableChanged(Table);
	if (!ApplyStructSettings(ColumnData->GetScriptStruct(), ColumnData->GetMutableMemory(), Settings, Error))
	{
		FailTool(Tool, Error);
		return false;
	}

	Table->Compile(true);
	return true;
}

bool UChooserColumnToolset::SetChooserColumnDefaultCell(UChooserTable* Table, int32 ColumnIndex, const FString& Value)
{
	static const TCHAR* Tool = TEXT("SetChooserColumnDefaultCell");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}

	void* DefaultCell = nullptr;
	FString Error;
	FProperty* Property = GetDefaultCellMemory(Table, ColumnIndex, DefaultCell, Error);
	if (!Property)
	{
		FailTool(Tool, Error);
		return false;
	}

	NotifyTableChanged(Table);

	FStringOutputDevice ImportErrors;
	const TCHAR* Result = Property->ImportText_Direct(*Value, DefaultCell, Table, PPF_None, &ImportErrors);
	if (!Result || !ImportErrors.IsEmpty())
	{
		FailTool(Tool, FString::Printf(TEXT("'%s' is not a valid cell for this column: %s"),
			*Value, ImportErrors.IsEmpty() ? TEXT("could not be parsed") : *ImportErrors));
		return false;
	}
	return true;
}

bool UChooserColumnToolset::RemoveChooserColumn(UChooserTable* Table, int32 ColumnIndex)
{
	static const TCHAR* Tool = TEXT("RemoveChooserColumn");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}

	FString Error;
	if (!GetColumnStruct(Table, ColumnIndex, Error))
	{
		FailTool(Tool, Error);
		return false;
	}

	NotifyTableChanged(Table);
	Table->ColumnsStructs.RemoveAt(ColumnIndex);
	Table->Compile(true);
	return true;
}

bool UChooserColumnToolset::MoveChooserColumn(UChooserTable* Table, int32 ColumnIndex, int32 TargetIndex)
{
	static const TCHAR* Tool = TEXT("MoveChooserColumn");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}

	FString Error;
	if (!GetColumnStruct(Table, ColumnIndex, Error))
	{
		FailTool(Tool, Error);
		return false;
	}
	if (TargetIndex < 0 || TargetIndex > Table->ColumnsStructs.Num())
	{
		FailTool(Tool, FString::Printf(TEXT("TargetIndex %d is outside 0..%d."), TargetIndex, Table->ColumnsStructs.Num()));
		return false;
	}
	if (IsRandomizeColumn(Table, ColumnIndex))
	{
		FailTool(Tool, TEXT("a Randomize column has to stay last, so it cannot be moved."));
		return false;
	}

	const int32 LastColumn = Table->ColumnsStructs.Num() - 1;
	if (IsRandomizeColumn(Table, LastColumn) && TargetIndex > LastColumn)
	{
		FailTool(Tool, TEXT("no column can be placed after the Randomize column."));
		return false;
	}
	if (TargetIndex == ColumnIndex)
	{
		return true;
	}

	NotifyTableChanged(Table);
	FInstancedStruct Column = MoveTemp(Table->ColumnsStructs[ColumnIndex]);
	Table->ColumnsStructs.RemoveAt(ColumnIndex);

	const int32 InsertIndex = ColumnIndex < TargetIndex ? TargetIndex - 1 : TargetIndex;
	Table->ColumnsStructs.Insert(MoveTemp(Column), InsertIndex);

	Table->Compile(true);
	return true;
}

int32 UChooserColumnToolset::DuplicateChooserColumn(UChooserTable* Table, int32 ColumnIndex)
{
	static const TCHAR* Tool = TEXT("DuplicateChooserColumn");
	if (!PrepareTable(Tool, Table))
	{
		return INDEX_NONE;
	}

	FString Error;
	FInstancedStruct* ColumnData = GetColumnStruct(Table, ColumnIndex, Error);
	if (!ColumnData)
	{
		FailTool(Tool, Error);
		return INDEX_NONE;
	}
	if (IsRandomizeColumn(Table, ColumnIndex))
	{
		FailTool(Tool, TEXT("only one Randomize column is allowed, so it cannot be duplicated."));
		return INDEX_NONE;
	}

	const int32 NewColumnIndex = ColumnIndex + 1;

	NotifyTableChanged(Table);

	// Copied out first: inserting an element of the array being grown is not allowed.
	FInstancedStruct Copy = *ColumnData;
	Table->ColumnsStructs.Insert(MoveTemp(Copy), NewColumnIndex);
	Table->ColumnsStructs[NewColumnIndex].GetMutable<FChooserColumnBase>().Initialize(Table);

	Table->Compile(true);
	return NewColumnIndex;
}

bool UChooserColumnToolset::SetChooserColumnDisabled(UChooserTable* Table, int32 ColumnIndex, bool bDisabled)
{
	static const TCHAR* Tool = TEXT("SetChooserColumnDisabled");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}

	FString Error;
	FInstancedStruct* ColumnData = GetColumnStruct(Table, ColumnIndex, Error);
	if (!ColumnData)
	{
		FailTool(Tool, Error);
		return false;
	}

	NotifyTableChanged(Table);
	ColumnData->GetMutable<FChooserColumnBase>().bDisabled = bDisabled;
	return true;
}

bool UChooserColumnToolset::AutoPopulateChooserColumn(UChooserTable* Table, int32 ColumnIndex)
{
	static const TCHAR* Tool = TEXT("AutoPopulateChooserColumn");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}

	FString Error;
	FInstancedStruct* ColumnData = GetColumnStruct(Table, ColumnIndex, Error);
	if (!ColumnData)
	{
		FailTool(Tool, Error);
		return false;
	}

	FChooserColumnBase& Column = ColumnData->GetMutable<FChooserColumnBase>();
	if (!Column.AutoPopulates())
	{
		FailTool(Tool, FString::Printf(TEXT("column %d cannot derive its cells from the row results."), ColumnIndex));
		return false;
	}

	NotifyTableChanged(Table);
	for (int32 RowIndex = 0; RowIndex < Table->ResultsStructs.Num(); ++RowIndex)
	{
		Column.AutoPopulate(RowIndex, GetResultObject(Table->ResultsStructs[RowIndex]));
	}
	Column.AutoPopulate(FallbackRowIndex, GetResultObject(Table->FallbackResult));
	return true;
}
