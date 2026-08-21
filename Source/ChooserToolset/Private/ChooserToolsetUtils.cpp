// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#include "ChooserToolsetUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Chooser.h"
#include "ChooserPropertyAccess.h"
#include "GameplayTagContainer.h"
#include "IChooserColumn.h"
#include "IChooserParameterBool.h"
#include "IChooserParameterEnum.h"
#include "IChooserParameterFloat.h"
#include "IChooserParameterGameplayTag.h"
#include "IChooserParameterObject.h"
#include "IChooserParameterRandomize.h"
#include "IChooserParameterStruct.h"
#include "ChooserParameterName.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"
#include "Misc/StringOutputDevice.h"
#include "ObjectChooser_Asset.h"
#include "ObjectChooser_Class.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

namespace UE::ChooserToolset
{

namespace Private
{
	const FName DisplayNameKey(TEXT("DisplayName"));
	const FName CategoryKey(TEXT("Category"));
	const FName TooltipKey(TEXT("Tooltip"));
	const FName ToolTipKey(TEXT("ToolTip"));
	const FName HiddenKey(TEXT("Hidden"));
	const FName FallbackValueName(TEXT("FallbackValue"));
	const FName BoolFallbackValueName(TEXT("bFallbackValue"));

	/** True for properties that are implementation detail rather than authored data. */
	bool IsHiddenProperty(const FProperty* Property)
	{
		if (!Property)
		{
			return true;
		}
		if (Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient))
		{
			return true;
		}
		return Property->GetName().EndsWith(TEXT("_DEPRECATED"));
	}

	/** Case-insensitive property lookup, so callers do not have to match C++ casing exactly. */
	FProperty* FindProperty(const UStruct* Struct, const FString& Name)
	{
		if (!Struct)
		{
			return nullptr;
		}
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (It->GetName().Equals(Name, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** The value a function returns, or null when it returns nothing or needs arguments. */
	FProperty* GetFunctionReturnProperty(const UFunction* Function)
	{
		if (!Function || Function->NumParms != 1)
		{
			return nullptr;
		}
		return Function->GetReturnProperty();
	}

	/** Case-insensitive function lookup on a class. */
	UFunction* FindFunction(const UStruct* Struct, const FString& Name)
	{
		const UClass* Class = Cast<const UClass>(Struct);
		if (!Class)
		{
			return nullptr;
		}
		for (TFieldIterator<UFunction> It(Class); It; ++It)
		{
			if (It->GetName().Equals(Name, ESearchCase::IgnoreCase) && GetFunctionReturnProperty(*It))
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** The struct or class a bound property descends into, or null when the path must stop here. */
	const UStruct* GetInnerType(const FProperty* Property)
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			return StructProperty->Struct;
		}
		if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			return ObjectProperty->PropertyClass;
		}
		return nullptr;
	}

	/** The concrete parameter struct a column's input should use, given the base type the column accepts. */
	const UScriptStruct* FindDefaultInputType(const UScriptStruct* BaseType)
	{
		if (!BaseType)
		{
			return nullptr;
		}

		const UScriptStruct* Fallback = nullptr;
		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			UScriptStruct* Candidate = *It;
			if (Candidate == BaseType || !Candidate->IsChildOf(BaseType))
			{
				continue;
			}

			bool bHasBinding = false;
			for (TFieldIterator<FStructProperty> PropertyIt(Candidate); PropertyIt; ++PropertyIt)
			{
				if (PropertyIt->Struct && PropertyIt->Struct->IsChildOf(FChooserPropertyBinding::StaticStruct()))
				{
					bHasBinding = true;
					break;
				}
			}
			if (!bHasBinding)
			{
				continue;
			}

			// Every Chooser plugin parameter that binds a property is named "<Type>ContextProperty";
			// anything else is a third party parameter and only used when nothing else fits.
			if (Candidate->GetName().EndsWith(TEXT("ContextProperty")))
			{
				return Candidate;
			}
			if (!Fallback)
			{
				Fallback = Candidate;
			}
		}
		return Fallback;
	}

	/** Locates the memory of a cell, and the property describing it. */
	FProperty* GetCellProperty(FChooserColumnBase& Column, const UScriptStruct* ColumnType, void* ColumnMemory,
		int32 RowIndex, void*& OutValue, FString& OutError)
	{
		OutValue = nullptr;

		if (RowIndex == FallbackRowIndex)
		{
			if (!Column.HasOutputs())
			{
				OutError = TEXT("only output columns have a fallback cell, because the fallback row passes no filters.");
				return nullptr;
			}

			FProperty* Property = FindFProperty<FProperty>(ColumnType, FallbackValueName);
			if (!Property)
			{
				Property = FindFProperty<FProperty>(ColumnType, BoolFallbackValueName);
			}
			if (!Property)
			{
				OutError = FString::Printf(TEXT("column type '%s' has no fallback value."), *ColumnType->GetName());
				return nullptr;
			}
			OutValue = Property->ContainerPtrToValuePtr<void>(ColumnMemory);
			return Property;
		}

		const FName RowValuesName = Column.RowValuesPropertyName();
		if (RowValuesName.IsNone())
		{
			OutError = FString::Printf(TEXT("column type '%s' stores no per-row values."), *ColumnType->GetName());
			return nullptr;
		}

		FArrayProperty* ArrayProperty = FindFProperty<FArrayProperty>(ColumnType, RowValuesName);
		if (!ArrayProperty)
		{
			OutError = FString::Printf(TEXT("column type '%s' declares row values in '%s', which does not exist."),
				*ColumnType->GetName(), *RowValuesName.ToString());
			return nullptr;
		}

		FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(ColumnMemory));
		if (!ArrayHelper.IsValidIndex(RowIndex))
		{
			OutError = FString::Printf(TEXT("row %d has no cell in this column."), RowIndex);
			return nullptr;
		}

		OutValue = ArrayHelper.GetElementPtr(RowIndex);
		return ArrayProperty->Inner;
	}

