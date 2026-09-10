<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T22:53:04.773Z","dependsOn":[]} -->
# Fix: Plan acceptance criteria — bare `Invoke-StaticChecks.ps1` form cannot run

## Context
Observed symptom. A preparation `implementer` in this session ran the static-check
command exactly as a claimed Plan's `## Acceptance criteria` states it,
`pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1`, and the script
refused to start with:

`Cannot process command because of one or more missing mandatory parameters: RepositoryRoot Baseline`

The worker had to abandon the Plan's form and substitute the documented
parameterized form from `.agents/references/static-checks.md:16`,
`pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot <worktree root> -Baseline <baseline SHA>`.
That reference is the only location in the repository outside `Documents/Plans/`
carrying the invocation, and it carries the runnable form; the bare form exists
only in Plan acceptance lines, where it was hand-written.

The unrunnable line is currently duplicated across five live Plans:

- `Documents/Plans/ChangeWorkflow/RunCheckpointTrims.md:157`
- `Documents/Plans/ChangeWorkflow/PlanStageTrims.md:164`
- `Documents/Plans/ChangeWorkflow/NextPlanReviewTrims.md:202`
- `Documents/Plans/ChangeWorkflow/CodeQualityMetricsSkillSkeleton.md:110`
- `Documents/Plans/ChangeWorkflow/CheckpointIsolationGoverningPathPreRead.md:115`

A sixth carrier, `Documents/Plans/ChangeWorkflow/DispatchBriefSectionReads.md:119`,
is deliberately excluded: that Plan was being rejected as obsolete when this
friction was recorded, so a fix session must skip it and any other carrier that
has since been completed or rejected.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: a38b9189-20c7-4333-b93f-26bac1ac2874
- Worktree/branch UUID: 8c2d32c8-71d6-4490-a4dc-9a4257501f8c
- Session branch: claude/8c2d32c8-71d6-4490-a4dc-9a4257501f8c
- Worktree: .claude\worktrees\BrokenEngine\8c2d32c8-71d6-4490-a4dc-9a4257501f8c
- Landing ref: branch `claude/8c2d32c8-71d6-4490-a4dc-9a4257501f8c`, the session
  branch above, which lands this Plan with this session's change
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
The symptom is fully diagnosed from the current tree: the script declares
`RepositoryRoot` and `Baseline` mandatory, `.agents/references/static-checks.md:16`
documents the parameterized form, and the five Plan lines above spell a form
that omits both parameters. No transcript is needed, so `/next-plan-review` is
not part of this fix.

Author's recommendation, and the smallest fix the author sees: in each of the
five Plan acceptance lines, replace the bare command with a citation of
`.agents/references/static-checks.md` as the owner of the runnable invocation,
keeping the rest of each acceptance sentence (which rows must pass) unchanged.
Rationale: the reference already owns the invocation, so citing it satisfies the
root `AGENTS.md` progressive-disclosure directive and cannot drift again if the
script's parameters change, whereas copying the parameterized form into five
Plans re-creates the same duplication that produced this defect.

For prevention, the author recommends one clause in the tooling-friction Plan
template's `## Acceptance criteria` block,
`.agents/skills/create-follow-up-plans/references/tooling-friction-plan-template.md:79-83`,
stating that a static-check acceptance line cites
`.agents/references/static-checks.md` rather than spelling a command, so a Plan
drafted from the template cannot reintroduce an unrunnable form. That template
is the emitter every future tooling-friction Plan copies; a rule added to
`Documents/Plans/AGENTS.md` instead would restate an invocation policy that root
`AGENTS.md` already owns for all bundled scripts, so the author recommends
against that location.

If root-causing shows the fix must reach `Documents/Plans/AGENTS.md`, root
`AGENTS.md`, or the script itself, surface it for re-planning instead of
expanding scope.

## Critical files
- `Documents/Plans/ChangeWorkflow/RunCheckpointTrims.md`
- `Documents/Plans/ChangeWorkflow/PlanStageTrims.md`
- `Documents/Plans/ChangeWorkflow/NextPlanReviewTrims.md`
- `Documents/Plans/ChangeWorkflow/CodeQualityMetricsSkillSkeleton.md`
- `Documents/Plans/ChangeWorkflow/CheckpointIsolationGoverningPathPreRead.md`
- `.agents/skills/create-follow-up-plans/references/tooling-friction-plan-template.md`

## In scope
- The single static-check acceptance-criterion line in each of the five Plans
  named under `## Critical files`, skipping any that no longer exists at fix
  time
- The `## Acceptance criteria` block of
  `.agents/skills/create-follow-up-plans/references/tooling-friction-plan-template.md`

## Out of scope
- `Documents/Plans/ChangeWorkflow/DispatchBriefSectionReads.md`
- `.agents/scripts/Invoke-StaticChecks.ps1` and its parameter contract
- `.agents/references/static-checks.md`, root `AGENTS.md`, and
  `Documents/Plans/AGENTS.md`
- Every other line of the five Plans, including their metadata markers
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (documentation text with no signature or invariant exposure);
escalate if the fix reaches root `AGENTS.md`, `Documents/Plans/AGENTS.md`, or the
script. Invariants: each edited Plan's byte-zero `broken-engine-plan/v1` marker
and `createdUtc` stay byte-for-byte unchanged; the invocation form keeps exactly
one owning location; no transcript path or home path is embedded.

## Acceptance criteria
- No file under `Documents/Plans/` states an `Invoke-StaticChecks.ps1`
  invocation lacking `-RepositoryRoot` and `-Baseline`
- A Plan drafted from the tooling-friction template carries a static-check
  acceptance line that cites `.agents/references/static-checks.md`
- The static-check runner, invoked as `.agents/references/static-checks.md`
  documents it, reports the `markdown-links` row passing and the
  `validate-skill` row passing for any changed package; `plan validate` exits 0
