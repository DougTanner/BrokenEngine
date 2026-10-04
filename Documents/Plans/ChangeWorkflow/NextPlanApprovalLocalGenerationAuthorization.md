<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-04T15:39:18.024Z","dependsOn":[]} -->
# Fix: /next-plan approval — Local generation authorization is not surfaced for a change that hits a Local data-mode trigger

## Context
Observed symptom: a `/next-plan` run that claimed
`Documents/Plans/Game/OneCppPerClass.md` changed
`DataPacker/Source/ExportJobs/ExportShader.cpp` (and deleted
`ExportShaderDependencies.cpp`), a `DataPacker/**` path that
`.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`
lists as a Local trigger. `.agents/skills/compile/SKILL.md` `## Inputs` says a
change hitting a Local trigger makes Local mandatory, "so request the Local
generation authorization at plan approval, before the first
BrokenEngineSandbox build". Neither the `/next-plan` execution card
(`.agents/skills/next-plan/SKILL.md` `### Execution card presentation/template`)
nor its approval presentation (`### Implementation approval`) has a place that
surfaces that need, so approval went out without the request. After approval,
the client `/compile` blocked with `data-mode.local-output-missing`; main had
to go back to the user for Local generation authorization and dispatch a
second builder, an extra user round trip.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: f8588a3d-3d69-46e9-b773-e4506a6f3b5a
- Worktree/branch UUID: 38e0f674-d9b5-4b35-b12a-a9c583aa6d5e
- Session branch: claude/38e0f674-d9b5-4b35-b12a-a9c583aa6d5e
- Worktree: .claude\worktrees\BrokenEngine\38e0f674-d9b5-4b35-b12a-a9c583aa6d5e
- Landing ref: claude/38e0f674-d9b5-4b35-b12a-a9c583aa6d5e, whose tip is that
  session's final commit and which survives exactly as long as the worktree
  recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/NextPlanApprovalLocalGenerationAuthorization.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above. OpenCode transcript review remains unsupported regardless of worktree
  retention.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref —
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. The author
recommends, as the likely smallest fix, that the preparation surface a Local
data-mode trigger hit in the card and that `### Implementation approval` carry
the Local generation authorization request among the decisions it presents,
referencing `/compile`'s rule rather than restating the trigger list, because
`runtime-data-mode.md` `## Mode selection` already owns that list. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/next-plan/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/next-plan/SKILL.md`
  `### Execution card presentation/template` and `### Implementation approval`,
  plus the step 4 preparation brief text in `## Steps` only if the card field
  needs the preparation to fill it

## Out of scope
- The landed change the session produced
- `/compile`, `runtime-data-mode.md`, and the data-mode selection logic
- `/prepare-change` and the general Change Workflow approval route
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one skill's approval workflow); escalate
if the fix reaches build/bootstrap coordination. Never embed transcript paths
or home paths. Local generation stays explicitly user-authorized; the fix must
never infer that authorization from approval of a Plan that does not grant it.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  claimed Plan whose change hits a `## Mode selection` Local trigger reaches
  the approval presentation with the Local generation authorization request,
  so the first BrokenEngineSandbox `/compile` after approval does not block
  with `data-mode.local-output-missing` for lack of authorization
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
Recorded from the `/next-plan` run checkpoint (`/next-plan-checkpoint-review`)
of the run that claimed `Documents/Plans/Game/OneCppPerClass.md`. The
misbehaving skill is outside that Plan's `## In scope`. No live Plan covered
this skill and symptom when this Plan was written. No dependencies or
Coordination constraints.
