<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-16T16:50:46.792Z","dependsOn":[]} -->
# Fix: /next-plan — a repeat claim refresh after approval syncs the tree under a baseline run already started

## Context
Observed symptom: during a `/next-plan` run, the user approved a change (D2)
after the first approval. The run then repeated the step 6 claim refresh,
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan 'Documents/Plans/Engine/GameShellIntoGameBase.md'`.
Before that refresh ran, the baseline build and the baseline replay recording on
the unedited tree had already started. The claim result carried a `sync` object
and moved the worktree onto new primary commits (to `58476e7f`). That made the
baseline stale. Main stopped the replay, killed a leftover server process,
rebuilt the unedited tree, and recorded the baseline again.

Where the rule lives today: `.agents/skills/next-plan/SKILL.md` `## Steps`
step 6 runs the claim refresh "immediately before the final preparation
handoff", and repeats it when a touch is reported. The card template's
`Acceptance checks:` field in `### Execution card presentation/template` asks
for a run on a build of the unedited tree that is recorded before the first
edit. Neither step 6 nor step 8 says which comes first when a repeat refresh
follows approval while that pre-edit run is in flight, or that a `sync` makes
an already-started pre-edit run stale.

Related but not a duplicate: `Documents/Plans/ChangeWorkflow/PreEditAcceptanceChecks.md`
covers a pre-edit check that is never dispatched before the first slice. This
Plan covers a pre-edit run that was dispatched and then made stale by a later
claim `sync`. Whichever lands second keeps the two ordering rules consistent.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 9c0d16a4-ef3f-47e9-8c5f-9edb064a3199
- Worktree/branch UUID: 7ee6c7fe-e2a0-4081-a01c-477dd7fa8a34
- Session branch: claude/7ee6c7fe-e2a0-4081-a01c-477dd7fa8a34
- Worktree: .claude\worktrees\BrokenEngine\7ee6c7fe-e2a0-4081-a01c-477dd7fa8a34
- Landing ref: claude/7ee6c7fe-e2a0-4081-a01c-477dd7fa8a34 (the observing
  session records and lands this Plan itself).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/ClaimRefreshBeforePreEditBaseline.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
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
make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author recommends one ordering sentence at step 6. Any repeat of the claim
refresh, including one after approval, completes before any pre-edit
acceptance run (a build or recording of the unedited tree) starts. A pre-edit
run that already started when a refresh returns a `sync` object is stale and
is run again. Rationale: step 6 already owns when the refresh runs. Ordering
the refresh first costs nothing when there is no `sync`, and it avoids a
rebuild and re-recording when there is one.

## Critical files
- `.agents/skills/next-plan/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/next-plan/SKILL.md`
  `## Steps` steps 6 and 8

## Out of scope
- The landed change the session produced, including the GameShellIntoGameBase
  baseline rebuild and re-recording
- `Invoke-NextPlanClaim.ps1`, WorktreeCli claim behavior, and the `sync` object
- The pre-edit dispatch rule `Documents/Plans/ChangeWorkflow/PreEditAcceptanceChecks.md`
  owns
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one skill's step ordering); escalate if
the fix reaches build/bootstrap coordination. Never embed transcript paths or
home paths. A run with no repeat refresh and no pre-edit check gains no step.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  read of steps 6 and 8 shows that a repeat claim refresh completes before any
  pre-edit acceptance run starts, and that a `sync` result makes an
  already-started pre-edit run stale so it is run again
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
