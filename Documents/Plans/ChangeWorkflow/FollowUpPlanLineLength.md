<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-29T14:56:31.305Z","dependsOn":[]} -->
# Fix: create-follow-up-plans — written Plans fail the static-checks line-length row

## Context
Observed symptom. During a `/next-plan` run, a `/create-follow-up-plans`
dispatch wrote and staged `Documents/Plans/Engine/CollisionFlightPlaneLayers.md`
with five lines of 518 to 925 characters. Main's later Change Workflow Step 5
run of
`pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot '<worktree root>' -Baseline <baseline SHA> -IncludeUntracked`
failed its `line-length` row, whose limit is 500 characters
(`.agents/scripts/Invoke-StaticChecks.ps1:37`), and main had to dispatch an
extra `implementer` to rewrap those lines, as
`.agents/references/change-workflow.md:170` directs.

Neither `.agents/skills/create-follow-up-plans/SKILL.md`, its
`references/worker.md`, nor its `references/tooling-friction-plan-template.md`
states a line-length limit or tells the worker to run the static-checks runner
on the Plan it writes; the template names the runner only as an acceptance
criterion of the future fix. The `plan validate` result that
`.agents/scripts/New-PlanFile.ps1` folds in checks scheduler validity, not
line length, so the worker's own done condition passed.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: be3ffb28-d3d7-4d8b-83f2-81a23f5d1939
- Worktree/branch UUID: 72ca68d0-0831-42bf-a368-d8e2ca9ab1ae
- Session branch: claude/72ca68d0-0831-42bf-a368-d8e2ca9ab1ae
- Worktree: .claude\worktrees\BrokenEngine\72ca68d0-0831-42bf-a368-d8e2ca9ab1ae
- Landing ref: claude/72ca68d0-0831-42bf-a368-d8e2ca9ab1ae, the session branch
  above, whose tip is that session's final commit and which survives exactly as
  long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a
  periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above. OpenCode transcript review remains unsupported regardless of worktree
  retention.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session
run `/next-plan-review <review ref>` in bounded friction mode — the landing ref
— supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author recommends one worker step rather than a restated limit: the worker
runs the static-checks runner on the Plan it wrote and rewraps any
`line-length` failure before handing off, so the 500-character limit keeps its
single owner in `Invoke-StaticChecks.ps1`.

## Critical files
- `.agents/skills/create-follow-up-plans/references/worker.md`
- `.agents/skills/create-follow-up-plans/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `references/worker.md`
  `### 2. Draft and classify` and `### 3. Apply exactly one case`, and, only if
  the new check adds a handoff row, `SKILL.md` `## Handoff`

## Out of scope
- The landed change the session produced
- `Invoke-StaticChecks.ps1`, `New-PlanFile.ps1`, and WorktreeCli `plan validate`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (documentation: one skill's worker steps); escalate to Tier 2
if the fix needs a script change. Never embed transcript paths or home paths.

## Acceptance criteria
- A Plan written by `/create-follow-up-plans` passes the static-checks
  runner's `line-length` row before the worker hands off, with no rewrap
  dispatch from main
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
