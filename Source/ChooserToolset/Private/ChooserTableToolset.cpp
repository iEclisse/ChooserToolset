// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#include "ChooserTableToolset.h"

#include "Animation/AnimationAsset.h"
#include "Animation/AnimInstance.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "Chooser.h"
#include "ChooserPropertyAccess.h"
#include "ChooserToolsetUtils.h"
#include "ChooserTypes.h"
#include "Editor.h"
#include "IChooserColumn.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"

using namespace UE::ChooserToolset;

namespace
{
	/** Adds a parameter to the table that owns the parameter list, and returns its index. */
	int32 AddParameter(UChooserTable* Table, const UScriptStruct* ParameterType,
		TFunctionRef<void(FContextObjectTypeBase&)> Initialize, EContextObjectDirection Direction)
	{
		UChooserTable* Owner = GetParameterOwner(Table);
		NotifyTableChanged(Owner);

		FInstancedStruct& Parameter = Owner->ContextData.AddDefaulted_GetRef();
		Parameter.InitializeAs(ParameterType);

		FContextObjectTypeBase& ParameterData = Parameter.GetMutable<FContextObjectTypeBase>();
		ParameterData.Direction = Direction;
		Initialize(ParameterData);

		Owner->OnContextClassChanged.Broadcast();
		Owner->Compile(true);
		return Owner->ContextData.Num() - 1;
	}
}

TArray<UChooserTable*> UChooserTableToolset::ListChooserTables(const FString& FolderPath)
{
	return FindChooserTables(FolderPath);
}

FChooserToolsetTableInfo UChooserTableToolset::DescribeChooserTable(UChooserTable* Table)
{
	FChooserToolsetTableInfo Info;
	if (!PrepareTable(TEXT("DescribeChooserTable"), Table))
	{
		return Info;
	}

	const UChooserTable* Signature = Table->GetContextOwner();
	Info.Table = Table;
	Info.ResultType = Signature->ResultType;
	Info.ResultClass = Signature->OutputObjectType;

	for (int32 ParameterIndex = 0; ParameterIndex < Signature->ContextData.Num(); ++ParameterIndex)
	{
		FChooserToolsetParameterInfo& Parameter = Info.Parameters.AddDefaulted_GetRef();
		Parameter.Index = ParameterIndex;

		const FInstancedStruct& ParameterData = Signature->ContextData[ParameterIndex];
		if (const FContextObjectTypeClass* ClassParameter = ParameterData.GetPtr<FContextObjectTypeClass>())
		{
			Parameter.Class = ClassParameter->Class;
			Parameter.Direction = ClassParameter->Direction;
		}
		else if (const FContextObjectTypeStruct* StructParameter = ParameterData.GetPtr<FContextObjectTypeStruct>())
		{
			Parameter.Struct = StructParameter->Struct;
			Parameter.Direction = StructParameter->Direction;
		}
	}

	for (int32 ColumnIndex = 0; ColumnIndex < Table->ColumnsStructs.Num(); ++ColumnIndex)
	{
		Info.Columns.Add(MakeColumnInfo(Table, ColumnIndex));
	}

	for (int32 RowIndex = 0; RowIndex < Table->ResultsStructs.Num(); ++RowIndex)
	{
		Info.Rows.Add(MakeRowInfo(Table, RowIndex));
	}

	Info.FallbackRow = MakeRowInfo(Table, FallbackRowIndex);

	for (const TObjectPtr<UObject>& NestedObject : Table->GetRootChooser()->NestedObjects)
	{
		if (UChooserTable* NestedTable = Cast<UChooserTable>(NestedObject))
		{
			Info.NestedChoosers.Add(NestedTable);
		}
	}

	return Info;
}

