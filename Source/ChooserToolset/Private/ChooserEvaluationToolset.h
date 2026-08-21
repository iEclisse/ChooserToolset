// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#pragma once

#include "ChooserToolsetTypes.h"
#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "ChooserEvaluationToolset.generated.h"

class UChooserTable;

/**
 * Runs chooser tables and inspects what they picked, both from the editor and against a running game.
 *
 * EvaluateChooserTable is the way to check a table actually behaves: it supplies the parameters, runs
 * the same code the game runs, and reports the result, the row it came from and anything the output
 * columns wrote. The debug tools cover the other case, a table already being evaluated in Play In
 * Editor, where the chooser records which rows each object selected.
 */
UCLASS(BlueprintType)
class UChooserEvaluationToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Evaluates a table against the given parameters and reports what it selected.
	 * @param Table The table to evaluate.
	 * @param Parameters One entry per table parameter the evaluation should supply. Parameters left out
	 *                   are passed as a null object or a default struct.
	 * @param bReturnAllMatches True to collect every row that passes instead of stopping at the first.
	 *                           Output columns then run once per matching row into the same struct, so
	 *                           Outputs reports what the last of them wrote.
	 * @param RandomSeed Seed for the Randomize column, so a table using one gives a repeatable answer.
	 *                   Negative uses the global random stream.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Evaluation")
	static FChooserToolsetEvaluationResult EvaluateChooserTable(
		UChooserTable* Table,
		const TArray<FChooserToolsetEvaluationParameter>& Parameters,
		bool bReturnAllMatches = false,
		int32 RandomSeed = -1);

	/**
	 * Lists the objects that recently evaluated this table, as the chooser editor's debug target menu
	 * shows them. Only objects from a running Play In Editor session appear.
	 * @param Table The table to inspect.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Evaluation")
	static TArray<FString> ListChooserDebugTargets(UChooserTable* Table);

	/**
	 * Follows one object's evaluations of a table, so GetChooserDebugSelectedRows reports the rows it
	 * selects.
	 * @param Table The table to watch.
	 * @param TargetName Name from ListChooserDebugTargets. Empty stops following anything.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Evaluation")
	static bool SetChooserDebugTarget(UChooserTable* Table, const FString& TargetName);

	/**
	 * Rows the table selected the last time it was evaluated, either by the debug target or by
	 * EvaluateChooserTable. A single entry of -2 means every row failed and the fallback row was used.
	 * @param Table The table to inspect.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser|Evaluation")
	static TArray<int32> GetChooserDebugSelectedRows(UChooserTable* Table);
};
