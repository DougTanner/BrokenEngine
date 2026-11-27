<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-26T14:01:17.626Z","dependsOn":[]} -->
# Main-written worker briefs state the wanted behavior, not a code shape

## Context

`.agents/references/subagent-reporting.md` `## Task brief` defines the brief
form (`Objective`, `Scope`, `Fixed decisions`, and so on) but says nothing
about whether main may prescribe the code to write. In two changes that landed
in squash `81c17f92c`, the main session's brief dictated a code shape, and the
worker copied it:

- Fixing adversarial finding ADV002, main's `/resolve-findings` brief listed
  the `Engine/Source/GameBase.cpp` chat-eligibility conditions to replicate in
  `Engine/Source/Main.cpp`. The worker added a copy of that check instead of
  reusing the existing one.
- Main's implementer brief for a mid-review "Option 3" water-foam addition
  prescribed an `iWaterLowCount > 0` guard. The worker wrote it in C++ and in
  `Engine/Data/Shaders/Water/WaterDisplacement.comp:85` and `:136`, where it
  is redundant.

Neither defect came from a worker's own design; both followed the brief
literally. A brief that describes the required behavior leaves the worker's
reuse search and design judgment to decide the shape.

## Design

Add one rule to `## Task brief`: when main writes a brief for implementation
or fix work, `Objective`, `Scope`, and `Fixed decisions` state the behavior
the change must produce and the evidence, never code to replicate (conditions,
guards, or helper bodies); a code shape belongs in `Fixed decisions` only when
the user or the approved plan chose it. The author recommends placing it in
`## Task brief` rather than `change-workflow.md` `### User Interaction`,
because the Task brief section owns brief content and every dispatch reads it.

## Critical files

- `.agents/references/subagent-reporting.md`

## In scope

- `.agents/references/subagent-reporting.md` `## Task brief`: the one rule
  above, removing or rewriting any existing sentence it overlaps first.

## Out of scope

- The reuse search in `.agents/skills/resolve-findings/references/worker.md`
  (another change owns it).
- Deferral-option reasons in `change-workflow.md` `### User Interaction`.
- The style-guide Governing-paths rule for C++ and GLSL implementer briefs
  (`Documents/Plans/ChangeWorkflow/RequireCodeIdentifiersInImplementerComments.md`).

## Acceptance criteria

- `## Task brief` states that implementation and fix briefs describe the
  wanted behavior, and that a prescribed code shape appears only as a
  user- or plan-made `Fixed decisions` entry.

## Notes

- Change Workflow tier: Tier 2 (scoped workflow behavior: one shared
  reference). Judgment-only prose; no script can decide it.
