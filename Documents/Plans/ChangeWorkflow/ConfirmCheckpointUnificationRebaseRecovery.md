<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-22T21:04:56.694Z","dependsOn":[]} -->
# Confirm Checkpoint Unification Rebase Recovery

## Context

Under standing landing confirmation, post-confirmation landing preparation for
unifying checkpoint transcript projection and context measurement hit
`primary.path-overlap` on
`Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` and
`rebase.patch-not-identical` after rebasing onto `main` commit
`ac80c001e6dfcece9759ddc6471849a0ac22101b` (`Move session change inventory into
shared module and remove raw stdout JSON mode`). Recovering from that rebase
changed bytes and meaning after user confirmation across two tracked files,
while the other eight paths in the landing set remained patch-identical:

- `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md:26-49`: the
  linear 3-way rebase onto `ac80c001e6dfcece9759ddc6471849a0ac22101b` exited
  `0` but silently mis-merged seven rows in `## Entries` (restoring three
  session-deleted rows covered by retained follow-up Plans, restoring the
  session-deleted `Invoke-Codex.ps1` `$routes` `Select-String` row already
  resolved by landed commit `adb2c09d6659920398b197d21e570d0205ade616`'s
  `-ListRoutes` change rather than a retained follow-up Plan, re-inserting one
  row deleted by `ac80c001e6dfcece9759ddc6471849a0ac22101b` and covered by
  `Documents/Plans/ChangeWorkflow/FixNextPlanSkillHostTruncation.md`,
  duplicating one existing row, and retaining one row semantically overlapping
  `Documents/Plans/ChangeWorkflow/FixNextPlanBriefInlineRepetition.md`).
  Removing those seven mis-merged rows preserved
  `ac80c001e6dfcece9759ddc6471849a0ac22101b`'s four additions (`:43-46`) and
  three deletions alongside the landing session's eight deletions and three
  additions (`:47-49`), leaving 24 valid rows with zero incident keys at three
  or more rows.
- `Documents/Plans/ChangeWorkflow/CitePlanReviewFindingsById.md` (`:1-2`,
  `:6-33`, `:72-97`, `:102-117`, `:124-126`): because
  `ac80c001e6dfcece9759ddc6471849a0ac22101b` landed
  `Documents/Plans/ChangeWorkflow/FixNextPlanBriefInlineRepetition.md` covering
  Tier 1-2 Plan review write-in, difference `/plan-simplicity-review`, and
  Step 4 pre-edit rules across `.agents/skills/next-plan/SKILL.md`,
  `.agents/references/change-workflow.md`, and
  `.agents/skills/plan-simplicity-review/SKILL.md`, recovery added
  `Documents/Plans/ChangeWorkflow/FixNextPlanBriefInlineRepetition.md` to
  metadata `dependsOn` at line `1` (preserving `createdUtc`), attributed the
  Tier-2 planner write-in incident in `## Context` to that prerequisite's Step 3
  work, and narrowed the title, `## Context`, `## Design`, `## Critical files`,
  `## In scope`, `## Out of scope`, `## Risk tier and invariants`,
  `## Acceptance criteria`, and `## Notes` exclusively to Tier-3
  `.agents/skills/next-plan/references/tier3-workflow.md` (`## Plan review`,
  `:23-39`).

Per `.agents/skills/finalize-changes/SKILL.md` `### Landing confirmation`, any
byte-changing post-confirmation recovery under standing confirmation files a
follow-up Plan before candidate re-preparation so the user can inspect and
confirm the recovered regions or request an adjustment. No defect is currently
known, and no determinism/CRC, serialization/`.pack`/`kiVersion`, replay, wire,
affinity, threading, allocation, shader, C++ build, or live `/agent-harness`
surface is affected.

## Unresolved decisions

Because the rebase recovery changed bytes and meaning after user confirmation,
the user decides whether to accept the recovered regions as landed or adjust
them:

- **Option 1 (Author recommendation):** Inspect and confirm the recovered
  regions in `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` and
  `Documents/Plans/ChangeWorkflow/CitePlanReviewFindingsById.md` with no
  further edits, completing this Plan via the empty-realized-change path in
  `.agents/skills/next-plan/SKILL.md` `## Rules`.
  - **Trade-offs:** Keeps the repaired 24-row incident ledger (zero keys at
    three or more rows) and the non-overlapping Tier-3 scoping and `dependsOn`
    edge of `CitePlanReviewFindingsById.md` intact with zero extra churn;
    assumes both recovery adjustments match user intent.
- **Option 2:** Specify a concrete adjustment to one or more of the recovered
  regions listed in `## In scope` (for example, restoring or removing specific
  rows in `WorkflowIncidents.md`, or revising the narrowed Tier-3 scope or
  `dependsOn` edge in `CitePlanReviewFindingsById.md`) and apply it in the
  executing session.
  - **Trade-offs:** Aligns the recovered files with a different user
    preference, at the cost of a scoped Tier-1 documentation/Plan edit and
    re-validation of the touched file(s).

Recommendation rationale: removing the seven mis-merged rows in
`WorkflowIncidents.md` kept the ledger accurate without re-introducing deleted
or duplicate entries, and narrowing `CitePlanReviewFindingsById.md` with a
`dependsOn` edge on `FixNextPlanBriefInlineRepetition.md` avoided two live
Plans editing the same Tier 1-2 files.

