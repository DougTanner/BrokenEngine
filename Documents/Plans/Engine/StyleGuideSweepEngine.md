<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:04:54.975Z","dependsOn":[]} -->
# Style guide sweep: Engine stage, remaining batches

## Context

`Documents/Investigations/ChangeWorkflow/StyleGuideWholeFileSweep.md` (the runbook) brings every first-party C++ file up to the style guide in one pass, one stage per top-level area. Stages `Common/`, `DataPacker/` and `Tools/` landed at `9a12791d`. The `Engine/` stage's first three batches (`Engine_Source_Agent`, `Engine_Source_Network`, `Engine_Source_Frame`) have landed, together with this Plan's rescope; the user stopped the stage there. The remaining three batches are this Plan. They land before the `Projects/` stage (`Documents/Plans/Game/StyleGuideSweepProjects.md`) because `Projects/` consumes the Engine renames.

## Design

Run the runbook for the remaining `Engine/` batches only, in this order:

1. `## Setup` — write the appendix prompts and coordinator to `Temp/StyleSweep/`, open SmartGit on the worktree, and confirm the batch list with `-List`. Record the primary SHA at the start as the stage baseline. Apply the prompt corrections in `## Notes` to the temporary copies only.
2. `## Phase 1 — per-unit pipeline` and `## Phase 2 — per-batch close` for each remaining batch, one after another: `Engine_Source_Graphics`, `Engine_Source_Ui`, then `Engine` (the rest of `Engine/`). The Codex pipeline (eight units in parallel, detached coordinator), no commits before landing, and the SmartGit review are user-directed.
3. `## Phase 3 — per-stage close and landing`: cleanup pass, ordering review, static checks, `/update-claude-docs`, builds, replay determinism, deferred-fixes section, landing.

The runbook's `## Relation to the Change Workflow` names the Change Workflow steps this stage skips and keeps.

Change Workflow tier: Tier 3. Trigger: the fixes rename and re-signature symbols across independently owned Engine subsystems (`Graphics`, `Ui` and the rest) and propagate them into `Projects/` and shaders. The runbook's `## Fix bound` keeps determinism/CRC, serialization, replay, wire, `.pack`, threading and trust-boundary behavior unchanged; a fix that would touch them is ledgered, not applied.

## Critical files

- `Documents/Investigations/ChangeWorkflow/StyleGuideWholeFileSweep.md` — the runbook (read only).
- Tracked `*.h` and `*.cpp` in the three remaining batches — 119 units (`Engine_Source_Graphics` 41, `Engine_Source_Ui` 41, `Engine` 37).
- `Documents/Investigations/ChangeWorkflow/StyleGuideSweepDeferredFixes.md` — its `## Engine/` section gains the remaining batches' entries.

## In scope

- Every tracked `*.h` and `*.cpp` in the coordinator batches `Engine_Source_Graphics`, `Engine_Source_Ui` and `Engine`, swept under the runbook's `## Scope` rules within `## Fix bound`.
- Reference updates that propagation of a rename or signature change from those batches requires in `Common/`, `DataPacker/`, `Tools/`, `Projects/`, other `Engine/` directories, shaders, and AGENTS.md or docs naming the identifier.
- The `/update-claude-docs` updates for renamed or re-signatured identifiers.
- Entries for the remaining batches in the `## Engine/` section of `StyleGuideSweepDeferredFixes.md`, with those batches added to its stages-covered line.

## Out of scope

- Re-sweeping the landed batches `Engine_Source_Agent`, `Engine_Source_Network` and `Engine_Source_Frame`, beyond the reference updates above.
- Sweeping files outside `Engine/` beyond the reference updates above; `Projects/` is the next Plan.
- `ThirdParty/`, and any edit under `.agents/` (Codex cannot write there; such findings go to the deferred-fixes file).
- Any out-of-bound fix per `## Fix bound`, and any follow-up Plan for a deferred fix.
- Changes to the runbook, the style guide, or the scanner.

## Acceptance criteria

- Every unit of the three batches has a `FIX-DONE` result and every batch a `BATCH-DONE` status; `Failures.txt` is empty or each listed unit was re-run to completion.
- Client and Server Debug build after each batch, before the next batch starts.
- The cleanup checklist `Temp/StyleSweep/Cleanup-Engine.md` exists and the ordering review reports no unresolved finding.
- `Invoke-StaticChecks.ps1` against the stage baseline passes.
- Client and Server Debug and Release build; Profile builds when the stage edited `BT_PROFILE` code; DataPacker Release builds when a changed file compiles into it.
- The `/agent-harness` replay determinism check passes.
- `StyleGuideSweepDeferredFixes.md` lists every out-of-bound, declined, and unresolved finding of the three batches with `path:line`, rule, and reason.
- The stage lands through `/finalize-changes` only after the user confirms the finalizer's summary.

## Notes

- Prompt corrections for the temporary copies: the scanner's `style-rule-17` rows now follow the current rule, so the FIND prompt adjudicates them instead of rejecting them; the FIND prompt's anonymous-namespace ruling follows current rule 66 (no anonymous namespace; file-local functions and non-const variables become `static`).
- The first three batches ran every FIND, FIX and PROPAGATE step on `gpt-6.1-sol` reasoning effort `high`, by user choice for that run.
- Propagation failure modes seen in the first three batches, each caught by the per-batch Debug build: a member rename applied to a DirectXMath type that has no such member, a rename that left callers of a removed member unchanged, and a propagation helper script that wrote stray text before the first line of seven files. A failure in the precompiled header hides every later error, so rebuild after each repair.
- Local data mode follows `.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`; propagated edits can reach shaders, `DataPacker/**` and `Engine/Data/**`. The runbook's Phase 2 authorizes Local generation, Gaea export still forbidden.