UChooserTable* UChooserTableToolset::CreateChooserTable(
	const FString& FolderPath, const FString& AssetName, UClass* ResultClass, EObjectChooserResultType ResultType)
{
	static const TCHAR* Tool = TEXT("CreateChooserTable");

	if (!ResultClass && ResultType != EObjectChooserResultType::NoPrimaryResult)
	{
		FailTool(Tool, TEXT("ResultClass is required unless ResultType is NoPrimaryResult."));
		return nullptr;
	}

	FString Error;
	UPackage* Package = CreateAssetPackage(FolderPath, AssetName, Error);
	if (!Package)
	{
		FailTool(Tool, Error);
		return nullptr;
	}

	UChooserTable* Table = NewObject<UChooserTable>(
		Package, UChooserTable::StaticClass(), FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
	Table->Version = UChooserTable::CurrentVersion;
	Table->ResultType = ResultType;
	Table->OutputObjectType = ResultType == EObjectChooserResultType::NoPrimaryResult
		? UClass::StaticClass()
		: ResultClass;

	FinalizeNewAsset(Table);
	return Table;
}

UChooserTable* UChooserTableToolset::CreateAnimationChooserTable(
	const FString& FolderPath, const FString& AssetName, TSubclassOf<UAnimInstance> AnimInstanceClass)
{
	UChooserTable* Table = CreateChooserTable(
		FolderPath, AssetName, UAnimationAsset::StaticClass(), EObjectChooserResultType::ObjectResult);
	if (!Table)
	{
		return nullptr;
	}

	Table->ContextData.SetNum(2);

	Table->ContextData[0].InitializeAs(FContextObjectTypeClass::StaticStruct());
	FContextObjectTypeClass& AnimInstanceParameter = Table->ContextData[0].GetMutable<FContextObjectTypeClass>();
	AnimInstanceParameter.Class = AnimInstanceClass ? AnimInstanceClass.Get() : UAnimInstance::StaticClass();
	AnimInstanceParameter.Direction = EContextObjectDirection::ReadWrite;

	Table->ContextData[1].InitializeAs(FContextObjectTypeStruct::StaticStruct());
	FContextObjectTypeStruct& SettingsParameter = Table->ContextData[1].GetMutable<FContextObjectTypeStruct>();
	SettingsParameter.Struct = FChooserPlayerSettings::StaticStruct();
	SettingsParameter.Direction = EContextObjectDirection::Write;

	Table->Compile(true);
	return Table;
}

bool UChooserTableToolset::SetChooserResultType(UChooserTable* Table, UClass* ResultClass, EObjectChooserResultType ResultType)
{
	static const TCHAR* Tool = TEXT("SetChooserResultType");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}
	if (!ResultClass && ResultType != EObjectChooserResultType::NoPrimaryResult)
	{
		FailTool(Tool, TEXT("ResultClass is required unless ResultType is NoPrimaryResult."));
		return false;
	}

	UChooserTable* Owner = GetParameterOwner(Table);
	NotifyTableChanged(Owner);
	Owner->ResultType = ResultType;
	Owner->OutputObjectType = ResultType == EObjectChooserResultType::NoPrimaryResult
		? UClass::StaticClass()
		: ResultClass;
	Owner->OnOutputObjectTypeChanged.Broadcast(Owner->OutputObjectType);
	Owner->Compile(true);
	return true;
}

int32 UChooserTableToolset::AddChooserObjectParameter(UChooserTable* Table, UClass* ObjectClass, EContextObjectDirection Direction)
{
	static const TCHAR* Tool = TEXT("AddChooserObjectParameter");
	if (!PrepareTable(Tool, Table))
	{
		return INDEX_NONE;
	}
	if (!ObjectClass)
	{
		FailTool(Tool, TEXT("ObjectClass is required."));
		return INDEX_NONE;
	}

	return AddParameter(Table, FContextObjectTypeClass::StaticStruct(),
		[ObjectClass](FContextObjectTypeBase& Parameter)
		{
			static_cast<FContextObjectTypeClass&>(Parameter).Class = ObjectClass;
		},
		Direction);
}

