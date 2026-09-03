// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#include "ChooserEvaluationToolset.h"

#include "Chooser.h"
#include "ChooserPropertyAccess.h"
#include "ChooserToolsetUtils.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/UnrealType.h"

using namespace UE::ChooserToolset;

namespace
{
	/** The value supplied for a parameter, or null when the caller left it out. */
	const FChooserToolsetEvaluationParameter* FindParameter(
		const TArray<FChooserToolsetEvaluationParameter>& Parameters, int32 ParameterIndex)
	{
		return Parameters.FindByPredicate(
			[ParameterIndex](const FChooserToolsetEvaluationParameter& Parameter)
			{
				return Parameter.ParameterIndex == ParameterIndex;
			});
	}

	/**
	 * The name the chooser debugger knows a context object by, or empty when the table never saw it.
	 *
	 * UpdateDebugging registers every context object it evaluates under a display name that may carry
	 * the owning actor and the world it came from, so the name is read back from the table instead of
	 * being rebuilt here.
	 */
	FString FindDebugTargetName(const UChooserTable* Table, const UObject* ContextObject)
	{
		if (!ContextObject)
		{
			return FString();
		}

		const FString ObjectName = ContextObject->GetName();
		const FString DecoratedPrefix = ObjectName + TEXT(" ");

		FString Match;
		Table->IterateRecentContextObjects(
			[&Match, &ObjectName, &DecoratedPrefix](const FString& Name)
			{
				if (Match.IsEmpty() && (Name == ObjectName || Name.StartsWith(DecoratedPrefix)))
				{
					Match = Name;
				}
			});
		return Match;
	}

	/** Reads every field of a struct into text, keyed by "ParameterIndex.FieldName". */
	void CollectStructFields(const UScriptStruct* Struct, const void* Memory, int32 ParameterIndex, TMap<FString, FString>& OutValues)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FString Value;
			It->ExportTextItem_Direct(Value, It->ContainerPtrToValuePtr<void>(Memory), nullptr, nullptr, PPF_None);
			OutValues.Add(FString::Printf(TEXT("%d.%s"), ParameterIndex, *It->GetName()), Value);
		}
	}
}