## Design

1. Present the two recovered regions in `## Critical files` and the two options
   in `## Unresolved decisions` to the user.
2. If the user confirms Option 1, make no edits and complete this Plan through
   the empty-realized-change path in `.agents/skills/next-plan/SKILL.md`
   `## Rules`.
3. If the user selects Option 2, apply only the user-specified adjustment
   within the regions named in `## In scope` (surfacing any request outside
   those regions for re-planning) and rerun the checks in
   `## Acceptance criteria`.

## Critical files

- `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md:26-49` —
  repaired `## Entries` table preserving
  `ac80c001e6dfcece9759ddc6471849a0ac22101b`'s four additions (`:43-46`) and
  three deletions plus the landing session's eight deletions and three
  additions (`:47-49`), totaling 24 valid rows with zero keys at three or more
  rows.
- `Documents/Plans/ChangeWorkflow/CitePlanReviewFindingsById.md` (`:1-2`,
  `:6-33`, `:72-97`, `:102-117`, `:124-126`) — added `dependsOn` edge on
  `Documents/Plans/ChangeWorkflow/FixNextPlanBriefInlineRepetition.md`,
  attributed the Tier-2 planner write-in incident to that prerequisite's Step 3
  work, and narrowed scope exclusively to Tier-3
  `.agents/skills/next-plan/references/tier3-workflow.md` (`## Plan review`,
  `:23-39`).

## In scope

Pending the user's answer to `## Unresolved decisions`, authorized edits are an
empty realized change (`.agents/skills/next-plan/SKILL.md` `## Rules`). If the
user selects Option 2, edits are bounded strictly to these regions:

- `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md`: `## Entries`
  rows at lines `26-49`.
- `Documents/Plans/ChangeWorkflow/CitePlanReviewFindingsById.md`: metadata
  `dependsOn` array at line `1` (never modifying `createdUtc`), title at line
  `2`, `## Context` lines `6-33`, `## Design` lines `72-77`,
  `## Critical files` lines `79-82`, `## In scope` lines `84-88`,
  `## Out of scope` lines `90-97`, `## Risk tier and invariants` lines
  `102-109`, `## Acceptance criteria` lines `111-117`, and `## Notes` lines
  `124-126`.

## Out of scope

- Any region of `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md`
  (lines `1-25`) or
  `Documents/Plans/ChangeWorkflow/CitePlanReviewFindingsById.md` (including
  `createdUtc` and lines `3-5`, `35-71`, `98-100`, `118-122`) outside the
  regions named in `## In scope`.
- The eight patch-identical paths from the landing commit
  (`.agents/skills/next-plan-checkpoint-review/` files,
  `.agents/skills/next-plan/references/run-checkpoint.md`,
  `Documents/Plans/ChangeWorkflow/BoundNextPlanDelegatedSkillReads.md`, and
  `Documents/Plans/ChangeWorkflow/DelegateReviewedFileReadsInFindingTriage.md`).
- `Documents/Plans/ChangeWorkflow/FixNextPlanBriefInlineRepetition.md`,
  `Documents/Plans/ChangeWorkflow/FixNextPlanSkillHostTruncation.md`, and other
  ChangeWorkflow skills, scripts, references, or Plans.
- Any C++, shader, Visual Studio project, or `WorktreeCli` file, and adding
  unit tests.

## Acceptance criteria

- Option 1 (no-edit confirmation): the user explicitly confirms the recovered
  regions in `## In scope`, no implementation edits are made to tracked files
  (only Plan completion's own deletion of this Plan and removal of any
  dependency edges pointing to it), and the Plan completes through
  `.agents/skills/next-plan/SKILL.md` `## Rules`'s empty-realized-change path.
- Option 2 (user-specified adjustment): only regions in `## In scope` change,
  `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` keeps zero
  incident keys at three or more rows if edited,
  `pwsh -NoProfile -File .agents/scripts/Invoke-PlanSchedulerStateCheck.ps1`
  reports `status: valid` and `code: ok` if
  `Documents/Plans/ChangeWorkflow/CitePlanReviewFindingsById.md` is edited, and
  the static-checks runner invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it passes every
  triggered row.

## Notes

- Change Workflow risk tier: Tier 1 (mechanical — empty realized change under
  the recommended Option 1 confirmation, or documentation and Plan-metadata
  edits to `WorkflowIncidents.md` and `CitePlanReviewFindingsById.md` under
  Option 2, with no public signature, runtime/tool behavior, determinism/CRC,
  wire/protocol, serialization/`.pack`/`kiVersion`, save/replay, threading,
  trust-boundary, or build/bootstrap coordination exposure).
- Originating event: standing-landing confirmation post-confirmation
  `rebase.patch-not-identical` recovery onto
  `ac80c001e6dfcece9759ddc6471849a0ac22101b` per
  `.agents/skills/finalize-changes/SKILL.md` `### Landing confirmation`.
- Dependencies: `dependsOn` stays empty (`[]`) per `Documents/Plans/AGENTS.md`
  `## Plan files` so the Plan is immediately eligible for bare `/next-plan`
  selection; no reciprocal `## Coordination` edges apply.
- Live `/agent-harness` verification does not apply.