	/** The values an enum property accepts, restricted to the ones its column offers. */
	FString DescribeEnumValues(const FProperty* Property, const UEnum* Enum)
	{
		static const FName ValidEnumValuesKey(TEXT("ValidEnumValues"));
		if (Property->HasMetaData(ValidEnumValuesKey))
		{
			TArray<FString> Allowed;
			Property->GetMetaData(ValidEnumValuesKey).ParseIntoArray(Allowed, TEXT(","));
			for (FString& Value : Allowed)
			{
				Value.TrimStartAndEndInline();
			}
			return FString::Join(Allowed, TEXT("|"));
		}

		TArray<FString> Names;
		for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
		{
			Names.Add(Enum->GetNameStringByIndex(Index));
		}
		return FString::Join(Names, TEXT("|"));
	}

	/**
	 * The shape of a value in the text format ImportText accepts: field names for a struct, the
	 * accepted names for an enum, and the C++ type for anything else.
	 */
	FString DescribeValueFormat(const FProperty* Property, int32 Depth)
	{
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			return DescribeEnumValues(Property, EnumProperty->GetEnum());
		}
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			if (ByteProperty->Enum)
			{
				return DescribeEnumValues(Property, ByteProperty->Enum);
			}
		}
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct == FInstancedStruct::StaticStruct())
			{
				// An instanced struct carries its type in the text, ahead of the field list.
				return TEXT("/Script/Module.StructName(Field=Value,...)");
			}

			// Only authored fields are worth naming; a struct with none of its own keeps its type name,
			// which is what a struct with a hand-written text format (a tag container, a tag query) has.
			TArray<FString> Fields;
			if (Depth > 0)
			{
				for (TFieldIterator<FProperty> It(StructProperty->Struct); It; ++It)
				{
					if (IsHiddenProperty(*It) || !It->HasAnyPropertyFlags(CPF_Edit))
					{
						continue;
					}
					Fields.Add(FString::Printf(TEXT("%s=%s"), *It->GetName(), *DescribeValueFormat(*It, Depth - 1)));
				}
			}
			if (Fields.IsEmpty())
			{
				return StructProperty->Struct->GetName();
			}
			return TEXT("(") + FString::Join(Fields, TEXT(",")) + TEXT(")");
		}

		FString ExtendedType;
		const FString BaseType = Property->GetCPPType(&ExtendedType);
		return BaseType + ExtendedType;
	}

	/** The enum a property holds, or null when it is not an enum property. */
	const UEnum* GetPropertyEnum(const FProperty* Property)
	{
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			return EnumProperty->GetEnum();
		}
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			return ByteProperty->Enum;
		}
		return nullptr;
	}

	/** Writes an integer into an enum property, whichever of the two forms it takes. */
	void SetEnumPropertyValue(const FProperty* Property, void* Value, int64 EnumValue)
	{
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(Value, EnumValue);
		}
		else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			ByteProperty->SetIntPropertyValue(Value, EnumValue);
		}
	}

	/**
	 * Relaxes the cell a new row starts with so it matches anything: comparisons become Any and ranges
	 * become unbounded. Column types differ on this out of the box - a new Bool cell already matches
	 * anything while a new Enum cell means "equals the first value" - and a filter nobody asked for is
	 * far harder to notice than a missing one.
	 */
	void RelaxDefaultRowValue(const UScriptStruct* ColumnType, void* ColumnMemory)
	{
		FStructProperty* DefaultProperty = FindFProperty<FStructProperty>(ColumnType, TEXT("DefaultRowValue"));
		if (!DefaultProperty)
		{
			return;
		}
		void* DefaultCell = DefaultProperty->ContainerPtrToValuePtr<void>(ColumnMemory);

		if (const FProperty* Comparison = FindFProperty<FProperty>(DefaultProperty->Struct, TEXT("Comparison")))
		{
			if (const UEnum* Enum = GetPropertyEnum(Comparison))
			{
				int64 AnyValue = Enum->GetValueByNameString(Enum->GetName() + TEXT("::MatchAny"));
				if (AnyValue == INDEX_NONE)
				{
					AnyValue = Enum->GetValueByNameString(Enum->GetName() + TEXT("::Any"));
				}
				if (AnyValue != INDEX_NONE)
				{
					SetEnumPropertyValue(Comparison, Comparison->ContainerPtrToValuePtr<void>(DefaultCell), AnyValue);
				}
			}
		}

		for (const TCHAR* Unbounded : { TEXT("bNoMin"), TEXT("bNoMax") })
		{
			if (FBoolProperty* Bound = FindFProperty<FBoolProperty>(DefaultProperty->Struct, Unbounded))
			{
				Bound->SetPropertyValue(Bound->ContainerPtrToValuePtr<void>(DefaultCell), true);
			}
		}
	}

	/** Collects errors from a value that knows how to compile itself. */
	void AppendCompileError(const FText& Message, const FString& Context, TArray<FString>& OutErrors)
	{
		if (!Message.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: %s"), *Context, *Message.ToString()));
		}
	}
}

void RaiseError(const FString& Message)
{
	UKismetSystemLibrary::RaiseScriptError(Message);
}

void FailTool(const TCHAR* Tool, const FString& Message)
{
	RaiseError(FString::Printf(TEXT("%s: %s"), Tool, *Message));
}

bool PrepareTable(const TCHAR* Tool, UChooserTable* Table)
{
	if (!Table)
	{
		FailTool(Tool, TEXT("Table is required."));
		return false;
	}
	SyncRowArrays(Table);
	return true;
}

