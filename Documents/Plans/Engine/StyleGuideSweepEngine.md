<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:04:54.975Z","dependsOn":[]} -->
# Style guide sweep: Engine stage, remaining batches

## Context

The `/sweep` skill's `style-guide` type brings every first-party C++ file up to the style guide in one pass, one stage per top-level area. Stages `Common/`, `DataPacker/` and `Tools/` landed at `9a12791d`. The `Engine/` stage's first three batches (`Engine_Source_Agent`, `Engine_Source_Network`, `Engine_Source_Frame`) have landed, together with this Plan's rescope; the user stopped the stage there. The remaining three batches are this Plan. They land before the `Projects/` stage (`Documents/Plans/Game/StyleGuideSweepProjects.md`) because `Projects/` consumes the Engine renames.

## Sweep

- Type: `style-guide`
- Targets: `Engine/*.h`, `Engine/*.cpp`, `:(exclude)Engine/Source/Agent/*`, `:(exclude)Engine/Source/Network/*`, `:(exclude)Engine/Source/Frame/*`
- Batches: `Engine/Source/Graphics`, `Engine/Source/Ui`

## Design

Run `/sweep` on this Plan, with its batches one after another in this order: `Engine_Source_Graphics`, `Engine_Source_Ui`, then `Engine` (the rest of `Engine/`). The Codex pipeline (eight units in parallel, detached coordinator), no commits before landing, and the SmartGit review are user-directed.

`/sweep` `## Rules` names the Change Workflow steps this stage skips and keeps.

Change Workflow tier: Tier 3. Trigger: the fixes rename and re-signature symbols across independently owned Engine subsystems (`Graphics`, `Ui` and the rest) and propagate them into `Projects/` and shaders. The `style-guide` type's `## Fix bound` keeps determinism/CRC, serialization, replay, wire, `.pack`, threading and trust-boundary behavior unchanged; a fix that would touch them is ledgered, not applied.

## Critical files

- `.agents/skills/sweep/` — the skill and its `style-guide` type (read only).
- Tracked `*.h` and `*.cpp` in the three remaining batches — 119 units (`Engine_Source_Graphics` 41, `Engine_Source_Ui` 41, `Engine` 37).
- `Documents/Investigations/ChangeWorkflow/StyleGuideSweepDeferredFixes.md` — gains the `## StyleGuideSweepEngine` section.

## In scope

- Every tracked `*.h` and `*.cpp` in the coordinator batches `Engine_Source_Graphics`, `Engine_Source_Ui` and `Engine`, swept under the `style-guide` type's `## Find rules` within its `## Fix bound`.
- Reference updates that propagation of a rename or signature change from those batches requires in `Common/`, `DataPacker/`, `Tools/`, `Projects/`, other `Engine/` directories, shaders, and AGENTS.md or docs naming the identifier.
- The `## StyleGuideSweepEngine` section of `StyleGuideSweepDeferredFixes.md`, holding the three batches' entries.

## Out of scope

- Re-sweeping the landed batches `Engine_Source_Agent`, `Engine_Source_Network` and `Engine_Source_Frame`, beyond the reference updates above.
- Sweeping files outside `Engine/` beyond the reference updates above; `Projects/` is the next Plan.
- `ThirdParty/`, and any edit under `.agents/` (Codex cannot write there; such findings go to the deferred-fixes file).
- Any out-of-bound fix per the `style-guide` type's `## Fix bound`, and any follow-up Plan for a deferred fix.
- Changes to the `/sweep` skill, the style guide, or the scanner.

## Acceptance criteria

- Every unit of the three batches has a `FIX-DONE` result and every batch a `BATCH-DONE` status; `Temp/Sweep/Failures.txt` is empty or each listed unit was re-run to completion.
- Client and Server Debug build after each batch, before the next batch starts.
- `Temp/Sweep/Cleanup.md` carries `CLEANUP-DONE`, and the `CLOSE-DONE` line reads `ordering=0` or each ordering finding has a `/resolve-findings` handoff.
- `Invoke-StaticChecks.ps1` against the stage baseline passes.
- Client and Server Debug and Release build; Profile builds when the stage edited `BT_PROFILE` code; DataPacker Release builds when a changed file compiles into it.
- The `/agent-harness` replay determinism check passes.
- The `## StyleGuideSweepEngine` section of `StyleGuideSweepDeferredFixes.md` lists every out-of-bound, declined, and unresolved finding of the three batches with `path:line`, rule, and reason.
- The stage lands through `/finalize-changes` only after the user confirms the finalizer's summary.
