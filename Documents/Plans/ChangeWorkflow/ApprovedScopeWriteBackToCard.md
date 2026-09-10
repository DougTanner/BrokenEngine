<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T23:48:11.858Z","dependsOn":[]} -->
# Fix: /next-plan — approved re-scoping is never written back into the execution card workers cite

## Context
Observed symptom, in a `/next-plan` run. Main presented the resolved Plan for
approval with re-scoped decisions: a template edit dropped on a simplicity
finding, and one carrier Plan excluded because another session held it. The user
approved that re-scoped presentation. Main then dispatched the Tier-1 combined
`/coherence-review` with a brief naming the scratch execution card
`Temp/next-plan-prep-StaticCheckInvocationInPlanAcceptance.md` `## Execution card`
as "the approved resolution", while that scratch card still carried the
pre-approval scope. The reviewer returned `Criteria: 3/7 passed` with two
`Required` findings raised against the dropped template edit and the excluded
carrier Plan, and a `Residuals` row naming the card-versus-brief contradiction.
Main discarded those findings as measuring a stale card and edited the card only
after the review had run, so the re-scoped acceptance criteria were verified only
through the reviewer's incidental decisive checks rather than as its acceptance
table. Rework forced: a full Tier-1 combined review pass produced findings that
were entirely discarded, and the approved acceptance table went unverified as a
table.

Where the gap sits in the current tree: `.agents/skills/next-plan/SKILL.md` step
7 (`### Implementation approval`) requires only that a revision be presented as a
new complete replacement presentation, and step 8 says only "Implement the
approved change." Neither requires main to reconcile the execution card's scratch
file with the approved decisions before dispatching a worker whose brief cites
that card as authoritative. A repository read shows the generic Change Workflow
path shares the shape: `.agents/references/change-workflow.md` Step 1 has main
lock in the approved stage decisions while the card is a Step 2 preparation
artifact, and `/prepare-change` is preparation-only
(`.agents/skills/prepare-change/references/worker.md` `## Rules`), so nothing
updates the card after approval there either.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 9bc12973-0e5d-4d13-8b72-2dde79f6aea0
- Worktree/branch UUID: e4bf3a4e-d87f-4cf5-b045-a223a8fcd96d
- Session branch: claude/e4bf3a4e-d87f-4cf5-b045-a223a8fcd96d
- Worktree: .claude\worktrees\BrokenEngine\e4bf3a4e-d87f-4cf5-b045-a223a8fcd96d
- Landing ref: branch claude/e4bf3a4e-d87f-4cf5-b045-a223a8fcd96d, this
  session's own branch, which lands this Plan with the session's change.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a
  periodic Plan-history squash can make it return an unrelated aggregate commit,
  so review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying client
`claude` and the recorded conversation session ID. Then make the smallest fix
inside the `## In scope` boundary below.

Author's recommendation: add one clause at the `/next-plan` approval-to-
implementation transition requiring main, once approval carries decisions that
differ from the presented card, to update the execution card's scratch file to
the approved scope — or to record the approved deltas alongside it — before any
post-approval dispatch cites that card, so a cited card is never stale. Because
the generic Step 4 path shares the gap described in `## Context`, the author
recommends one parallel clause at the Change Workflow Step 4 site if the fix
session confirms the shared gap from a repository read; if root-causing instead
shows the fix belongs somewhere else, surface it for re-planning rather than
expanding scope.

## Critical files
- `.agents/skills/next-plan/SKILL.md` — step 7 `### Implementation approval` and
  step 8
- `.agents/references/change-workflow.md` — Step 4 dispatch site, only for the
  parallel clause if the shared gap is confirmed

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/next-plan/SKILL.md`
  step 7 `### Implementation approval` and step 8, plus, only when the fix
  session confirms the shared gap, one parallel clause at the Step 4 dispatch
  site in `.agents/references/change-workflow.md`

## Out of scope
- The landed change the observing session produced, and the reviews it ran
- Any change to `/prepare-change`, `/coherence-review`, or the reviewer handoff
  form
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. Never embed transcript paths or home paths.

## Acceptance criteria
- A post-approval dispatch cannot cite an execution card that still carries
  pre-approval scope: the changed text requires the card update before that
  dispatch, and a reader following it reaches the reconciled card
- /external-skill-creator validate mode passes wherever the Change Workflow
  Apply the triggered cleanup step triggers it; plan validate exits 0
