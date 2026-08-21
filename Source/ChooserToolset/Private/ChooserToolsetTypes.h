// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#pragma once

#include "ChooserPropertyAccess.h"
#include "CoreMinimal.h"
#include "IHasContext.h"

#include "ChooserToolsetTypes.generated.h"

class UChooserTable;

/** How a filter cell compares the value on its row against the value its column reads. */
UENUM()
enum class EChooserToolsetComparison : uint8
{
	/** The row passes when the input equals the cell value. */
	Equal,
	/** The row passes when the input differs from the cell value. */
	NotEqual,
	/** The cell is ignored and the row always passes this column. */
	Any
};

/**
 * The kind of value a column reads or writes. A column can only bind to a property of a matching kind,
 * so this is what pairs a column type with a bindable property.
 */
UENUM()
enum class EChooserToolsetValueKind : uint8
{
	/** bool. */
	Bool,
	/** float or double. */
	Float,
	/** Any UENUM, including byte enums. */
	Enum,
	/** FName, FString or FText. */
	Name,
	/** An object or class reference. */
	Object,
	/** A struct, written whole by an Output Struct column. */
	Struct,
	/** FGameplayTagContainer. */
	GameplayTags,
	/** FGameplayTagQuery. */
	GameplayTagQuery,
	/** FChooserRandomizationContext, used only by the Randomize column. */
	Randomization,
	/** A kind this toolset does not recognise, usually from a column type outside the Chooser plugin. */
	Unknown
};

/** An object or struct a chooser table reads its inputs from and writes its outputs to. */
USTRUCT(BlueprintType)
struct FChooserToolsetParameterInfo
{
	GENERATED_BODY()

	/** Position in the table's parameter list. Column bindings address parameters by this index. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	int32 Index = INDEX_NONE;

	/** Class of an object parameter, null for a struct parameter. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TObjectPtr<UClass> Class;

	/** Struct type of a struct parameter, null for an object parameter. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TObjectPtr<UScriptStruct> Struct;

	/** Whether the table reads this parameter, writes it, or both. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	EContextObjectDirection Direction = EContextObjectDirection::Read;
};

/** A property of a chooser parameter that a column can bind to. */
USTRUCT(BlueprintType)
struct FChooserToolsetBindingInfo
{
	GENERATED_BODY()

	/** Index of the parameter this property belongs to. Pass it as ParameterIndex when binding a column. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	int32 ParameterIndex = INDEX_NONE;

	/** Dot separated path to pass as PropertyPath, e.g. "CharacterMovement.MaxWalkSpeed". */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	FString PropertyPath;

	/** The property's C++ type, e.g. "double" or "FGameplayTagContainer". */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	FString TypeName;

	/** Which column types can bind to this property. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	EChooserToolsetValueKind ValueKind = EChooserToolsetValueKind::Unknown;

	/** True when the path ends in a getter function rather than a data member, so it can be read but not written. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	bool bIsFunction = false;
};

/** A column type that can be added to a chooser table. */
USTRUCT(BlueprintType)
struct FChooserToolsetColumnTypeInfo
{
	GENERATED_BODY()

	/** Pass this back as ColumnType to AddChooserColumn. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TObjectPtr<UScriptStruct> ColumnType;

	/** Name shown in the chooser editor's add-column menu, e.g. "Float Range". */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	FString DisplayName;

	/** Menu category the column is filed under: Filter, Scoring, Output or Random. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	FString Category;

	/** What the column does. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	FString Description;

	/** The kind of property this column binds to. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	EChooserToolsetValueKind ValueKind = EChooserToolsetValueKind::Unknown;

	/** True when the column writes to its bound property instead of rejecting rows. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	bool bHasOutputs = false;

	/** True when the column rejects rows whose cell does not match the input. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	bool bHasFilters = false;

	/** True when the column ranks the surviving rows by cost rather than rejecting them. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	bool bHasCosts = false;

	/** Names of the column-wide settings SetChooserColumnSettings accepts. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TArray<FString> SettingNames;

	/**
	 * Shape of a cell of this column in the text format SetChooserCell accepts: field names for a
	 * struct cell, the accepted names for an enum cell, e.g. "(Min=float,Max=float)".
	 */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	FString CellFormat;
};

/** A column of a chooser table. */
USTRUCT(BlueprintType)
struct FChooserToolsetColumnInfo
{
	GENERATED_BODY()

	/** Position in the table's column list. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	int32 Index = INDEX_NONE;

	/** The column's type. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TObjectPtr<UScriptStruct> ColumnType;

	/** Name shown in the chooser editor, e.g. "Float Range". */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	FString DisplayName;