UPackage* CreateAssetPackage(const FString& FolderPath, const FString& AssetName, FString& OutError)
{
	if (AssetName.IsEmpty())
	{
		OutError = TEXT("AssetName is required.");
		return nullptr;
	}

	FString Folder = FolderPath;
	if (!Folder.StartsWith(TEXT("/")))
	{
		Folder = TEXT("/") + Folder;
	}
	Folder.RemoveFromEnd(TEXT("/"));
	if (Folder.IsEmpty() || Folder == TEXT("/"))
	{
		OutError = TEXT("FolderPath is required, for example \"/Game/Choosers\".");
		return nullptr;
	}

	const FString PackageName = Folder / AssetName;

	FText FailureReason;
	if (!FPackageName::IsValidLongPackageName(PackageName, false, &FailureReason))
	{
		OutError = FString::Printf(TEXT("'%s' is not a valid asset path: %s"), *PackageName, *FailureReason.ToString());
		return nullptr;
	}

	// Only ask the file system about paths that resolve to one, so an unmounted root does not warn.
	FString Filename;
	const bool bIsMounted = FPackageName::TryConvertLongPackageNameToFilename(
		PackageName, Filename, FPackageName::GetAssetPackageExtension());
	if ((bIsMounted && FPackageName::DoesPackageExist(PackageName)) || FindPackage(nullptr, *PackageName))
	{
		OutError = FString::Printf(TEXT("An asset already exists at '%s'."), *PackageName);
		return nullptr;
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		OutError = FString::Printf(TEXT("Failed to create a package at '%s'."), *PackageName);
		return nullptr;
	}
	Package->FullyLoad();
	return Package;
}

void FinalizeNewAsset(UObject* Asset)
{
	if (!Asset)
	{
		return;
	}
	FAssetRegistryModule::AssetCreated(Asset);
	Asset->MarkPackageDirty();
}

void NotifyTableChanged(UChooserTable* Table)
{
	if (!Table)
	{
		return;
	}
	Table->Modify();
	Table->MarkPackageDirty();
}

TArray<UChooserTable*> FindChooserTables(const FString& FolderPath)
{
	TArray<UChooserTable*> Result;

	const FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	FARFilter Filter;
	Filter.ClassPaths.Add(UChooserTable::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	if (!FolderPath.IsEmpty())
	{
		FString Folder = FolderPath;
		if (!Folder.StartsWith(TEXT("/")))
		{
			Folder = TEXT("/") + Folder;
		}
		Folder.RemoveFromEnd(TEXT("/"));
		Filter.PackagePaths.Add(FName(*Folder));
		Filter.bRecursivePaths = true;
	}

	TArray<FAssetData> Assets;
	AssetRegistryModule.Get().GetAssets(Filter, Assets);
	Result.Reserve(Assets.Num());
	for (const FAssetData& Asset : Assets)
	{
		if (UChooserTable* Table = Cast<UChooserTable>(Asset.GetAsset()))
		{
			Result.Add(Table);
		}
	}
	return Result;
}

void SyncRowArrays(UChooserTable* Table)
{
	if (!Table)
	{
		return;
	}

	const int32 RowCount = Table->ResultsStructs.Num();
	if (Table->DisabledRows.Num() != RowCount)
	{
		Table->DisabledRows.SetNum(RowCount);
	}

	for (FInstancedStruct& ColumnData : Table->ColumnsStructs)
	{
		if (ColumnData.IsValid())
		{
			ColumnData.GetMutable<FChooserColumnBase>().SetNumRows(RowCount);
		}
	}
}

bool IsValidRowIndex(const UChooserTable* Table, int32 RowIndex)
{
	if (!Table)
	{
		return false;
	}
	return RowIndex == FallbackRowIndex || Table->ResultsStructs.IsValidIndex(RowIndex);
}

FInstancedStruct* GetColumnStruct(UChooserTable* Table, int32 ColumnIndex, FString& OutError)
{
	if (!Table)
	{
		OutError = TEXT("Table is required.");
		return nullptr;
	}
	if (!Table->ColumnsStructs.IsValidIndex(ColumnIndex))
	{
		OutError = FString::Printf(TEXT("column %d does not exist, the table has %d columns."),
			ColumnIndex, Table->ColumnsStructs.Num());
		return nullptr;
	}
	FInstancedStruct& Column = Table->ColumnsStructs[ColumnIndex];
	if (!Column.IsValid())
	{
		OutError = FString::Printf(TEXT("column %d is empty."), ColumnIndex);
		return nullptr;
	}
	return &Column;
}

UChooserTable* GetParameterOwner(UChooserTable* Table)
{
	return Table ? Table->GetContextOwner() : nullptr;
}

const UStruct* GetParameterType(const UChooserTable* Table, int32 ParameterIndex)
{
	if (!Table)
	{
		return nullptr;
	}

	const TConstArrayView<FInstancedStruct> ContextData = Table->GetContextData();
	if (!ContextData.IsValidIndex(ParameterIndex))
	{
		return nullptr;
	}

	if (const FContextObjectTypeClass* ClassParameter = ContextData[ParameterIndex].GetPtr<FContextObjectTypeClass>())
	{
		return ClassParameter->Class;
	}
	if (const FContextObjectTypeStruct* StructParameter = ContextData[ParameterIndex].GetPtr<FContextObjectTypeStruct>())
	{
		return StructParameter->Struct;
	}
	return nullptr;
}

TArray<const UScriptStruct*> FindColumnTypes()
{
	TArray<const UScriptStruct*> Result;
	const UScriptStruct* BaseType = FChooserColumnBase::StaticStruct();

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		const UScriptStruct* Candidate = *It;
		if (Candidate == BaseType || !Candidate->IsChildOf(BaseType))
		{
			continue;
		}
		if (Candidate->HasMetaData(Private::HiddenKey))
		{
			continue;
		}
		Result.Add(Candidate);
	}

	Result.Sort([](const UScriptStruct& Left, const UScriptStruct& Right)
		{
			return GetStructDisplayName(&Left) < GetStructDisplayName(&Right);
		});
	return Result;
}

