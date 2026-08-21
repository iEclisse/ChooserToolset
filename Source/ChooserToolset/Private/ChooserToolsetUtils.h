// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#pragma once

#include "ChooserToolsetTypes.h"
#include "CoreMinimal.h"

struct FChooserColumnBase;
struct FChooserPropertyBinding;
struct FInstancedStruct;
class UChooserTable;
class UPackage;

/**
 * Shared implementation helpers for the Chooser toolsets.
 *
 * The Chooser plugin keeps a table's rows, columns and cells in arrays of FInstancedStruct that no
 * scripting API reaches, and the editor keeps those arrays consistent by hand: cell arrays must be
 * resized with the row list, columns must be ordered filter, then output, then randomize, and a
 * column's binding only resolves after it has been compiled against the table's parameters.
 * Everything here works on those arrays directly while keeping that bookkeeping correct.
 */
namespace UE::ChooserToolset
{
	/** Row index that addresses a table's fallback row rather than a row of the row list. */
	constexpr int32 FallbackRowIndex = -2;

	/** Reports a failure to the caller of a tool. */
	void RaiseError(const FString& Message);

	/** Reports a failure, prefixed by the name of the tool that refused the call. */
	void FailTool(const TCHAR* Tool, const FString& Message);

	/**
	 * Checks the table argument every chooser tool takes and brings its cell arrays back in line with
	 * its rows. Fails the tool and returns false when there is no table.
	 */
	bool PrepareTable(const TCHAR* Tool, UChooserTable* Table);

	/** Creates a package for a new asset. Returns null and fills OutError when the path is unusable or taken. */
	UPackage* CreateAssetPackage(const FString& FolderPath, const FString& AssetName, FString& OutError);

	/** Registers a freshly created asset with the asset registry and marks its package dirty. */
	void FinalizeNewAsset(UObject* Asset);

	/** Marks the table modified and dirties its package. */
	void NotifyTableChanged(UChooserTable* Table);

	/** Loads chooser tables under a content folder. Empty searches the whole project. */
	TArray<UChooserTable*> FindChooserTables(const FString& FolderPath);

	/** Resizes DisabledRows and every column's cell array to match the row list. */
	void SyncRowArrays(UChooserTable* Table);

	/** True when RowIndex addresses an existing row or the fallback row. */
	bool IsValidRowIndex(const UChooserTable* Table, int32 RowIndex);

	/** Returns the column at ColumnIndex. Returns null and fills OutError when the index is out of range. */
	FInstancedStruct* GetColumnStruct(UChooserTable* Table, int32 ColumnIndex, FString& OutError);

	/** The table that owns the parameter list, which for a nested chooser is its root. */
	UChooserTable* GetParameterOwner(UChooserTable* Table);

	/** Class or struct of a table parameter. Returns null when the index is out of range or the parameter has no type. */
	const UStruct* GetParameterType(const UChooserTable* Table, int32 ParameterIndex);

	/** Instantiable structs deriving from FChooserColumnBase, excluding those hidden from the editor. */
	TArray<const UScriptStruct*> FindColumnTypes();

	/** Display name a struct shows in the chooser editor, falling back to its C++ name. */
	FString GetStructDisplayName(const UScriptStruct* Struct);

	/** Value of a struct's metadata key, empty when it is not set. */
	FString GetStructMetadata(const UScriptStruct* Struct, const FName& Key);

	/** The value kind a column binds to, derived from the parameter type its input accepts. */
	EChooserToolsetValueKind GetColumnValueKind(const FChooserColumnBase& Column);

	/** The value kind a property can be bound as. */
	EChooserToolsetValueKind GetPropertyValueKind(const FProperty* Property);

	/** Describes a column type for ListChooserColumnTypes. */
	FChooserToolsetColumnTypeInfo MakeColumnTypeInfo(const UScriptStruct* ColumnType);

	/** Describes a column of a table. */
	FChooserToolsetColumnInfo MakeColumnInfo(UChooserTable* Table, int32 ColumnIndex);

	/** Describes a row of a table, or its fallback row when RowIndex is FallbackRowIndex. */
	FChooserToolsetRowInfo MakeRowInfo(UChooserTable* Table, int32 RowIndex);

	/** The binding of a column's input, or null when the column has no input. */
	FChooserPropertyBinding* GetColumnBinding(FChooserColumnBase& Column);

	/**
	 * Points a column's input at a property of one of the table's parameters, creating the input when
	 * the column has none. An empty PropertyPath binds the parameter itself.
	 */
	bool BindColumn(UChooserTable* Table, int32 ColumnIndex, int32 ParameterIndex, const FString& PropertyPath, FString& OutError);

	/** Splits a dot separated path into properties of Root. Fills OutError when a segment does not exist. */
	bool ResolvePropertyPath(const UStruct* Root, const FString& PropertyPath, TArray<FName>& OutChain, FField*& OutLeaf, FString& OutError);

	/** Walks the properties of Root, adding every path a column can bind to. */
	void CollectBindableProperties(const UStruct* Root, int32 ParameterIndex, int32 MaxDepth, const FString& NameFilter, TArray<FChooserToolsetBindingInfo>& OutBindings);

	/** Makes a freshly created column's template cell match anything, so new rows gain no hidden filter. */
	void RelaxNewColumnDefaultCell(FInstancedStruct& Column);

	/** Locates the cell a new row starts from. Returns null and fills OutError when the column has none. */
	FProperty* GetDefaultCellMemory(UChooserTable* Table, int32 ColumnIndex, void*& OutValue, FString& OutError);

	/**
	 * Locates the memory holding a cell, and the property describing it. Row index FallbackRowIndex
	 * addresses the fallback cell, which only output columns have.
	 */
	FProperty* GetCellMemory(UChooserTable* Table, int32 ColumnIndex, int32 RowIndex, void*& OutValue, FString& OutError);

	/** The enum a column filters or writes, or null when the column is not enum based or has no binding. */
	const UEnum* GetColumnEnum(FChooserColumnBase& Column);

	/** Reads a cell as text. Returns false and fills OutError when the cell cannot be read. */
	bool GetCellText(UChooserTable* Table, int32 ColumnIndex, int32 RowIndex, FString& OutText, FString& OutError);

	/** Writes a cell from text in the same format GetCellText produces. */
	bool SetCellText(UChooserTable* Table, int32 ColumnIndex, int32 RowIndex, const FString& Text, FString& OutError);

	/** Names and current values of a struct's editable properties, as text. */
	TMap<FString, FString> GetStructSettings(const UScriptStruct* Struct, const void* Memory, const FName& RowValuesName);

	/** Applies text values to a struct's editable properties by name. */
	bool ApplyStructSettings(const UScriptStruct* Struct, void* Memory, const TMap<FString, FString>& Settings, FString& OutError);

	/**
	 * Editable property names of a column type, in declaration order, leaving out the ones with a tool
	 * of their own: the cell array named by RowValuesName, the input binding and the disabled flag.
	 */
	TArray<FString> GetSettingNames(const UScriptStruct* Struct, const FName& RowValuesName);

	/** Wraps Result in the FObjectChooserBase struct that suits it, e.g. an asset, a class or a chooser table. */
	bool MakeResultStruct(const UChooserTable* Table, UObject* Result, bool bSoftReference, FInstancedStruct& OutResult, FString& OutError);

	/** The object a row's result points at, or null when the result is empty or not a reference. */
	UObject* GetResultObject(const FInstancedStruct& Result);

	/** Compiles the table and collects everything its columns and results reported. */
	FChooserToolsetValidationResult ValidateTable(UChooserTable* Table);
}
