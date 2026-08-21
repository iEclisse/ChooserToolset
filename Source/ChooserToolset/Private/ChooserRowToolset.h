// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "ChooserRowToolset.generated.h"

class UChooserTable;

/**
 * Manages the rows of a chooser table and the result each one selects.
 *
 * Row order is evaluation order: the first row that survives every filter column wins. When no row
 * survives, the table falls back to its fallback row, addressed here as row index -2.
 *
 * A result is whatever object the row picks. Pass an asset for a table returning objects, a class for
 * a table returning classes, and another chooser table to hand the decision to it: a table nested in
 * this asset becomes a Nested Chooser, and a separate chooser asset becomes an Evaluate Chooser.
 * A Lookup Proxy result, which resolves a proxy asset through a proxy table, is the one kind of result
 * this toolset does not author: it also needs a proxy table binding, which only the chooser editor sets.
 */
UCLASS(BlueprintType)
class UChooserRowToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Adds a row and returns its index. Its cells start at each column's default value.
	 * @param Table The table to modify.
	 * @param Result Object the row selects. Leave it out on a table whose result type is NoPrimaryResult,
	 *               where rows exist only to drive the output columns.
	 * @param bSoftReference True to store an asset as a soft reference, which keeps it unloaded until the
	 *                       row is chosen. Ignored for classes and chooser tables.
	 * @param InsertAt Row index to insert before. Negative or past the end appends.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Rows")
	static int32 AddChooserRow(UChooserTable* Table, UObject* Result = nullptr, bool bSoftReference = false, int32 InsertAt = -1);

	/**
	 * Replaces what a row selects.
	 * @param Table The table to modify.
	 * @param RowIndex Row to change, or -2 for the fallback row, which creates it when it does not exist.
	 * @param Result Object the row selects. Leave it out on a table whose result type is NoPrimaryResult.
	 * @param bSoftReference True to store an asset as a soft reference.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Rows")
	static bool SetChooserRowResult(UChooserTable* Table, int32 RowIndex, UObject* Result = nullptr, bool bSoftReference = false);

	/**
	 * Removes a row and its cells.
	 * @param Table The table to modify.
	 * @param RowIndex Row to remove, or -2 to remove the fallback row.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Rows")
	static bool RemoveChooserRow(UChooserTable* Table, int32 RowIndex);

	/**
	 * Moves a row, taking its cells with it. Use this to change which of two matching rows wins.
	 * @param Table The table to modify.
	 * @param RowIndex Row to move.
	 * @param TargetIndex Index to move it to, counted before the row is lifted out.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Rows")
	static bool MoveChooserRow(UChooserTable* Table, int32 RowIndex, int32 TargetIndex);

	/**
	 * Copies a row and its cells into a new row directly below it, and returns the new row's index.
	 * @param Table The table to modify.
	 * @param RowIndex Row to copy.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Rows")
	static int32 DuplicateChooserRow(UChooserTable* Table, int32 RowIndex);

	/**
	 * Keeps a row in the asset but skips it during evaluation. Disabled rows are stripped when the
	 * project cooks.
	 * @param Table The table to modify.
	 * @param RowIndex Row to disable or re-enable.
	 * @param bDisabled True to skip the row, false to evaluate it again.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Rows")
	static bool SetChooserRowDisabled(UChooserTable* Table, int32 RowIndex, bool bDisabled);
};