FString GetStructDisplayName(const UScriptStruct* Struct)
{
	if (!Struct)
	{
		return FString();
	}
	const FString DisplayName = GetStructMetadata(Struct, Private::DisplayNameKey);
	return DisplayName.IsEmpty() ? Struct->GetName() : DisplayName;
}

FString GetStructMetadata(const UScriptStruct* Struct, const FName& Key)
{
	if (!Struct || !Struct->HasMetaData(Key))
	{
		return FString();
	}
	return Struct->GetMetaData(Key);
}

EChooserToolsetValueKind GetColumnValueKind(const FChooserColumnBase& Column)
{
	const UScriptStruct* BaseType = Column.GetInputBaseType();
	if (!BaseType)
	{
		return EChooserToolsetValueKind::Unknown;
	}
	if (BaseType == FChooserParameterBoolBase::StaticStruct())
	{
		return EChooserToolsetValueKind::Bool;
	}
	if (BaseType == FChooserParameterFloatBase::StaticStruct())
	{
		return EChooserToolsetValueKind::Float;
	}
	if (BaseType == FChooserParameterEnumBase::StaticStruct())
	{
		return EChooserToolsetValueKind::Enum;
	}
	if (BaseType == FChooserParameterNameBase::StaticStruct())
	{
		return EChooserToolsetValueKind::Name;
	}
	if (BaseType == FChooserParameterObjectBase::StaticStruct())
	{
		return EChooserToolsetValueKind::Object;
	}
	if (BaseType == FChooserParameterStructBase::StaticStruct())
	{
		return EChooserToolsetValueKind::Struct;
	}
	if (BaseType == FChooserParameterGameplayTagBase::StaticStruct())
	{
		return EChooserToolsetValueKind::GameplayTags;
	}
	if (BaseType == FChooserParameterGameplayTagQueryBase::StaticStruct())
	{
		return EChooserToolsetValueKind::GameplayTagQuery;
	}
	if (BaseType->IsChildOf(FChooserParameterRandomizeBase::StaticStruct()))
	{
		return EChooserToolsetValueKind::Randomization;
	}
	return EChooserToolsetValueKind::Unknown;
}

EChooserToolsetValueKind GetPropertyValueKind(const FProperty* Property)
{
	if (!Property)
	{
		return EChooserToolsetValueKind::Unknown;
	}
	if (Property->IsA<FBoolProperty>())
	{
		return EChooserToolsetValueKind::Bool;
	}
	if (Property->IsA<FEnumProperty>())
	{
		return EChooserToolsetValueKind::Enum;
	}
	if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		return ByteProperty->Enum ? EChooserToolsetValueKind::Enum : EChooserToolsetValueKind::Float;
	}
	if (Property->IsA<FNumericProperty>())
	{
		return EChooserToolsetValueKind::Float;
	}
	if (Property->IsA<FNameProperty>() || Property->IsA<FStrProperty>() || Property->IsA<FTextProperty>())
	{
		return EChooserToolsetValueKind::Name;
	}
	if (Property->IsA<FObjectPropertyBase>() || Property->IsA<FClassProperty>())
	{
		return EChooserToolsetValueKind::Object;
	}
	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (StructProperty->Struct == FGameplayTagContainer::StaticStruct())
		{
			return EChooserToolsetValueKind::GameplayTags;
		}
		if (StructProperty->Struct == FGameplayTagQuery::StaticStruct())
		{
			return EChooserToolsetValueKind::GameplayTagQuery;
		}
		if (StructProperty->Struct == FChooserRandomizationContext::StaticStruct())
		{
			return EChooserToolsetValueKind::Randomization;
		}
		return EChooserToolsetValueKind::Struct;
	}
	return EChooserToolsetValueKind::Unknown;
}

FChooserToolsetColumnTypeInfo MakeColumnTypeInfo(const UScriptStruct* ColumnType)
{
	FChooserToolsetColumnTypeInfo Info;
	if (!ColumnType)
	{
		return Info;
	}

	Info.ColumnType = const_cast<UScriptStruct*>(ColumnType);
	Info.DisplayName = GetStructDisplayName(ColumnType);
	Info.Category = GetStructMetadata(ColumnType, Private::CategoryKey);
	Info.Description = GetStructMetadata(ColumnType, Private::TooltipKey);
	if (Info.Description.IsEmpty())
	{
		Info.Description = GetStructMetadata(ColumnType, Private::ToolTipKey);
	}
	FInstancedStruct Instance;
	Instance.InitializeAs(ColumnType);
	FChooserColumnBase& Column = Instance.GetMutable<FChooserColumnBase>();
	Info.SettingNames = GetSettingNames(ColumnType, Column.RowValuesPropertyName());
	Info.ValueKind = GetColumnValueKind(Column);
	Info.bHasOutputs = Column.HasOutputs();
	Info.bHasFilters = Column.HasFilters();
	Info.bHasCosts = Column.HasCosts();

	const FName RowValuesName = Column.RowValuesPropertyName();
	if (const FArrayProperty* ArrayProperty = FindFProperty<FArrayProperty>(ColumnType, RowValuesName))
	{
		Info.CellFormat = Private::DescribeValueFormat(ArrayProperty->Inner, 2);
	}

	return Info;
}

