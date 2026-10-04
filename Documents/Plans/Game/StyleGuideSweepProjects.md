<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:04:58.740Z","dependsOn":["Documents/Plans/Engine/StyleGuideSweepEngine.md"]} -->
# Style guide sweep: Projects stage

## Context

The `/sweep` skill's `style-guide` type brings every first-party C++ file up to the style guide in one pass, one stage per top-level area. Stages `Common/`, `DataPacker/` and `Tools/` landed at `9a12791d`. The `Projects/` stage is the last; it runs after the `Engine/` stage (`Documents/Plans/Engine/StyleGuideSweepEngine.md`) because it consumes the Engine renames.

## Sweep

- Type: `style-guide`
- Targets: `Projects/*.h`, `Projects/*.cpp`
- Batches: `Projects/BrokenEngineSandbox/Source/Agent`, `Projects/BrokenEngineSandbox/Source/Frame`, `Projects/BrokenEngineSandbox/Source/Network`, `Projects/BrokenEngineSandbox/Source/Ui`

## Design

Run `/sweep` on this Plan, with its five batches one after another in this order: `Projects_BrokenEngineSandbox_Source_Agent`, `Projects_BrokenEngineSandbox_Source_Frame`, `Projects_BrokenEngineSandbox_Source_Network`, `Projects_BrokenEngineSandbox_Source_Ui`, then `Projects` (the rest of `Projects/`). The Codex pipeline (eight units in parallel, detached coordinator), no commits before landing, and the SmartGit review are user-directed.

`/sweep` `## Rules` names the Change Workflow steps this stage skips and keeps.

Change Workflow tier: Tier 3. Trigger: the fixes rename and re-signature symbols across independently owned game subsystems (`Agent`, `Frame`, `Network`, `Ui` and the rest of `Projects/BrokenEngineSandbox/Source/`) and propagate them into shaders. The `style-guide` type's `## Fix bound` keeps determinism/CRC, serialization, save, replay, wire, `.pack`, threading and trust-boundary behavior unchanged; a fix that would touch them is ledgered, not applied.

## Critical files

- `.agents/skills/sweep/` — the skill and its `style-guide` type (read only).
- Tracked `Projects/**/*.h` and `Projects/**/*.cpp` — 74 units, about 21,000 lines.
- `Documents/Investigations/ChangeWorkflow/StyleGuideSweepDeferredFixes.md` — gains the `## StyleGuideSweepProjects` section.

## In scope

- Every tracked `*.h` and `*.cpp` under `Projects/`, swept under the `style-guide` type's `## Find rules` within its `## Fix bound`.
- Reference updates that propagation of a `Projects/` rename or signature change requires in shaders and in AGENTS.md or docs naming the identifier.
- The `## StyleGuideSweepProjects` section of `StyleGuideSweepDeferredFixes.md`.

## Out of scope

- Sweeping files outside `Projects/` beyond the reference updates above.
- `ThirdParty/`, and any edit under `.agents/` (Codex cannot write there; such findings go to the deferred-fixes file).
- Any out-of-bound fix per the `style-guide` type's `## Fix bound`, and any follow-up Plan for a deferred fix.
- Changes to the `/sweep` skill, the style guide, or the scanner.

## Acceptance criteria

- Every `Projects/` unit has a `FIX-DONE` result and every batch a `BATCH-DONE` status; `Temp/Sweep/Failures.txt` is empty or each listed unit was re-run to completion.
- `Temp/Sweep/Cleanup.md` carries `CLEANUP-DONE`, and the `CLOSE-DONE` line reads `ordering=0` or each ordering finding has a `/resolve-findings` handoff.
- `Invoke-StaticChecks.ps1` against the stage baseline passes.
- Client and Server Debug and Release build; Profile builds when the stage edited `BT_PROFILE` code.
- The `/agent-harness` replay determinism check passes.
- The `## StyleGuideSweepProjects` section of `StyleGuideSweepDeferredFixes.md` lists every out-of-bound, declined, and unresolved finding with `path:line`, rule, and reason.
- The stage lands through `/finalize-changes` only after the user confirms the finalizer's summary.

## Notes

- Game save and settings code lives here; the `style-guide` type's `## Fix bound` treats any change to save bytes as out of bound.
