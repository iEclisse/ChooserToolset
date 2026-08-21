// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#pragma once

#include "ChooserToolsetTypes.h"
#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "ChooserColumnToolset.generated.h"

class UChooserTable;

/**
 * Manages the columns of a chooser table: which condition each column tests, which property of which
 * parameter it reads or writes, and the settings that apply to the whole column.
 *
 * Each column binds to one property of one table parameter, so add the parameters first. Filter columns
 * reject rows whose cell does not match the property; scoring columns rank the survivors; output columns
 * write a value back to the property of the winning row. Column order matters only for outputs, which
 * always run after the filters, and for the Randomize column, which must be last.
 */
UCLASS(BlueprintType)
class UChooserColumnToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Lists the column types a table can use, with the kind of property each binds to and the format
	 * of its cells. Use this before AddChooserColumn instead of guessing a type.
	 * @param NameFilter Case-insensitive substring the column name must contain. Empty returns all.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Columns")
	static TArray<FChooserToolsetColumnTypeInfo> ListChooserColumnTypes(const FString& NameFilter = TEXT(""));

	/**
	 * Adds a column and returns its index. The column is placed where the chooser editor would put it:
	 * filters before outputs, and a Randomize column last.
	 * @param Table The table to modify.
	 * @param ColumnType Column type from ListChooserColumnTypes.
	 * @param ParameterIndex Parameter the column reads or writes. Pass -1 to add the column unbound.
	 * @param PropertyPath Dot separated property of that parameter, e.g. "CharacterMovement.MaxWalkSpeed".
	 *                     Empty binds the parameter itself, which is what an Output Struct column writing
	 *                     a whole struct, or an Object column comparing whole objects, wants.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Columns")
	static int32 AddChooserColumn(
		UChooserTable* Table, UScriptStruct* ColumnType, int32 ParameterIndex = -1, const FString& PropertyPath = TEXT(""));

	/**
	 * Points an existing column at a different property.
	 * @param Table The table to modify.
	 * @param ColumnIndex Column to rebind.
	 * @param ParameterIndex Parameter the column reads or writes.
	 * @param PropertyPath Dot separated property of that parameter. Empty binds the parameter itself.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Columns")
	static bool SetChooserColumnBinding(
		UChooserTable* Table, int32 ColumnIndex, int32 ParameterIndex, const FString& PropertyPath = TEXT(""));

	/**
	 * Changes settings that apply to the whole column, such as MaxDistance on a Float Difference column
	 * or TagMatchType on a Gameplay Tag column. Names come from ListChooserColumnTypes, values are in
	 * Unreal text format.
	 * @param Table The table to modify.
	 * @param ColumnIndex Column to configure.
	 * @param Settings Setting values keyed by setting name.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Columns")
	static bool SetChooserColumnSettings(UChooserTable* Table, int32 ColumnIndex, const TMap<FString, FString>& Settings);

	/**
	 * Replaces the cell a new row starts from. A new column's template cell matches anything, so rows
	 * only gain a condition where a cell is filled in; set it to make new rows start constrained.
	 * @param Table The table to modify.
	 * @param ColumnIndex Column to configure.
	 * @param Value The cell, in the format DescribeChooserTable reports as the column's DefaultCell.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Columns")
	static bool SetChooserColumnDefaultCell(UChooserTable* Table, int32 ColumnIndex, const FString& Value);

	/**
	 * Removes a column and every cell in it.
	 * @param Table The table to modify.
	 * @param ColumnIndex Column to remove.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Columns")
	static bool RemoveChooserColumn(UChooserTable* Table, int32 ColumnIndex);

	/**
	 * Moves a column, taking its cells with it.
	 * @param Table The table to modify.
	 * @param ColumnIndex Column to move.
	 * @param TargetIndex Index to move it to, counted before the column is lifted out.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Columns")
	static bool MoveChooserColumn(UChooserTable* Table, int32 ColumnIndex, int32 TargetIndex);

	/**
	 * Copies a column, its binding, its settings and its cells into a new column directly to its right,
	 * and returns the new column's index.
	 * @param Table The table to modify.
	 * @param ColumnIndex Column to copy.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Columns")
	static int32 DuplicateChooserColumn(UChooserTable* Table, int32 ColumnIndex);

	/**
	 * Keeps a column in the asset but skips it during evaluation. Disabled columns are stripped when
	 * the project cooks.
	 * @param Table The table to modify.
	 * @param ColumnIndex Column to disable or re-enable.
	 * @param bDisabled True to skip the column, false to evaluate it again.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Columns")
	static bool SetChooserColumnDisabled(UChooserTable* Table, int32 ColumnIndex, bool bDisabled);

	/**
	 * Fills every cell of a column from the result of its row, for column types that know how to derive
	 * their value from the selected asset. Only a Float Difference column with an AutoPopulator set can
	 * do this; other columns raise.
	 * @param Table The table to modify.
	 * @param ColumnIndex Column to fill.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Columns")
	static bool AutoPopulateChooserColumn(UChooserTable* Table, int32 ColumnIndex);
};