FChooserToolsetColumnInfo MakeColumnInfo(UChooserTable* Table, int32 ColumnIndex)
{
	FChooserToolsetColumnInfo Info;

	FString Error;
	FInstancedStruct* ColumnData = GetColumnStruct(Table, ColumnIndex, Error);
	if (!ColumnData)
	{
		return Info;
	}

	FChooserColumnBase& Column = ColumnData->GetMutable<FChooserColumnBase>();
	const UScriptStruct* ColumnType = ColumnData->GetScriptStruct();

	Info.Index = ColumnIndex;
	Info.ColumnType = const_cast<UScriptStruct*>(ColumnType);
	Info.DisplayName = GetStructDisplayName(ColumnType);
	Info.bDisabled = Column.bDisabled;
	Info.bHasOutputs = Column.HasOutputs();
	Info.Settings = GetStructSettings(ColumnType, ColumnData->GetMemory(), Column.RowValuesPropertyName());

	void* DefaultCell = nullptr;
	FString DefaultCellError;
	if (const FProperty* DefaultProperty = GetDefaultCellMemory(Table, ColumnIndex, DefaultCell, DefaultCellError))
	{
		DefaultProperty->ExportTextItem_Direct(Info.DefaultCell, DefaultCell, nullptr, Table, PPF_None);
	}

	if (const FChooserPropertyBinding* Binding = GetColumnBinding(Column))
	{
		Info.ParameterIndex = Binding->ContextIndex;
		TArray<FString> Segments;
		Segments.Reserve(Binding->PropertyBindingChain.Num());
		for (const FName& Element : Binding->PropertyBindingChain)
		{
			Segments.Add(Element.ToString());
		}
		Info.PropertyPath = FString::Join(Segments, TEXT("."));
		Info.CompileError = Binding->CompileMessage.ToString();
		if (Binding->PropertyBindingChain.IsEmpty() && !Binding->IsBoundToRoot)
		{
			Info.ParameterIndex = INDEX_NONE;
		}
	}

	return Info;
}

FChooserToolsetRowInfo MakeRowInfo(UChooserTable* Table, int32 RowIndex)
{
	FChooserToolsetRowInfo Info;
	if (!IsValidRowIndex(Table, RowIndex))
	{
		return Info;
	}

	Info.Index = RowIndex;

	const FInstancedStruct& Result = RowIndex == FallbackRowIndex
		? Table->FallbackResult
		: Table->ResultsStructs[RowIndex];
	if (Result.IsValid())
	{
		Info.Result = GetResultObject(Result);
		Info.ResultType = GetStructDisplayName(Result.GetScriptStruct());
	}

	if (Table->DisabledRows.IsValidIndex(RowIndex))
	{
		Info.bDisabled = Table->DisabledRows[RowIndex];
	}

	Info.Cells.Reserve(Table->ColumnsStructs.Num());
	for (int32 ColumnIndex = 0; ColumnIndex < Table->ColumnsStructs.Num(); ++ColumnIndex)
	{
		FString CellText;
		FString Error;
		GetCellText(Table, ColumnIndex, RowIndex, CellText, Error);
		Info.Cells.Add(CellText);
	}

	return Info;
}

FChooserPropertyBinding* GetColumnBinding(FChooserColumnBase& Column)
{
	FInstancedStruct* Input = Column.GetInputValuePtr();
	if (!Input || !Input->IsValid())
	{
		return nullptr;
	}

	const UScriptStruct* InputType = Input->GetScriptStruct();
	for (TFieldIterator<FStructProperty> It(InputType); It; ++It)
	{
		if (It->Struct && It->Struct->IsChildOf(FChooserPropertyBinding::StaticStruct()))
		{
			return It->ContainerPtrToValuePtr<FChooserPropertyBinding>(Input->GetMutableMemory());
		}
	}
	return nullptr;
}

bool BindColumn(UChooserTable* Table, int32 ColumnIndex, int32 ParameterIndex, const FString& PropertyPath, FString& OutError)
{
	FInstancedStruct* ColumnData = GetColumnStruct(Table, ColumnIndex, OutError);
	if (!ColumnData)
	{
		return false;
	}

	UChooserTable* Owner = GetParameterOwner(Table);
	const UStruct* ParameterType = GetParameterType(Table, ParameterIndex);
	if (!ParameterType)
	{
		OutError = FString::Printf(
			TEXT("parameter %d does not exist or has no type, the table has %d parameters."),
			ParameterIndex, Owner ? Owner->ContextData.Num() : 0);
		return false;
	}

	FChooserColumnBase& Column = ColumnData->GetMutable<FChooserColumnBase>();
	if (!Column.HasPrimaryInput())
	{
		OutError = FString::Printf(TEXT("column %d takes no input, so it cannot be bound."), ColumnIndex);
		return false;
	}

	FInstancedStruct* Input = Column.GetInputValuePtr();
	if (!Input)
	{
		OutError = FString::Printf(TEXT("column %d has no input to bind."), ColumnIndex);
		return false;
	}
	if (!Input->IsValid())
	{
		const UScriptStruct* InputType = Private::FindDefaultInputType(Column.GetInputBaseType());
		if (!InputType)
		{
			OutError = FString::Printf(TEXT("no parameter type binds properties for column %d."), ColumnIndex);
			return false;
		}
		Column.SetInputType(InputType);
	}

	FChooserPropertyBinding* Binding = GetColumnBinding(Column);
	if (!Binding)
	{
		OutError = FString::Printf(TEXT("column %d's input does not bind a property."), ColumnIndex);
		return false;
	}

	TArray<FName> Chain;
	FField* Leaf = nullptr;
	if (!PropertyPath.IsEmpty() && !ResolvePropertyPath(ParameterType, PropertyPath, Chain, Leaf, OutError))
	{
		return false;
	}

	NotifyTableChanged(Table);
	Binding->PropertyBindingChain = Chain;
	Binding->ContextIndex = ParameterIndex;
	Binding->IsBoundToRoot = PropertyPath.IsEmpty();
	Binding->DisplayName = PropertyPath.IsEmpty() ? ParameterType->GetName() : PropertyPath;

	// SetPropertyData fills the type a binding carries beside its path - the enum, the allowed class,
	// the struct. A binding to the parameter itself has no leaf property and reads that type from the
	// parameter instead, so this has to run either way. Without it an Output Struct column has no
	// struct type, which leaves its template cell empty and wipes every cell on the next compile.
	Binding->SetPropertyData(Owner, Leaf);

	Column.InputValueChanged();
	Table->Compile(true);
	return true;
}