int32 UChooserTableToolset::AddChooserStructParameter(UChooserTable* Table, UScriptStruct* Struct, EContextObjectDirection Direction)
{
	static const TCHAR* Tool = TEXT("AddChooserStructParameter");
	if (!PrepareTable(Tool, Table))
	{
		return INDEX_NONE;
	}
	if (!Struct)
	{
		FailTool(Tool, TEXT("Struct is required."));
		return INDEX_NONE;
	}

	return AddParameter(Table, FContextObjectTypeStruct::StaticStruct(),
		[Struct](FContextObjectTypeBase& Parameter)
		{
			static_cast<FContextObjectTypeStruct&>(Parameter).Struct = Struct;
		},
		Direction);
}

bool UChooserTableToolset::RemoveChooserParameter(UChooserTable* Table, int32 ParameterIndex)
{
	static const TCHAR* Tool = TEXT("RemoveChooserParameter");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}

	UChooserTable* Owner = GetParameterOwner(Table);
	if (!Owner->ContextData.IsValidIndex(ParameterIndex))
	{
		FailTool(Tool, FString::Printf(TEXT("parameter %d does not exist, the table has %d parameters."),
			ParameterIndex, Owner->ContextData.Num()));
		return false;
	}

	NotifyTableChanged(Owner);
	Owner->ContextData.RemoveAt(ParameterIndex);
	Owner->OnContextClassChanged.Broadcast();
	Owner->Compile(true);
	return true;
}

bool UChooserTableToolset::SetChooserParameterDirection(UChooserTable* Table, int32 ParameterIndex, EContextObjectDirection Direction)
{
	static const TCHAR* Tool = TEXT("SetChooserParameterDirection");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}

	UChooserTable* Owner = GetParameterOwner(Table);
	if (!Owner->ContextData.IsValidIndex(ParameterIndex))
	{
		FailTool(Tool, FString::Printf(TEXT("parameter %d does not exist, the table has %d parameters."),
			ParameterIndex, Owner->ContextData.Num()));
		return false;
	}

	NotifyTableChanged(Owner);
	Owner->ContextData[ParameterIndex].GetMutable<FContextObjectTypeBase>().Direction = Direction;
	Owner->OnContextClassChanged.Broadcast();
	Owner->Compile(true);
	return true;
}

TArray<FChooserToolsetBindingInfo> UChooserTableToolset::ListBindableProperties(
	UChooserTable* Table, int32 ParameterIndex, const FString& NameFilter, int32 MaxDepth)
{
	static const TCHAR* Tool = TEXT("ListBindableProperties");

	TArray<FChooserToolsetBindingInfo> Bindings;
	if (!PrepareTable(Tool, Table))
	{
		return Bindings;
	}
	if (MaxDepth < 1)
	{
		FailTool(Tool, TEXT("MaxDepth must be at least 1."));
		return Bindings;
	}

	const UChooserTable* Owner = Table->GetContextOwner();
	if (ParameterIndex >= 0 && !Owner->ContextData.IsValidIndex(ParameterIndex))
	{
		FailTool(Tool, FString::Printf(TEXT("parameter %d does not exist, the table has %d parameters."),
			ParameterIndex, Owner->ContextData.Num()));
		return Bindings;
	}

	const int32 First = ParameterIndex >= 0 ? ParameterIndex : 0;
	const int32 Last = ParameterIndex >= 0 ? ParameterIndex : Owner->ContextData.Num() - 1;
	for (int32 Index = First; Index <= Last; ++Index)
	{
		CollectBindableProperties(GetParameterType(Table, Index), Index, MaxDepth, NameFilter, Bindings);
	}
	return Bindings;
}