FChooserToolsetEvaluationResult UChooserEvaluationToolset::EvaluateChooserTable(
	UChooserTable* Table, const TArray<FChooserToolsetEvaluationParameter>& Parameters, bool bReturnAllMatches, int32 RandomSeed)
{
	static const TCHAR* Tool = TEXT("EvaluateChooserTable");

	FChooserToolsetEvaluationResult Result;
	if (!PrepareTable(Tool, Table))
	{
		return Result;
	}

	Table->Compile(true);

	const UChooserTable* Signature = Table->GetContextOwner();
	const int32 ParameterCount = Signature->ContextData.Num();

	for (const FChooserToolsetEvaluationParameter& Parameter : Parameters)
	{
		if (Parameter.ParameterIndex < 0 || Parameter.ParameterIndex >= ParameterCount)
		{
			FailTool(Tool, FString::Printf(TEXT("parameter %d does not exist, the table has %d parameters."),
				Parameter.ParameterIndex, ParameterCount));
			return Result;
		}
	}

	// The context holds struct parameters by reference, so their storage has to outlive the evaluation
	// and never move while parameters are being added.
	TArray<FInstancedStruct> StructStorage;
	StructStorage.Reserve(ParameterCount);

	FChooserEvaluationContext Context;
	Context.Params.Reserve(ParameterCount);
	Context.ObjectParams.Reserve(ParameterCount);

	const UObject* DebugContextObject = nullptr;

	for (int32 ParameterIndex = 0; ParameterIndex < ParameterCount; ++ParameterIndex)
	{
		const FChooserToolsetEvaluationParameter* Supplied = FindParameter(Parameters, ParameterIndex);
		const FInstancedStruct& ParameterData = Signature->ContextData[ParameterIndex];

		if (const FContextObjectTypeClass* ClassParameter = ParameterData.GetPtr<FContextObjectTypeClass>())
		{
			UObject* Object = Supplied ? Supplied->Object : nullptr;
			if (Object && ClassParameter->Class && !Object->IsA(ClassParameter->Class))
			{
				FailTool(Tool, FString::Printf(TEXT("parameter %d expects a '%s', but '%s' was passed."),
					ParameterIndex, *ClassParameter->Class->GetName(), *Object->GetName()));
				return Result;
			}
			Context.AddObjectParam(Object);
			if (!DebugContextObject)
			{
				DebugContextObject = Object;
			}
			continue;
		}

		const FContextObjectTypeStruct* StructParameter = ParameterData.GetPtr<FContextObjectTypeStruct>();
		if (!StructParameter || !StructParameter->Struct)
		{
			FailTool(Tool, FString::Printf(TEXT("parameter %d has no type, so nothing can be passed for it."), ParameterIndex));
			return Result;
		}

		FInstancedStruct& Storage = StructStorage.AddDefaulted_GetRef();
		Storage.InitializeAs(StructParameter->Struct);

		if (Supplied && !Supplied->Fields.IsEmpty())
		{
			FString Error;
			if (!ApplyStructSettings(StructParameter->Struct, Storage.GetMutableMemory(), Supplied->Fields, Error))
			{
				FailTool(Tool, FString::Printf(TEXT("parameter %d: %s"), ParameterIndex, *Error));
				return Result;
			}
		}

		Context.AddStructViewParam(FStructView(StructParameter->Struct, Storage.GetMutableMemory()));
	}

	FRandomStream RandomStream(RandomSeed);
	if (RandomSeed >= 0)
	{
		Context.RandomStream = &RandomStream;
	}

	Table->SetDebugSelectedRows({});

	// The chooser only records the row it picked while it evaluates the object it is debugging, which
	// in a game is whatever the user picked in the editor. Pointing the debug target at the caller's
	// own context object for the length of this call is what makes SelectedRows report anything.
	// UpdateDebugging has to run first: it is what registers the object under its debugger name.
	UChooserTable* Root = Table->GetRootChooser();
	const FString PreviousDebugTarget = Root->GetDebugTargetName();
	Table->UpdateDebugging(Context);
	const FString DebugTarget = FindDebugTargetName(Table, DebugContextObject);
	if (!DebugTarget.IsEmpty())
	{
		Root->SetDebugTarget(DebugTarget);
	}

	TArray<TObjectPtr<UObject>> Selected;
	UChooserTable::EvaluateChooser(Context, Table,
		FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda(
			[&Selected, bReturnAllMatches](UObject* Object)
			{
				Selected.Add(Object);
				return bReturnAllMatches
					? FObjectChooserBase::EIteratorStatus::Continue
					: FObjectChooserBase::EIteratorStatus::Stop;
			}));

	Root->SetDebugTarget(PreviousDebugTarget);

	Result.Results = MoveTemp(Selected);
	Result.SelectedRows = Table->GetDebugSelectedRows();

	int32 StructIndex = 0;
	for (int32 ParameterIndex = 0; ParameterIndex < ParameterCount; ++ParameterIndex)
	{
		const FInstancedStruct& ParameterData = Signature->ContextData[ParameterIndex];
		const FContextObjectTypeStruct* StructParameter = ParameterData.GetPtr<FContextObjectTypeStruct>();
		if (!StructParameter)
		{
			continue;
		}

		const FInstancedStruct& Storage = StructStorage[StructIndex++];
		if (StructParameter->Direction != EContextObjectDirection::Read)
		{
			CollectStructFields(StructParameter->Struct, Storage.GetMemory(), ParameterIndex, Result.Outputs);
		}
	}

	return Result;
}

TArray<FString> UChooserEvaluationToolset::ListChooserDebugTargets(UChooserTable* Table)
{
	TArray<FString> Targets;
	if (!PrepareTable(TEXT("ListChooserDebugTargets"), Table))
	{
		return Targets;
	}

	Table->GetRootChooser()->IterateRecentContextObjects(
		[&Targets](const FString& Name)
		{
			Targets.AddUnique(Name);
		});
	return Targets;
}

bool UChooserEvaluationToolset::SetChooserDebugTarget(UChooserTable* Table, const FString& TargetName)
{
	static const TCHAR* Tool = TEXT("SetChooserDebugTarget");
	if (!PrepareTable(Tool, Table))
	{
		return false;
	}

	UChooserTable* Root = Table->GetRootChooser();
	if (TargetName.IsEmpty())
	{
		Root->ResetDebugTarget();
		return true;
	}

	Root->SetDebugTarget(TargetName);
	return true;
}

TArray<int32> UChooserEvaluationToolset::GetChooserDebugSelectedRows(UChooserTable* Table)
{
	if (!PrepareTable(TEXT("GetChooserDebugSelectedRows"), Table))
	{
		return TArray<int32>();
	}
	return Table->GetDebugSelectedRows();
}