bool ResolvePropertyPath(const UStruct* Root, const FString& PropertyPath, TArray<FName>& OutChain, FField*& OutLeaf, FString& OutError)
{
	OutChain.Reset();
	OutLeaf = nullptr;

	if (!Root)
	{
		OutError = TEXT("the parameter has no type to read properties from.");
		return false;
	}

	TArray<FString> Segments;
	PropertyPath.ParseIntoArray(Segments, TEXT("."));

	const UStruct* Current = Root;
	for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
	{
		const FString& Segment = Segments[SegmentIndex];
		const bool bIsLast = SegmentIndex == Segments.Num() - 1;

		FProperty* Property = Private::FindProperty(Current, Segment);
		if (!Property)
		{
			UFunction* Function = Private::FindFunction(Current, Segment);
			if (!Function)
			{
				OutError = FString::Printf(TEXT("'%s' has no property or getter named '%s'."),
					*Current->GetName(), *Segment);
				return false;
			}
			OutChain.Add(Function->GetFName());
			Property = Private::GetFunctionReturnProperty(Function);
			OutLeaf = Property;
		}
		else
		{
			OutChain.Add(Property->GetFName());
			OutLeaf = Property;
		}

		if (!bIsLast)
		{
			Current = Private::GetInnerType(Property);
			if (!Current)
			{
				OutError = FString::Printf(TEXT("'%s' is not a struct or object, so '%s' cannot continue past it."),
					*Segment, *PropertyPath);
				return false;
			}
		}
	}

	return true;
}

void CollectBindableProperties(const UStruct* Root, int32 ParameterIndex, int32 MaxDepth, const FString& NameFilter,
	TArray<FChooserToolsetBindingInfo>& OutBindings)
{
	if (!Root || MaxDepth <= 0)
	{
		return;
	}

	struct FPending
	{
		const UStruct* Type = nullptr;
		FString Path;
		int32 Depth = 0;
	};

	TArray<FPending> Queue;
	Queue.Add({ Root, FString(), 1 });

	TSet<const UStruct*> Visited;

	while (!Queue.IsEmpty())
	{
		const FPending Pending = Queue.Pop();
		if (Visited.Contains(Pending.Type) && Pending.Depth > 1)
		{
			continue;
		}
		Visited.Add(Pending.Type);

		for (TFieldIterator<FProperty> It(Pending.Type); It; ++It)
		{
			FProperty* Property = *It;
			if (Private::IsHiddenProperty(Property))
			{
				continue;
			}

			const FString Path = Pending.Path.IsEmpty() ? Property->GetName() : Pending.Path + TEXT(".") + Property->GetName();
			const EChooserToolsetValueKind Kind = GetPropertyValueKind(Property);

			if (Kind != EChooserToolsetValueKind::Unknown
				&& (NameFilter.IsEmpty() || Path.Contains(NameFilter, ESearchCase::IgnoreCase)))
			{
				FChooserToolsetBindingInfo& Info = OutBindings.AddDefaulted_GetRef();
				Info.ParameterIndex = ParameterIndex;
				Info.PropertyPath = Path;
				Info.TypeName = Property->GetCPPType();
				Info.ValueKind = Kind;
			}

			if (Pending.Depth < MaxDepth)
			{
				if (const UStruct* Inner = Private::GetInnerType(Property))
				{
					Queue.Add({ Inner, Path, Pending.Depth + 1 });
				}
			}
		}

		if (const UClass* Class = Cast<const UClass>(Pending.Type))
		{
			for (TFieldIterator<UFunction> It(Class); It; ++It)
			{
				UFunction* Function = *It;
				FProperty* ReturnProperty = Private::GetFunctionReturnProperty(Function);
				if (!ReturnProperty || !Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
				{
					continue;
				}

				const EChooserToolsetValueKind Kind = GetPropertyValueKind(ReturnProperty);
				if (Kind == EChooserToolsetValueKind::Unknown)
				{
					continue;
				}

				const FString Path = Pending.Path.IsEmpty()
					? Function->GetName()
					: Pending.Path + TEXT(".") + Function->GetName();
				if (!NameFilter.IsEmpty() && !Path.Contains(NameFilter, ESearchCase::IgnoreCase))
				{
					continue;
				}

				FChooserToolsetBindingInfo& Info = OutBindings.AddDefaulted_GetRef();
				Info.ParameterIndex = ParameterIndex;
				Info.PropertyPath = Path;
				Info.TypeName = ReturnProperty->GetCPPType();
				Info.ValueKind = Kind;
				Info.bIsFunction = true;
			}
		}
	}
}

FProperty* GetCellMemory(UChooserTable* Table, int32 ColumnIndex, int32 RowIndex, void*& OutValue, FString& OutError)
{
	OutValue = nullptr;

	FInstancedStruct* ColumnData = GetColumnStruct(Table, ColumnIndex, OutError);
	if (!ColumnData)
	{
		return nullptr;
	}
	if (!IsValidRowIndex(Table, RowIndex))
	{
		OutError = FString::Printf(TEXT("row %d does not exist."), RowIndex);
		return nullptr;
	}

	FChooserColumnBase& Column = ColumnData->GetMutable<FChooserColumnBase>();
	return Private::GetCellProperty(
		Column, ColumnData->GetScriptStruct(), ColumnData->GetMutableMemory(), RowIndex, OutValue, OutError);
}

const UEnum* GetColumnEnum(FChooserColumnBase& Column)
{
	const FInstancedStruct* InputValue = Column.GetInputValuePtr();
	if (!InputValue || !InputValue->IsValid()
		|| !InputValue->GetScriptStruct()->IsChildOf(FChooserParameterEnumBase::StaticStruct()))
	{
		return nullptr;
	}
	return InputValue->Get<FChooserParameterEnumBase>().GetEnum();
}

void RelaxNewColumnDefaultCell(FInstancedStruct& Column)
{
	Private::RelaxDefaultRowValue(Column.GetScriptStruct(), Column.GetMutableMemory());
}

FProperty* GetDefaultCellMemory(UChooserTable* Table, int32 ColumnIndex, void*& OutValue, FString& OutError)
{
	OutValue = nullptr;

	FInstancedStruct* ColumnData = GetColumnStruct(Table, ColumnIndex, OutError);
	if (!ColumnData)
	{
		return nullptr;
	}

	FProperty* Property = FindFProperty<FProperty>(ColumnData->GetScriptStruct(), TEXT("DefaultRowValue"));
	if (!Property)
	{
		OutError = FString::Printf(TEXT("column %d has no template cell for new rows."), ColumnIndex);
		return nullptr;
	}

	OutValue = Property->ContainerPtrToValuePtr<void>(ColumnData->GetMutableMemory());
	return Property;
}

bool GetCellText(UChooserTable* Table, int32 ColumnIndex, int32 RowIndex, FString& OutText, FString& OutError)
{
	OutText.Reset();

	void* Value = nullptr;
	FProperty* Property = GetCellMemory(Table, ColumnIndex, RowIndex, Value, OutError);
	if (!Property)
	{
		return false;
	}

	Property->ExportTextItem_Direct(OutText, Value, nullptr, Table, PPF_None);
	return true;
}

bool SetCellText(UChooserTable* Table, int32 ColumnIndex, int32 RowIndex, const FString& Text, FString& OutError)
{
	void* Value = nullptr;
	FProperty* Property = GetCellMemory(Table, ColumnIndex, RowIndex, Value, OutError);
	if (!Property)
	{
		return false;
	}

	NotifyTableChanged(Table);

	FStringOutputDevice ImportErrors;
	const TCHAR* Result = Property->ImportText_Direct(*Text, Value, Table, PPF_None, &ImportErrors);
	if (!Result || !ImportErrors.IsEmpty())
	{
		OutError = FString::Printf(TEXT("'%s' is not a valid value for this cell: %s"),
			*Text, ImportErrors.IsEmpty() ? TEXT("could not be parsed") : *ImportErrors);
		return false;
	}
	return true;
}

TArray<FString> GetSettingNames(const UScriptStruct* Struct, const FName& RowValuesName)
{
	TArray<FString> Names;
	if (!Struct)
	{
		return Names;
	}

	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		FProperty* Property = *It;
		if (Private::IsHiddenProperty(Property) || !Property->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}
		// Cells, bindings and the disabled flag each have their own tool, so they are not settings.
		const FString Name = Property->GetName();
		if (Property->GetFName() == RowValuesName
			|| Name.StartsWith(TEXT("DefaultRowValue"))
			|| Name == TEXT("InputValue")
			|| Name == TEXT("bDisabled"))
		{
			continue;
		}
		Names.Add(Property->GetName());
	}
	return Names;
}

