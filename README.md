# Chooser Toolset

MCP toolsets that let an AI assistant author Unreal Engine 5.8 **Chooser Tables** end to end —
creating a table, declaring what it reads and writes, adding rows and columns, filling cells,
nesting tables inside each other, and running the finished table to see which row it picks.

The plugin registers five toolsets with the engine's `ToolsetRegistry`, so they appear over the
built-in `ModelContextProtocol` server next to `EditorToolset`, `StateTreeToolset` and friends, plus
two Agent Skills that hand an assistant the authoring rules before it starts guessing.

## Why it exists

The Chooser plugin exposes no scripting API for its tables. `UChooserTable::ResultsStructs`,
`ColumnsStructs`, `DisabledRows` and `ContextData` are arrays of `FInstancedStruct` that Python and
`ObjectTools.set_properties` cannot reach, and the editor keeps them consistent by hand: every cell
array has to be resized with the row list, columns have to stay ordered filter → output → randomize,
and a column's property binding only resolves once it has been compiled against the table's
parameters. This plugin does that bookkeeping from C++, so an authored table opens correctly in the
chooser editor and evaluates the way the editor would.

## Requirements

- Unreal Engine **5.8**, editor builds only.
- Enabled engine plugins: `Chooser`, `ToolsetRegistry`, `ModelContextProtocol`.
- A C++ project — the plugin ships source and compiles with your editor target.

## Install

```
git clone https://github.com/iEclisse/ChooserToolset.git YourProject/Plugins/ChooserToolset
```

Then enable it in your `.uproject`:

```json
{ "Name": "ChooserToolset", "Enabled": true }
```

Regenerate project files and build the editor target. The toolsets register on
`OnAllModuleLoadingPhasesComplete`, so they are live as soon as the editor finishes loading.

> Editing the plugin later needs a full close-build-relaunch cycle. Live Coding compiles fine, but
> `ToolsetRegistry` caches the `UFunction` pointers it registered, and a patched module leaves every
> tool answering `Function "X" is no longer available`.

## Toolsets

43 tools across five toolsets.

| Toolset | Tools | What it covers |
| --- | --- | --- |
| `ChooserTableToolset` | 17 | The asset and its signature: create (generic or animation), list, describe, result type and result class, object and struct parameters with their read/write direction, the bindable properties of each parameter, nested tables, compile and validation, binding renames, stripping disabled data, opening and closing the chooser editor. |
| `ChooserRowToolset` | 6 | Rows and results: add, insert, replace, remove, move, duplicate, disable. Results can be an asset (hard or soft), a class, a nested table or another chooser asset, and row index -2 addresses the fallback row. |
| `ChooserColumnToolset` | 10 | Columns: list every available column type with its value kind and cell format, add (bound in one call), rebind, column-wide settings, the template cell new rows start from, remove, move, duplicate, disable, auto-populate. |
| `ChooserCellToolset` | 6 | Cells: read and write any cell as text, plus typed writes for the values text does not express well — enum value names, gameplay tags, object references and class references. |
| `ChooserEvaluationToolset` | 4 | Running the table: evaluate with supplied parameters and report the result, the row it came from and everything the output columns wrote, with an optional random seed. Plus the Play In Editor debug target: list, follow, and read the rows it selects. |

### Agent Skills

Two `UAgentSkill` subclasses, surfaced through `AgentSkillToolset.ListSkills` / `GetSkills`:

- **`ChooserAuthoringSkill`** — what a chooser table is, the order to build one in, how bindings and
  cells work, what an Output Struct column needs, how the caller's `Evaluate Chooser` node changes
  what the outputs mean, and what to do after every change.
- **`ChooserDebuggingSkill`** — an ordered checklist from validation to evaluation, plus
  symptom-to-cause pairs: always returns the first row, always returns null, outputs never arrive,
  outputs hold values from a row that did not win, randomize never varies.

## A table in six calls

```
Table  = ChooserTableToolset.CreateChooserTable("/Game/Choosers", "CT_Attacks", UAnimSequence)
Param  = ChooserTableToolset.AddChooserStructParameter(Table, FMyCombatState, Read)
Column = ChooserColumnToolset.AddChooserColumn(Table, FEnumColumn, Param, "Stance")
Row    = ChooserRowToolset.AddChooserRow(Table, AM_HeavyAttack)
         ChooserCellToolset.SetChooserEnumCell(Table, Column, Row, ["Aggressive"])
         ChooserTableToolset.ValidateChooserTable(Table)
```

Then `AssetTools.save_assets` writes it to disk, and `ChooserEvaluationToolset.EvaluateChooserTable`
runs it to confirm the row it picks:

```json
{"results": [{"refPath": "/Game/Anims/AM_HeavyAttack.AM_HeavyAttack"}],
 "selectedRows": [0],
 "outputs": {}}
```

## Conventions

- **Row -2 is the fallback row.** Every tool that takes a row index accepts it. It has cells only in
  output columns, because it passes no filters.
- **A new row starts permissive.** Each column keeps a template cell that a new row copies, and
  `AddChooserColumn` leaves it matching anything, so a row only gains a condition where a cell is
  filled in. `DescribeChooserTable` reports it as `DefaultCell`; `SetChooserColumnDefaultCell`
  changes it. Watch for this on tables authored in the editor, where an untouched enum cell means
  "equals the enum's first value" rather than "anything".
- **Cells are text.** `GetChooserCell` returns the Unreal text form of a cell and `SetChooserCell`
  takes it back, which covers every column type including ones from other plugins. The `CellFormat`
  of `ListChooserColumnTypes` shows the shape for a given column. The typed cell tools exist only
  where text is awkward or where a real reference is better than a path.
- **Parameters come before columns.** A column binds to one property of one parameter, so nothing can
  be bound until the parameter exists. `ListBindableProperties` reports every path that is bindable,
  with the value kind that says which column types accept it. An Output Struct column binds to the
  whole parameter — pass an empty `PropertyPath`, which is what gives it its struct type.
- **Asset lifecycle lives in `AssetTools`.** Saving, duplicating, renaming and deleting a chooser
  asset are not repeated here; use `AssetTools.save_assets`, `duplicate`, `move` and `delete`.
- **One result type is out of reach.** A Lookup Proxy row, which resolves a proxy asset through a
  proxy table, also needs a proxy table binding that only the chooser editor sets, so this toolset
  does not author it. Every other result type is covered.

## Tests

149 automation tests under `AI.Toolsets.ChooserToolset`, covering the success path and every failure
path of every tool. Run them from a live editor through `AutomationTestToolset`, or headless:

```
UnrealEditor-Cmd.exe YourProject.uproject -ExecCmds="Automation RunTests AI.Toolsets.ChooserToolset;quit" -Unattended -NullRHI
```

## License

MIT — see [LICENSE](LICENSE). Unreal Engine and its Chooser plugin are Epic Games' own and are not
covered by it.
