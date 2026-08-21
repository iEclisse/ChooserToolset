// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#pragma once

#include "ChooserToolsetTypes.h"
#include "CoreMinimal.h"
#include "ObjectClassColumn.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "ChooserCellToolset.generated.h"

class UChooserTable;

/**
 * Reads and writes the cells of a chooser table: the value one column holds for one row.
 *
 * A cell means whatever its column type says it does. On a filter column it is the condition the row
 * requires; on a scoring column it is the value the row is ranked against; on an output column it is
 * the value written when the row wins. Row index -2 addresses the fallback row, which only has cells
 * in output columns.
 *
 * SetChooserCell takes the value in Unreal text format, which is what GetChooserCell returns and what
 * the CellFormat of ListChooserColumnTypes shows, e.g. "MatchTrue" for a Bool column or
 * "(Min=100.000000,Max=400.000000)" for a Float Range column. The typed tools below exist for the
 * values that text does not express well: enum names, gameplay tags, and real object references.
 */
UCLASS(BlueprintType)
class UChooserCellToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Reads a cell in the text format SetChooserCell accepts.
	 * @param Table The table to read from.
	 * @param ColumnIndex Column the cell belongs to.
	 * @param RowIndex Row the cell belongs to, or -2 for the fallback row.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Cells")
	static FString GetChooserCell(UChooserTable* Table, int32 ColumnIndex, int32 RowIndex);

	/**
	 * Writes a cell from text.
	 * @param Table The table to modify.
	 * @param ColumnIndex Column the cell belongs to.
	 * @param RowIndex Row the cell belongs to, or -2 for the fallback row.
	 * @param Value The value, in the format GetChooserCell returns for this column.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Cells")
	static bool SetChooserCell(UChooserTable* Table, int32 ColumnIndex, int32 RowIndex, const FString& Value);

	/**
	 * Writes a cell of an enum column, taking enum values by name rather than by their numeric value.
	 * @param Table The table to modify.
	 * @param ColumnIndex An Enum, Enum (Or) or Output Enum column.
	 * @param RowIndex Row the cell belongs to, or -2 for the fallback row.
	 * @param ValueNames Names of the enum values, e.g. ["Walking"]. An Enum (Or) column matches any of
	 *                   them; the other enum columns use the first and ignore the rest.
	 * @param Comparison How the row matches the input. Ignored by Enum (Or) and Output Enum columns.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Cells")
	static bool SetChooserEnumCell(
		UChooserTable* Table,
		int32 ColumnIndex,
		int32 RowIndex,
		const TArray<FString>& ValueNames,
		EChooserToolsetComparison Comparison = EChooserToolsetComparison::Equal);

	/**
	 * Writes a cell of a gameplay tag column. How the tags are matched against the input is a setting
	 * of the column, not of the cell.
	 * @param Table The table to modify.
	 * @param ColumnIndex A Gameplay Tag or Output Gameplay Tag column.
	 * @param RowIndex Row the cell belongs to, or -2 for the fallback row.
	 * @param Tags Tag names, e.g. ["Character.State.Crouched"]. Tags that are not registered raise.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Cells")
	static bool SetChooserGameplayTagCell(UChooserTable* Table, int32 ColumnIndex, int32 RowIndex, const TArray<FString>& Tags);

	/**
	 * Writes a cell of an object column.
	 * @param Table The table to modify.
	 * @param ColumnIndex An Object or Output Object column.
	 * @param RowIndex Row the cell belongs to, or -2 for the fallback row.
	 * @param Value Object the cell holds. Leave it out to clear the cell.
	 * @param Comparison How the row matches the input. Ignored by an Output Object column.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Cells")
	static bool SetChooserObjectCell(
		UChooserTable* Table,
		int32 ColumnIndex,
		int32 RowIndex,
		UObject* Value = nullptr,
		EChooserToolsetComparison Comparison = EChooserToolsetComparison::Equal);

	/**
	 * Writes a cell of an Object Class column, which tests the class of the object the column reads.
	 * @param Table The table to modify.
	 * @param ColumnIndex An Object Class column.
	 * @param RowIndex Row the cell belongs to.
	 * @param Value Class the cell tests against. Leave it out to clear the cell.
	 * @param Comparison How the input's class is compared to Value.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Cells")
	static bool SetChooserClassCell(
		UChooserTable* Table,
		int32 ColumnIndex,
		int32 RowIndex,
		UClass* Value = nullptr,
		EObjectClassColumnCellValueComparison Comparison = EObjectClassColumnCellValueComparison::SubClassOf);
};