	/** Parameter the column reads or writes, or INDEX_NONE when the column has no binding yet. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	int32 ParameterIndex = INDEX_NONE;

	/** Property path the column is bound to, empty when the column has no binding yet. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	FString PropertyPath;

	/** True when the column is kept in the asset but skipped during evaluation. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	bool bDisabled = false;

	/** True when the column writes to its bound property instead of rejecting rows. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	bool bHasOutputs = false;

	/** Cell a new row starts from, in the text format SetChooserColumnDefaultCell accepts. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	FString DefaultCell;

	/** The column's own settings, e.g. "MaxDistance" on a Float Difference column. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TMap<FString, FString> Settings;

	/** What the last compile said about this column's binding. Empty when the binding resolves. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	FString CompileError;
};

/** A row of a chooser table. */
USTRUCT(BlueprintType)
struct FChooserToolsetRowInfo
{
	GENERATED_BODY()

	/** Position in the table's row list, or -2 for the fallback row. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	int32 Index = INDEX_NONE;

	/** What the row selects: an asset, a class, or another chooser table. Null on an output-only row. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TObjectPtr<UObject> Result;

	/** How the result is stored, e.g. "Asset", "Asset (Soft Reference)", "Class" or "Evaluate Chooser". */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	FString ResultType;

	/** True when the row is kept in the asset but skipped during evaluation. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	bool bDisabled = false;

	/**
	 * Cells of this row, one per column in column order, in the text format SetChooserCell accepts.
	 * An entry is empty when the column has no cell for this row.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TArray<FString> Cells;
};

/** A chooser table with its signature, columns and rows. */
USTRUCT(BlueprintType)
struct FChooserToolsetTableInfo
{
	GENERATED_BODY()

	/** The table this describes. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TObjectPtr<UChooserTable> Table;

	/** Whether the table returns an object, a class, or nothing but output values. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	EObjectChooserResultType ResultType = EObjectChooserResultType::ObjectResult;

	/** Class every result must be, or must derive from when ResultType is ClassResult. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TObjectPtr<UClass> ResultClass;

	/** Parameters the table reads and writes. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TArray<FChooserToolsetParameterInfo> Parameters;

	/** Columns in evaluation order. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TArray<FChooserToolsetColumnInfo> Columns;

	/** Rows in evaluation order. The first row passing every filter wins. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TArray<FChooserToolsetRowInfo> Rows;

	/**
	 * Row used when no other row passes. Its Result is null when the table has no fallback, and only its
	 * output column cells hold anything, since it passes no filters.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	FChooserToolsetRowInfo FallbackRow;

	/** Tables stored inside this asset, referenced by Nested Chooser rows. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TArray<TObjectPtr<UChooserTable>> NestedChoosers;
};

/** What compiling a chooser table found. */
USTRUCT(BlueprintType)
struct FChooserToolsetValidationResult
{
	GENERATED_BODY()

	/** True when no errors were found. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	bool bSuccess = false;

	/** Problems that stop the table from evaluating. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TArray<FString> Errors;

	/** Problems that leave the table working but are likely mistakes. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TArray<FString> Warnings;
};

/** One parameter value handed to a test evaluation. */
USTRUCT(BlueprintType)
struct FChooserToolsetEvaluationParameter
{
	GENERATED_BODY()

	/** Index of the parameter in the table's parameter list. */
	UPROPERTY(BlueprintReadWrite, Category = "ChooserToolset")
	int32 ParameterIndex = 0;

	/** Object to evaluate against, for an object parameter. Ignored for a struct parameter. */
	UPROPERTY(BlueprintReadWrite, Category = "ChooserToolset")
	TObjectPtr<UObject> Object;

	/**
	 * Field values for a struct parameter, keyed by field name, in Unreal text format.
	 * Fields left out keep the struct's default. Ignored for an object parameter.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "ChooserToolset")
	TMap<FString, FString> Fields;
};

/** What a test evaluation produced. */
USTRUCT(BlueprintType)
struct FChooserToolsetEvaluationResult
{
	GENERATED_BODY()

	/** Objects the table selected, in selection order. Empty when nothing matched and there is no fallback. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TArray<TObjectPtr<UObject>> Results;

	/** Rows that produced those results, in the same order. */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TArray<int32> SelectedRows;

	/**
	 * Values the output columns wrote, keyed by "ParameterIndex.PropertyPath", in Unreal text format.
	 * Only parameters the table is allowed to write appear here.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "ChooserToolset")
	TMap<FString, FString> Outputs;
};