TMap<FString, FString> GetStructSettings(const UScriptStruct* Struct, const void* Memory, const FName& RowValuesName)
{
	TMap<FString, FString> Settings;
	if (!Struct || !Memory)
	{
		return Settings;
	}

	for (const FString& Name : GetSettingNames(Struct, RowValuesName))
	{
		const FProperty* Property = Private::FindProperty(Struct, Name);
		if (!Property)
		{
			continue;
		}
		FString Value;
		Property->ExportTextItem_Direct(Value, Property->ContainerPtrToValuePtr<void>(Memory), nullptr, nullptr, PPF_None);
		Settings.Add(Name, Value);
	}
	return Settings;
}

bool ApplyStructSettings(const UScriptStruct* Struct, void* Memory, const TMap<FString, FString>& Settings, FString& OutError)
{
	if (!Struct || !Memory)
	{
		OutError = TEXT("nothing to apply settings to.");
		return false;
	}

	for (const TPair<FString, FString>& Setting : Settings)
	{
		FProperty* Property = Private::FindProperty(Struct, Setting.Key);
		if (!Property || Private::IsHiddenProperty(Property))
		{
			OutError = FString::Printf(TEXT("'%s' has no setting named '%s'."), *Struct->GetName(), *Setting.Key);
			return false;
		}

		FStringOutputDevice ImportErrors;
		const TCHAR* Result = Property->ImportText_Direct(
			*Setting.Value, Property->ContainerPtrToValuePtr<void>(Memory), nullptr, PPF_None, &ImportErrors);
		if (!Result || !ImportErrors.IsEmpty())
		{
			OutError = FString::Printf(TEXT("'%s' is not a valid value for '%s': %s"),
				*Setting.Value, *Setting.Key, ImportErrors.IsEmpty() ? TEXT("could not be parsed") : *ImportErrors);
			return false;
		}
	}
	return true;
}

