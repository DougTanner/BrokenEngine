<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T23:10:56.526Z","dependsOn":[]} -->
# Fix: create-follow-up-plans tooling-friction template — Plans prescribe an argument-less `Invoke-StaticChecks.ps1` that always exits 1

## Context
`.agents/scripts/Invoke-StaticChecks.ps1:11-12` declares `-RepositoryRoot` and
`-Baseline` as `[Parameter(Mandatory)]`, so the argument-less form
`pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1` cannot run: under
`-NonInteractive` it exits 1 on the missing mandatory parameters instead of
emitting a `broken-engine-static-checks/v1` envelope. The only correct form in
the repository is the row at `.agents/references/static-checks.md:16`,
`pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot <worktree root> -Baseline <baseline SHA>`.

The argument-less form is nonetheless what Plans carry as an acceptance row. It
appeared in the Plan claimed by the recording session
(`Documents/Plans/ChangeWorkflow/CheckpointIsolationGoverningPathPreRead.md:115`,
deleted at that Plan's landing) and it is still live in six sibling Plans under
`Documents/Plans/ChangeWorkflow` (paths and lines in `## In scope`). The emitter
is `.agents/skills/create-follow-up-plans/references/tooling-friction-plan-template.md`,
whose `## Acceptance criteria` block prescribes the acceptance rows for every
tooling-friction Plan without citing `.agents/references/static-checks.md` for
the runner's invocation, so each Plan author spells a command out instead of
following the one documented form.

Cost in the recording session: two fix workers reached the acceptance row, found
the prescribed command unrunnable, and each had to deviate from the documented
acceptance command to produce a passing static-checks result.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: a9e74f3d-2914-4a8f-ad51-3a671350b4a6
- Worktree/branch UUID: 2f9bfe84-cdc3-4888-9c11-272dcd151967
- Session branch: claude/2f9bfe84-cdc3-4888-9c11-272dcd151967
- Worktree: .claude\worktrees\BrokenEngine\2f9bfe84-cdc3-4888-9c11-272dcd151967
- Landing ref: claude/2f9bfe84-cdc3-4888-9c11-272dcd151967
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref
above — supplying the recorded client and the recorded conversation session ID.
Then make the smallest fix inside the `## In scope` boundary below.

The author's recommendation for that fix, on the evidence above: in the
template's `## Acceptance criteria` block, replace the spelled-out static-checks
command with a citation of `.agents/references/static-checks.md` as the owner of
the runner's invocation, keeping the acceptance outcome (the runner's rows
passing for the changed artifacts) as the criterion. That keeps the single form
at its owning layer, so a later template change cannot drift from the script's
parameters again. The rationale for citing rather than copying the correct
command is that copying it reproduces exactly the drift this Plan records.
Separately, correct the acceptance line in each live sibling Plan named below to
the documented form so a fix session running one of them does not hit the same
dead end.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/create-follow-up-plans/references/tooling-friction-plan-template.md`
- `.agents/references/static-checks.md` (cited owner of the invocation; read, and
  changed only if the citation needs an anchor it does not yet carry)
- `Documents/Plans/ChangeWorkflow/CheckpointProjectionEmitsPaths.md`
- `Documents/Plans/ChangeWorkflow/CodeQualityMetricsSkillSkeleton.md`
- `Documents/Plans/ChangeWorkflow/DispatchBriefSectionReads.md`
- `Documents/Plans/ChangeWorkflow/NextPlanReviewTrims.md`
- `Documents/Plans/ChangeWorkflow/PlanStageTrims.md`
- `Documents/Plans/ChangeWorkflow/RunCheckpointTrims.md`

## In scope
- Root-cause investigation as `## Design` states
- The `## Acceptance criteria` block of
  `.agents/skills/create-follow-up-plans/references/tooling-friction-plan-template.md`:
  make it cite `.agents/references/static-checks.md` for the runner's invocation
  instead of spelling a command
- The single acceptance line naming `Invoke-StaticChecks.ps1` in each of these
  live Plans, corrected to the documented form:
  - `Documents/Plans/ChangeWorkflow/CheckpointProjectionEmitsPaths.md:123`
  - `Documents/Plans/ChangeWorkflow/CodeQualityMetricsSkillSkeleton.md:110`
  - `Documents/Plans/ChangeWorkflow/DispatchBriefSectionReads.md:119`
  - `Documents/Plans/ChangeWorkflow/NextPlanReviewTrims.md:202`
  - `Documents/Plans/ChangeWorkflow/PlanStageTrims.md:164`
  - `Documents/Plans/ChangeWorkflow/RunCheckpointTrims.md:157`

## Out of scope
- `.agents/scripts/Invoke-StaticChecks.ps1` itself: its mandatory parameters are
  the documented contract and are not to be relaxed or defaulted
- Every other section of the tooling-friction template, and every other line of
  the six Plans above
- The landed change the recording session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 1 — mechanical: documentation only, no C++ and no runtime or script
behavior change; the trigger is the changed instruction prose and Plan prose.
Escalate only if root-causing concludes the script's parameters must change,
which is out of scope above. Never embed transcript paths or home paths. Each
edited Plan keeps its byte-zero `broken-engine-plan/v1` marker and its
`createdUtc` unchanged.

## Acceptance criteria
- The template's `## Acceptance criteria` block names no static-checks command
  and cites `.agents/references/static-checks.md` for the invocation
- A search of `Documents/Plans/` and `.agents/` for
  `Invoke-StaticChecks.ps1` finds no occurrence lacking `-RepositoryRoot` and
  `-Baseline`
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports its `validate-skill`, `plan-scheduler`, and
  `markdown-links` rows passing for the changed artifacts
