// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#include "ChooserCellToolset.h"

#include "Chooser.h"
#include "ChooserToolsetUtils.h"
#include "EnumColumn.h"
#include "GameplayTagContainer.h"
#include "IChooserColumn.h"
#include "MultiEnumColumn.h"
#include "ObjectChooser_Asset.h"
#include "ObjectChooser_Class.h"
#include "ObjectColumn.h"
#include "OutputEnumColumn.h"
#include "OutputObjectColumn.h"

using namespace UE::ChooserToolset;

namespace
{
	/** Locates a cell and fails the tool when it does not exist. */
	FStructProperty* GetStructCell(const TCHAR* Tool, UChooserTable* Table, int32 ColumnIndex, int32 RowIndex, void*& OutValue)
	{
		if (!PrepareTable(Tool, Table))
		{
			return nullptr;
		}

		FString Error;
		FProperty* Property = GetCellMemory(Table, ColumnIndex, RowIndex, OutValue, Error);
		if (!Property)
		{
			FailTool(Tool, Error);
			return nullptr;
		}

		FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (!StructProperty)
		{
			FailTool(Tool, FString::Printf(
				TEXT("column %d holds plain values, so use SetChooserCell with its text form instead."), ColumnIndex));
			return nullptr;
		}
		return StructProperty;
	}

	/** Resolves an enum value by short name, full name or display name. */
	bool FindEnumValue(const UEnum* Enum, const FString& Name, int64& OutValue, FName& OutFullName)
	{
		int64 Value = Enum->GetValueByNameString(Name);
		if (Value == INDEX_NONE)
		{
			Value = Enum->GetValueByNameString(Enum->GetName() + TEXT("::") + Name);
		}
		if (Value == INDEX_NONE)
		{
			for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
			{
				if (Enum->GetDisplayNameTextByIndex(Index).ToString().Equals(Name, ESearchCase::IgnoreCase))
				{
					Value = Enum->GetValueByIndex(Index);
					break;
				}
			}
		}
		if (Value == INDEX_NONE)
		{
			return false;
		}

		OutValue = Value;
		OutFullName = FName(*Enum->GetNameStringByValue(Value));
		return true;
	}

	/** Lists the value names of an enum, for error messages. */
	FString DescribeEnumValues(const UEnum* Enum)
	{
		TArray<FString> Names;
		for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
		{
			Names.Add(Enum->GetNameStringByIndex(Index));
		}
		return FString::Join(Names, TEXT(", "));
	}

	EEnumColumnCellValueComparison ToEnumComparison(EChooserToolsetComparison Comparison)
	{
		switch (Comparison)
		{
		case EChooserToolsetComparison::NotEqual:
			return EEnumColumnCellValueComparison::MatchNotEqual;
		case EChooserToolsetComparison::Any:
			return EEnumColumnCellValueComparison::MatchAny;
		default:
			return EEnumColumnCellValueComparison::MatchEqual;
		}
	}

	EObjectColumnCellValueComparison ToObjectComparison(EChooserToolsetComparison Comparison)
	{
		switch (Comparison)
		{
		case EChooserToolsetComparison::NotEqual:
			return EObjectColumnCellValueComparison::MatchNotEqual;
		case EChooserToolsetComparison::Any:
			return EObjectColumnCellValueComparison::MatchAny;
		default:
			return EObjectColumnCellValueComparison::MatchEqual;
		}
	}
}

FString UChooserCellToolset::GetChooserCell(UChooserTable* Table, int32 ColumnIndex, int32 RowIndex)
{
	static const TCHAR* Tool = TEXT("GetChooserCell");
	if (!PrepareTable(Tool, Table))
	{
		return FString();
	}

	FString Text;
	FString Error;
	if (!GetCellText(Table, ColumnIndex, RowIndex, Text, Error))
	{
		FailTool(Tool, Error);
		return FString();
	}
	return Text;
}

bool UChooserCellToolset::SetChooserCell(UChooserTable* Table, int32 ColumnIndex, int32 RowIndex, const FString& Value)
{
	static const TCHAR* Tool = TEXT("SetChooserCell");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}

	FString Error;
	if (!SetCellText(Table, ColumnIndex, RowIndex, Value, Error))
	{
		FailTool(Tool, Error);
		return false;
	}
	return true;
}