UChooserTable* UChooserTableToolset::CreateNestedChooser(UChooserTable* Table, const FString& Name)
{
	static const TCHAR* Tool = TEXT("CreateNestedChooser");
	if (!PrepareTable(Tool, Table))
	{
		return nullptr;
	}
	if (Name.IsEmpty())
	{
		FailTool(Tool, TEXT("Name is required."));
		return nullptr;
	}

	UChooserTable* Root = Table->GetRootChooser();

	FName NestedName(*Name);
	if (FindObject<UObject>(Root, *Name))
	{
		NestedName = MakeUniqueObjectName(Root, UChooserTable::StaticClass(), NestedName);
	}

	UChooserTable* Nested = NewObject<UChooserTable>(Root, UChooserTable::StaticClass(), NestedName, RF_Transactional);
	Nested->Version = UChooserTable::CurrentVersion;
	Nested->RootChooser = Root;

	NotifyTableChanged(Root);
	Root->AddNestedChooser(Nested);

	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->AssetUpdateTags(Root, EAssetRegistryTagsCaller::Fast);
	}
	return Nested;
}

bool UChooserTableToolset::DeleteNestedChooser(UChooserTable* Table, UChooserTable* NestedChooser)
{
	static const TCHAR* Tool = TEXT("DeleteNestedChooser");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}
	if (!NestedChooser)
	{
		FailTool(Tool, TEXT("NestedChooser is required."));
		return false;
	}

	UChooserTable* Root = Table->GetRootChooser();
	if (!Root->NestedObjects.Contains(NestedChooser))
	{
		FailTool(Tool, FString::Printf(TEXT("'%s' is not nested inside '%s'."),
			*NestedChooser->GetName(), *Root->GetName()));
		return false;
	}

	Root->DeleteNestedChooser(NestedChooser);
	return true;
}

FChooserToolsetValidationResult UChooserTableToolset::ValidateChooserTable(UChooserTable* Table)
{
	if (!PrepareTable(TEXT("ValidateChooserTable"), Table))
	{
		return FChooserToolsetValidationResult();
	}
	return ValidateTable(Table);
}

int32 UChooserTableToolset::ReplaceChooserBindingNames(
	UChooserTable* Table, const FString& FindName, const FString& ReplaceName, bool bMatchWholeWord, bool bMatchCase)
{
	static const TCHAR* Tool = TEXT("ReplaceChooserBindingNames");
	if (!PrepareTable(Tool, Table))
	{
		return 0;
	}
	if (FindName.IsEmpty())
	{
		FailTool(Tool, TEXT("FindName is required."));
		return 0;
	}

	const ESearchCase::Type SearchCase = bMatchCase ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase;

	int32 ChangedCount = 0;
	for (FInstancedStruct& ColumnData : Table->ColumnsStructs)
	{
		if (!ColumnData.IsValid())
		{
			continue;
		}

		FChooserColumnBase& Column = ColumnData.GetMutable<FChooserColumnBase>();
		FChooserPropertyBinding* Binding = GetColumnBinding(Column);
		if (!Binding)
		{
			continue;
		}

		const TArray<FName> Before = Binding->PropertyBindingChain;
		Column.GetInputValue()->ReplaceString(FindName, SearchCase, bMatchWholeWord, ReplaceName);
		if (Binding->PropertyBindingChain != Before)
		{
			++ChangedCount;
		}
	}

	if (ChangedCount > 0)
	{
		NotifyTableChanged(Table);
		Table->Compile(true);
	}
	return ChangedCount;
}

bool UChooserTableToolset::RemoveDisabledChooserData(UChooserTable* Table)
{
	static const TCHAR* Tool = TEXT("RemoveDisabledChooserData");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}

	NotifyTableChanged(Table);
	SyncRowArrays(Table);
	Table->RemoveDisabledData();
	SyncRowArrays(Table);
	Table->Compile(true);
	return true;
}

bool UChooserTableToolset::OpenChooserEditor(UChooserTable* Table)
{
	static const TCHAR* Tool = TEXT("OpenChooserEditor");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}
	if (!GEditor)
	{
		FailTool(Tool, TEXT("no editor is running."));
		return false;
	}
	return GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Table);
}

bool UChooserTableToolset::CloseChooserEditor(UChooserTable* Table)
{
	static const TCHAR* Tool = TEXT("CloseChooserEditor");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}
	if (!GEditor)
	{
		FailTool(Tool, TEXT("no editor is running."));
		return false;
	}
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Table);
	return true;
}
