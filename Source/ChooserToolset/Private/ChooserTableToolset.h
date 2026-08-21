// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#pragma once

#include "ChooserToolsetTypes.h"
#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "ChooserTableToolset.generated.h"

class UAnimInstance;
class UChooserTable;

/**
 * Creates and inspects Chooser Table assets, and owns everything table-wide: the signature (what the
 * table returns and which parameters it reads and writes), the tables nested inside it, and compiling.
 *
 * A chooser table is a decision table. Its rows each hold a result; its columns each read one property
 * of one parameter and reject, score or write to the rows. Evaluating it walks the rows in order and
 * returns the first result whose row survives every filter column, or the fallback row when none does.
 *
 * Authoring order that avoids rework:
 *   1. CreateChooserTable with the class the table returns.
 *   2. AddChooserObjectParameter / AddChooserStructParameter for everything the table reads or writes.
 *      Columns can only bind to properties of these, so parameters come before columns.
 *   3. ChooserColumnToolset.AddChooserColumn for each condition, ChooserRowToolset.AddChooserRow for
 *      each result, then ChooserCellToolset to fill the cells.
 *   4. ValidateChooserTable, then save the asset with AssetTools.save_assets.
 */
UCLASS(BlueprintType)
class UChooserTableToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Lists the chooser tables in the project.
	 * @param FolderPath Content folder to search recursively, e.g. "/Game/Choosers". Empty searches the whole project.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Discovery")
	static TArray<UChooserTable*> ListChooserTables(const FString& FolderPath = TEXT(""));

	/**
	 * Describes a table in full: its signature, parameters, columns, rows with their cell values,
	 * fallback row and nested tables. Call this before editing a table you did not just create.
	 * @param Table The table to inspect.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Discovery")
	static FChooserToolsetTableInfo DescribeChooserTable(UChooserTable* Table);

	/**
	 * Creates a chooser table asset.
	 * @param FolderPath Content folder for the new asset, e.g. "/Game/Choosers".
	 * @param AssetName Asset name, e.g. "CT_AttackAnimations".
	 * @param ResultClass Class every result must be, or must derive from when ResultType is ClassResult.
	 *                    Leave it out when ResultType is NoPrimaryResult.
	 * @param ResultType Whether rows hold an object, a class, or nothing but values for output columns.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Creation")
	static UChooserTable* CreateChooserTable(
		const FString& FolderPath,
		const FString& AssetName,
		UClass* ResultClass = nullptr,
		EObjectChooserResultType ResultType = EObjectChooserResultType::ObjectResult);

	/**
	 * Creates a chooser table set up for the Chooser Player animation node: it returns an animation
	 * asset and takes the anim instance plus a Chooser Player Settings struct as parameters.
	 * @param FolderPath Content folder for the new asset.
	 * @param AssetName Asset name, e.g. "CT_Locomotion".
	 * @param AnimInstanceClass Anim instance the table reads from. Leave it out for the base AnimInstance.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Creation")
	static UChooserTable* CreateAnimationChooserTable(
		const FString& FolderPath, const FString& AssetName, TSubclassOf<UAnimInstance> AnimInstanceClass = nullptr);

	/**
	 * Changes what a table returns. Existing rows whose result no longer fits the new class are
	 * reported by ValidateChooserTable.
	 * @param Table The table to modify.
	 * @param ResultClass Class every result must be, or must derive from when ResultType is ClassResult.
	 * @param ResultType Whether rows hold an object, a class, or nothing but values for output columns.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Signature")
	static bool SetChooserResultType(
		UChooserTable* Table, UClass* ResultClass = nullptr, EObjectChooserResultType ResultType = EObjectChooserResultType::ObjectResult);

	/**
	 * Adds an object the table reads properties from, and returns its parameter index.
	 * @param Table The table to modify.
	 * @param ObjectClass Class of the object that will be passed in at evaluation time.
	 * @param Direction Whether the table reads this object, writes to it, or both.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Signature")
	static int32 AddChooserObjectParameter(
		UChooserTable* Table, UClass* ObjectClass, EContextObjectDirection Direction = EContextObjectDirection::Read);

	/**
	 * Adds a struct the table reads properties from, and returns its parameter index.
	 * @param Table The table to modify.
	 * @param Struct Struct type that will be passed in at evaluation time.
	 * @param Direction Whether the table reads this struct, writes to it, or both.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Signature")
	static int32 AddChooserStructParameter(
		UChooserTable* Table, UScriptStruct* Struct, EContextObjectDirection Direction = EContextObjectDirection::Read);

	/**
	 * Removes a parameter. Columns bound to it stop resolving, so rebind them afterwards.
	 * @param Table The table to modify.
	 * @param ParameterIndex Index of the parameter to remove.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Signature")
	static bool RemoveChooserParameter(UChooserTable* Table, int32 ParameterIndex);

	/**
	 * Sets whether the table reads a parameter, writes to it, or both. Output columns can only write
	 * to a parameter marked Write or ReadWrite.
	 * @param Table The table to modify.
	 * @param ParameterIndex Index of the parameter.
	 * @param Direction The new direction.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Signature")
	static bool SetChooserParameterDirection(UChooserTable* Table, int32 ParameterIndex, EContextObjectDirection Direction);

	/**
	 * Lists the properties of a parameter that a column can bind to, with the kind of column each suits.
	 * @param Table The table whose parameters to search.
	 * @param ParameterIndex Index of the parameter, or -1 to search every parameter.
	 * @param NameFilter Case-insensitive substring the property path must contain. Empty returns all.
	 * @param MaxDepth How far to follow nested structs and object references. 1 lists direct members only.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Signature")
	static TArray<FChooserToolsetBindingInfo> ListBindableProperties(
		UChooserTable* Table, int32 ParameterIndex = -1, const FString& NameFilter = TEXT(""), int32 MaxDepth = 2);

	/**
	 * Creates a table stored inside another table, for use as the result of a Nested Chooser row.
	 * The nested table shares its parent's parameters and has its own rows and columns.
	 * @param Table The table that will own the nested table.
	 * @param Name Name for the nested table, e.g. "Crouched".
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Nesting")
	static UChooserTable* CreateNestedChooser(UChooserTable* Table, const FString& Name);

	/**
	 * Deletes a nested table and clears the rows that pointed at it.
	 * @param Table The table that owns the nested table.
	 * @param NestedChooser The nested table to delete.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Nesting")
	static bool DeleteNestedChooser(UChooserTable* Table, UChooserTable* NestedChooser);

	/**
	 * Compiles the table and reports what is wrong with it: bindings that no longer resolve, rows with
	 * no result, and results that do not match the table's result class. Run this after editing.
	 * @param Table The table to compile.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Maintenance")
	static FChooserToolsetValidationResult ValidateChooserTable(UChooserTable* Table);

	/**
	 * Renames a property inside every binding of the table, for when a bound property was renamed on
	 * its parameter. Returns how many bindings changed.
	 * @param Table The table to modify.
	 * @param FindName Property name to look for in binding paths.
	 * @param ReplaceName Name to put in its place.
	 * @param bMatchWholeWord True to only replace a path segment that equals FindName exactly.
	 * @param bMatchCase True to compare case-sensitively.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Maintenance")
	static int32 ReplaceChooserBindingNames(
		UChooserTable* Table,
		const FString& FindName,
		const FString& ReplaceName,
		bool bMatchWholeWord = true,
		bool bMatchCase = true);

	/**
	 * Permanently deletes every disabled row and column of the table. Disabled data is stripped when
	 * the project cooks, so this makes the editor match what ships.
	 * @param Table The table to clean up.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Maintenance")
	static bool RemoveDisabledChooserData(UChooserTable* Table);

	/**
	 * Opens the chooser editor window for a table.
	 * @param Table The table to open.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Maintenance")
	static bool OpenChooserEditor(UChooserTable* Table);

	/**
	 * Closes the chooser editor window of a table, if one is open.
	 * @param Table The table whose editor should close.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Maintenance")
	static bool CloseChooserEditor(UChooserTable* Table);
};