bool UChooserCellToolset::SetChooserEnumCell(
	UChooserTable* Table, int32 ColumnIndex, int32 RowIndex, const TArray<FString>& ValueNames, EChooserToolsetComparison Comparison)
{
	static const TCHAR* Tool = TEXT("SetChooserEnumCell");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}
	if (ValueNames.IsEmpty())
	{
		FailTool(Tool, TEXT("ValueNames must name at least one enum value."));
		return false;
	}

	FString Error;
	FInstancedStruct* ColumnData = GetColumnStruct(Table, ColumnIndex, Error);
	if (!ColumnData)
	{
		FailTool(Tool, Error);
		return false;
	}

	const UEnum* Enum = GetColumnEnum(ColumnData->GetMutable<FChooserColumnBase>());
	if (!Enum)
	{
		FailTool(Tool, FString::Printf(
			TEXT("column %d is not bound to an enum property, so its cells have no value names."), ColumnIndex));
		return false;
	}

	TArray<int64> Values;
	FName FirstFullName;
	for (const FString& Name : ValueNames)
	{
		int64 Value = 0;
		FName FullName;
		if (!FindEnumValue(Enum, Name, Value, FullName))
		{
			FailTool(Tool, FString::Printf(TEXT("'%s' is not a value of enum '%s'. Its values are: %s."),
				*Name, *Enum->GetName(), *DescribeEnumValues(Enum)));
			return false;
		}
		if (Values.IsEmpty())
		{
			FirstFullName = FullName;
		}
		Values.Add(Value);
	}

	void* Cell = nullptr;
	FStructProperty* CellProperty = GetStructCell(Tool, Table, ColumnIndex, RowIndex, Cell);
	if (!CellProperty)
	{
		return false;
	}

	NotifyTableChanged(Table);

	if (CellProperty->Struct == FChooserEnumRowData::StaticStruct())
	{
		FChooserEnumRowData& RowData = *static_cast<FChooserEnumRowData*>(Cell);
		RowData.Value = static_cast<uint8>(Values[0]);
		RowData.ValueName = FirstFullName;
		RowData.Comparison = ToEnumComparison(Comparison);
		return true;
	}

	if (CellProperty->Struct == FChooserOutputEnumRowData::StaticStruct())
	{
		FChooserOutputEnumRowData& RowData = *static_cast<FChooserOutputEnumRowData*>(Cell);
		RowData.Value = static_cast<uint8>(Values[0]);
		RowData.ValueName = FirstFullName;
		return true;
	}

	if (CellProperty->Struct == FChooserMultiEnumRowData::StaticStruct())
	{
		uint32 Mask = 0;
		for (int64 Value : Values)
		{
			if (Value < 0 || Value > 31)
			{
				FailTool(Tool, FString::Printf(
					TEXT("an Enum (Or) column only matches enum values 0 to 31, and '%s' is %lld."),
					*Enum->GetNameStringByValue(Value), Value));
				return false;
			}
			Mask |= 1u << static_cast<uint32>(Value);
		}
		static_cast<FChooserMultiEnumRowData*>(Cell)->Value = Mask;
		return true;
	}

	FailTool(Tool, FString::Printf(TEXT("column %d does not hold enum cells."), ColumnIndex));
	return false;
}

bool UChooserCellToolset::SetChooserGameplayTagCell(UChooserTable* Table, int32 ColumnIndex, int32 RowIndex, const TArray<FString>& Tags)
{
	static const TCHAR* Tool = TEXT("SetChooserGameplayTagCell");

	void* Cell = nullptr;
	FStructProperty* CellProperty = GetStructCell(Tool, Table, ColumnIndex, RowIndex, Cell);
	if (!CellProperty)
	{
		return false;
	}
	if (CellProperty->Struct != FGameplayTagContainer::StaticStruct())
	{
		FailTool(Tool, FString::Printf(TEXT("column %d does not hold gameplay tag cells."), ColumnIndex));
		return false;
	}

	FGameplayTagContainer Container;
	for (const FString& Tag : Tags)
	{
		const FGameplayTag Resolved = FGameplayTag::RequestGameplayTag(FName(*Tag), false);
		if (!Resolved.IsValid())
		{
			FailTool(Tool, FString::Printf(TEXT("'%s' is not a registered gameplay tag."), *Tag));
			return false;
		}
		Container.AddTag(Resolved);
	}

	NotifyTableChanged(Table);
	*static_cast<FGameplayTagContainer*>(Cell) = MoveTemp(Container);
	return true;
}

bool UChooserCellToolset::SetChooserObjectCell(
	UChooserTable* Table, int32 ColumnIndex, int32 RowIndex, UObject* Value, EChooserToolsetComparison Comparison)
{
	static const TCHAR* Tool = TEXT("SetChooserObjectCell");

	void* Cell = nullptr;
	FStructProperty* CellProperty = GetStructCell(Tool, Table, ColumnIndex, RowIndex, Cell);
	if (!CellProperty)
	{
		return false;
	}

	NotifyTableChanged(Table);

	if (CellProperty->Struct == FChooserObjectRowData::StaticStruct())
	{
		FChooserObjectRowData& RowData = *static_cast<FChooserObjectRowData*>(Cell);
		RowData.Value = Value;
		RowData.Comparison = ToObjectComparison(Comparison);
		return true;
	}

	if (CellProperty->Struct == FChooserOutputObjectRowData::StaticStruct())
	{
		FChooserOutputObjectRowData& RowData = *static_cast<FChooserOutputObjectRowData*>(Cell);
		if (UClass* ValueClass = Cast<UClass>(Value))
		{
			RowData.Value.InitializeAs(FClassChooser::StaticStruct());
			RowData.Value.GetMutable<FClassChooser>().Class = ValueClass;
		}
		else
		{
			RowData.Value.InitializeAs(FAssetChooser::StaticStruct());
			RowData.Value.GetMutable<FAssetChooser>().Asset = Value;
		}
		return true;
	}

	FailTool(Tool, FString::Printf(TEXT("column %d does not hold object cells."), ColumnIndex));
	return false;
}

bool UChooserCellToolset::SetChooserClassCell(
	UChooserTable* Table, int32 ColumnIndex, int32 RowIndex, UClass* Value, EObjectClassColumnCellValueComparison Comparison)
{
	static const TCHAR* Tool = TEXT("SetChooserClassCell");

	void* Cell = nullptr;
	FStructProperty* CellProperty = GetStructCell(Tool, Table, ColumnIndex, RowIndex, Cell);
	if (!CellProperty)
	{
		return false;
	}
	if (CellProperty->Struct != FChooserObjectClassRowData::StaticStruct())
	{
		FailTool(Tool, FString::Printf(TEXT("column %d is not an Object Class column."), ColumnIndex));
		return false;
	}

	NotifyTableChanged(Table);
	FChooserObjectClassRowData& RowData = *static_cast<FChooserObjectClassRowData*>(Cell);
	RowData.Value = Value;
	RowData.Comparison = Comparison;
	return true;
}
