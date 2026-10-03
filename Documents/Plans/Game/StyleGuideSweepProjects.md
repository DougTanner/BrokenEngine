<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:04:58.740Z","dependsOn":["Documents/Plans/Engine/StyleGuideSweepEngine.md"]} -->
# Style guide sweep: Projects stage

## Context

`Documents/Investigations/ChangeWorkflow/StyleGuideWholeFileSweep.md` (the runbook) brings every first-party C++ file up to the style guide in one pass, one stage per top-level area. Stages `Common/`, `DataPacker/` and `Tools/` landed at `9a12791d`. The `Projects/` stage is the last; it runs after the `Engine/` stage (`Documents/Plans/Engine/StyleGuideSweepEngine.md`) because it consumes the Engine renames.

## Design

Run the runbook for the `Projects/` stage only, in this order:

1. `## Setup` — write the appendix prompts and coordinator to `Temp/StyleSweep/`, open SmartGit on the worktree, and confirm the batch list with `-List`. Record the primary SHA at the start as the stage baseline.
2. `## Phase 1 — per-unit pipeline` and `## Phase 2 — per-batch close` for each of the five `Projects/` batches, one after another: `Projects_BrokenEngineSandbox_Source_Agent`, `Projects_BrokenEngineSandbox_Source_Frame`, `Projects_BrokenEngineSandbox_Source_Network`, `Projects_BrokenEngineSandbox_Source_Ui`, then `Projects` (the rest of `Projects/`). The Codex pipeline (Luna `xhigh` read-only FIND, Sol `high` workspace-write FIX, Sol `high` PROPAGATE, eight units in parallel, detached coordinator), no commits before landing, and the SmartGit review are user-directed.
3. `## Phase 3 — per-stage close and landing`: cleanup pass, ordering review, static checks, `/update-claude-docs`, builds, replay determinism, deferred-fixes section, landing.

The runbook's `## Relation to the Change Workflow` names the Change Workflow steps this stage skips and keeps.

Change Workflow tier: Tier 3. Trigger: the fixes rename and re-signature symbols across independently owned game subsystems (`Agent`, `Frame`, `Network`, `Ui` and the rest of `Projects/BrokenEngineSandbox/Source/`) and propagate them into shaders. The runbook's `## Fix bound` keeps determinism/CRC, serialization, save, replay, wire, `.pack`, threading and trust-boundary behavior unchanged; a fix that would touch them is ledgered, not applied.

## Critical files

- `Documents/Investigations/ChangeWorkflow/StyleGuideWholeFileSweep.md` — the runbook (read only).
- Tracked `Projects/**/*.h` and `Projects/**/*.cpp` — 74 units, about 21,000 lines.
- `Documents/Investigations/ChangeWorkflow/StyleGuideSweepDeferredFixes.md` — gains the stage's section.

## In scope

- Every tracked `*.h` and `*.cpp` under `Projects/`, swept under the runbook's `## Scope` rules within `## Fix bound`.
- Reference updates that propagation of a `Projects/` rename or signature change requires in shaders and in AGENTS.md or docs naming the identifier.
- The `/update-claude-docs` updates for renamed or re-signatured identifiers.
- A new `Projects/` section in `StyleGuideSweepDeferredFixes.md`, with `Projects/` added to its stages-covered line.

## Out of scope

- Sweeping files outside `Projects/` beyond the reference updates above.
- `ThirdParty/`, and any edit under `.agents/` (Codex cannot write there; such findings go to the deferred-fixes file).
- Any out-of-bound fix per `## Fix bound`, and any follow-up Plan for a deferred fix.
- Changes to the runbook, the style guide, or the scanner.

## Acceptance criteria

- Every `Projects/` unit has a `FIX-DONE` result and every batch a `BATCH-DONE` status; `Failures.txt` is empty or each listed unit was re-run to completion.
- The cleanup checklist `Temp/StyleSweep/Cleanup-Projects.md` exists and the ordering review reports no unresolved finding.
- `Invoke-StaticChecks.ps1` against the stage baseline passes.
- Client and Server Debug and Release build; Profile builds when the stage edited `BT_PROFILE` code.
- The `/agent-harness` replay determinism check passes.
- `StyleGuideSweepDeferredFixes.md` carries a `Projects/` section listing every out-of-bound, declined, and unresolved finding with `path:line`, rule, and reason.
- The stage lands through `/finalize-changes` only after the user confirms the finalizer's summary.

## Notes

- Optional prerequisite: `Documents/Plans/ChangeWorkflow/StyleRule17ScannerSsize.md`. Until it lands, the scanner's `style-rule-17` rows encode the old `size_t` rule and the FIND prompt rejects them; the sweep runs correctly either way, so it is not a `dependsOn` edge.
- Game save and settings code lives here; the runbook's `## Fix bound` treats any change to save bytes as out of bound.