bool MakeResultStruct(const UChooserTable* Table, UObject* Result, bool bSoftReference, FInstancedStruct& OutResult, FString& OutError)
{
	if (!Table)
	{
		OutError = TEXT("Table is required.");
		return false;
	}

	const UChooserTable* Signature = Table->GetContextOwner();

	if (!Result)
	{
		if (Signature->ResultType != EObjectChooserResultType::NoPrimaryResult)
		{
			OutError = TEXT("Result is required, this table returns a value for every row.");
			return false;
		}
		// A table with no primary result still needs a result struct on every row, otherwise its
		// output columns never run.
		OutResult.InitializeAs(FClassChooser::StaticStruct());
		OutResult.GetMutable<FClassChooser>().Class = UClass::StaticClass();
		return true;
	}

	if (UChooserTable* ResultTable = Cast<UChooserTable>(Result))
	{
		if (ResultTable->GetRootChooser() == Table->GetRootChooser() && ResultTable != Table->GetRootChooser())
		{
			OutResult.InitializeAs(FNestedChooser::StaticStruct());
			OutResult.GetMutable<FNestedChooser>().Chooser = ResultTable;
		}
		else
		{
			OutResult.InitializeAs(FEvaluateChooser::StaticStruct());
			OutResult.GetMutable<FEvaluateChooser>().Chooser = ResultTable;
		}
		return true;
	}

	if (UClass* ResultClass = Cast<UClass>(Result))
	{
		if (Signature->OutputObjectType && !ResultClass->IsChildOf(Signature->OutputObjectType))
		{
			OutError = FString::Printf(TEXT("'%s' does not derive from the table's result class '%s'."),
				*ResultClass->GetName(), *Signature->OutputObjectType->GetName());
			return false;
		}
		OutResult.InitializeAs(FClassChooser::StaticStruct());
		OutResult.GetMutable<FClassChooser>().Class = ResultClass;
		return true;
	}

	if (Signature->ResultType == EObjectChooserResultType::ObjectResult
		&& Signature->OutputObjectType
		&& !Result->IsA(Signature->OutputObjectType))
	{
		OutError = FString::Printf(TEXT("'%s' is not a '%s', which is this table's result class."),
			*Result->GetName(), *Signature->OutputObjectType->GetName());
		return false;
	}

	if (bSoftReference)
	{
		OutResult.InitializeAs(FSoftAssetChooser::StaticStruct());
		OutResult.GetMutable<FSoftAssetChooser>().Asset = Result;
	}
	else
	{
		OutResult.InitializeAs(FAssetChooser::StaticStruct());
		OutResult.GetMutable<FAssetChooser>().Asset = Result;
	}
	return true;
}

UObject* GetResultObject(const FInstancedStruct& Result)
{
	if (!Result.IsValid())
	{
		return nullptr;
	}

	if (const FClassChooser* ClassResult = Result.GetPtr<FClassChooser>())
	{
		return ClassResult->Class;
	}
	if (const FNestedChooser* NestedResult = Result.GetPtr<FNestedChooser>())
	{
		return NestedResult->Chooser;
	}
	if (const FSoftAssetChooser* SoftResult = Result.GetPtr<FSoftAssetChooser>())
	{
		return SoftResult->Asset.Get();
	}
	if (UObject* Referenced = Result.Get<FObjectChooserBase>().GetReferencedObject())
	{
		return Referenced;
	}

	// Result types from other plugins, such as the ProxyTable plugin's Lookup Proxy, still point at
	// whatever object they hold.
	for (TFieldIterator<FObjectProperty> It(Result.GetScriptStruct()); It; ++It)
	{
		if (UObject* Referenced = It->GetObjectPropertyValue(It->ContainerPtrToValuePtr<void>(Result.GetMemory())))
		{
			return Referenced;
		}
	}
	return nullptr;
}

FChooserToolsetValidationResult ValidateTable(UChooserTable* Table)
{
	FChooserToolsetValidationResult Result;
	if (!Table)
	{
		Result.Errors.Add(TEXT("Table is required."));
		return Result;
	}

	SyncRowArrays(Table);
	Table->Compile(true);

	const UChooserTable* Signature = Table->GetContextOwner();
	if (Signature->ResultType != EObjectChooserResultType::NoPrimaryResult && !Signature->OutputObjectType)
	{
		Result.Errors.Add(TEXT("The table has no result class, so nothing can be assigned to its rows."));
	}
	if (Signature->ContextData.IsEmpty())
	{
		Result.Warnings.Add(TEXT("The table has no parameters, so its columns have nothing to read."));
	}
	if (Table->ResultsStructs.IsEmpty())
	{
		Result.Warnings.Add(TEXT("The table has no rows, so it always returns its fallback result."));
	}

	for (int32 ColumnIndex = 0; ColumnIndex < Table->ColumnsStructs.Num(); ++ColumnIndex)
	{
		FInstancedStruct& ColumnData = Table->ColumnsStructs[ColumnIndex];
		const FString Context = FString::Printf(TEXT("Column %d"), ColumnIndex);
		if (!ColumnData.IsValid())
		{
			Result.Errors.Add(FString::Printf(TEXT("%s is empty."), *Context));
			continue;
		}

		FChooserColumnBase& Column = ColumnData.GetMutable<FChooserColumnBase>();
		if (Column.bDisabled)
		{
			Result.Warnings.Add(FString::Printf(TEXT("%s (%s) is disabled and does nothing."),
				*Context, *GetStructDisplayName(ColumnData.GetScriptStruct())));
			continue;
		}

		if (!Column.HasPrimaryInput())
		{
			continue;
		}

		FChooserParameterBase* Input = Column.GetInputValue();
		if (!Input)
		{
			Result.Errors.Add(FString::Printf(TEXT("%s (%s) has no property bound."),
				*Context, *GetStructDisplayName(ColumnData.GetScriptStruct())));
			continue;
		}

		FText Message;
		if (Input->HasCompileErrors(Message))
		{
			Private::AppendCompileError(Message, Context, Result.Errors);
		}
	}

	for (int32 RowIndex = 0; RowIndex < Table->ResultsStructs.Num(); ++RowIndex)
	{
		FInstancedStruct& RowResult = Table->ResultsStructs[RowIndex];
		const FString Context = FString::Printf(TEXT("Row %d"), RowIndex);
		if (!RowResult.IsValid())
		{
			Result.Errors.Add(FString::Printf(TEXT("%s has no result, so it selects nothing."), *Context));
			continue;
		}

		FText Message;
		if (RowResult.GetMutable<FObjectChooserBase>().HasCompileErrors(Message))
		{
			Private::AppendCompileError(Message, Context, Result.Errors);
		}
		else if (!GetResultObject(RowResult) && Signature->ResultType != EObjectChooserResultType::NoPrimaryResult)
		{
			Result.Warnings.Add(FString::Printf(TEXT("%s points at nothing."), *Context));
		}
	}

	Result.bSuccess = Result.Errors.IsEmpty();
	return Result;
}

}
