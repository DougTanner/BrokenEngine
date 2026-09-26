<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T14:31:19.516Z","dependsOn":[]} -->
# Fix: next-plan step 6 — a sync that touches any cited path forces full re-preparation, and main reads the diff itself

## Context
Observed during a `/next-plan` run on an Engine Plan. At step 6 (final claim
refresh), the idempotent claim result carried a `sync` object
(`fastForwarded`, `20a3821b..96ee407b`). Diffing `sync.from..sync.to` against
the paths the preparation handoff cited intersected two of them:
`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md` and
`Projects/BrokenEngineSandbox/Documents/AgentHarness/cross-cell.md`, each with a
one-line edit unrelated to the claimed Plan.

`.agents/skills/next-plan/SKILL.md` step 6 states: "When they intersect,
return to step 4, rerun the affected Plan review checks in step 5, and repeat
this final refresh before approval". The rule has no content-relevance test:
any path intersection, however unrelated the change, requires full
re-preparation and rerunning Plan reviews. Following it would have repeated
preparation and reviews for an irrelevant one-line doc edit. Main instead
deviated from the step: it ran `git diff sync.from..sync.to -- <paths>` itself,
judged the change irrelevant, and continued.

That workaround also cost main context: the diff output (about 9,228
characters) entered the main session. A delegated worker returning one
affects/does-not-affect row per intersecting path would have kept the diff out
of main.

Both symptoms come from the same step-6 rule, so they are recorded together.
`.agents/skills/next-plan/references/claim-results.md`
`## Session baseline and the sync object` defines the `sync` object and the
baseline, but not the intersection reading; step 6 owns that reading.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 71906c29-36c2-4b49-a6f5-370b446822cb
- Worktree/branch UUID: 64ef46fd-8a52-4527-97d2-36b0fa86529b
- Session branch: claude/64ef46fd-8a52-4527-97d2-36b0fa86529b
- Worktree: .claude\worktrees\BrokenEngine\64ef46fd-8a52-4527-97d2-36b0fa86529b
- Landing ref: claude/64ef46fd-8a52-4527-97d2-36b0fa86529b (the observing
  session records and lands this Plan itself; the branch survives as long as
  the worktree above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/NextPlanSyncIntersectionRelevance.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the
recorded client and conversation session ID. Then make the smallest fix inside
the `## In scope` boundary below. If root-causing shows the fix lies outside
that boundary, surface it for re-planning instead of expanding scope.

The author recommends this direction, for the fix session to confirm. When the
claim result carries a `sync` object and the diff intersects cited paths, step
6 delegates the relevance judgment instead of treating any intersection as
disqualifying. Main dispatches one `researcher` (the `change-workflow.md`
delegation-table role for research requiring judgment), giving it
`sync.from..sync.to` and the intersecting cited paths. The worker returns one row
per path stating whether the change touches any statement the preparation
handoff or execution card relies on. Only a "touches" row returns the run to
step 4 and step 5 for the affected reviews; all "does not touch" rows let
the run proceed. Rationale: keeping the existing path-intersection trigger
avoids unnecessary dispatches when nothing intersects. A delegated relevance
test removes both symptoms at once: the forced re-preparation for unrelated
edits, and the raw diff entering main.

## Critical files
- `.agents/skills/next-plan/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/next-plan/SKILL.md`
  `## Steps` step 6 (the final claim refresh and its `sync` intersection rule)

## Out of scope
- The landed change the session produced
- `.agents/skills/next-plan/references/claim-results.md`, which defines the
  `sync` object and baseline but not the intersection reading
- The claim script and WorktreeCli `sync` reporting
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2, trigger: scoped tool behavior (one skill's workflow step
changes how a `/next-plan` run reacts to a `sync` result). Escalate if the fix
reaches build/bootstrap coordination. Invariant kept: preparation and Plan
review evidence still match the current tree before approval whenever a synced
change touches what they rely on. Never embed transcript paths or home paths.

## Acceptance criteria
- A final claim refresh whose `sync` diff intersects cited paths only with
  changes unrelated to the preparation evidence proceeds to approval without
  re-preparation, and without the diff text entering the main session.
- A synced change that touches a statement the preparation handoff or card
  relies on still returns the run to step 4 for the affected reviews.
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
