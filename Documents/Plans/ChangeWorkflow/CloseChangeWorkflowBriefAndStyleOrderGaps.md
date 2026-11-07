<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T01:50:17.850Z","dependsOn":[]} -->
# Fix: Change Workflow — alternatives briefs and style re-runs collide with their workers

## Context

Two symptoms from one `/next-plan` run, both emitted by
`.agents/references/change-workflow.md`.

Observed symptom 1, `/plan-alternatives` evidence paths:
`.agents/references/change-workflow.md:142-143` has main put evidence paths under
`Governing paths:` in each `/plan-alternatives` brief, while
`.agents/skills/plan-alternatives/references/worker.md:52-57` bars the gitignored
scratch tree from a researcher's search and never accepts a path there as
evidence. On a claimed Plan, preparation evidence lives in that tree, including
the resolved Plan snapshot `.agents/skills/plan-alternatives/SKILL.md:146-150`
describes. In the observed run, both researchers reported the conflict and one
read the barred file. The cost was two contradiction reports and one read
against the worker rule.

Observed symptom 2, `/code-style-review` beside a re-review: during a
`/resolve-findings` round, `/code-style-review`, which may edit, was dispatched in
parallel with a focused `/repo-code-review` re-review of the same range. The
style pass made one small loop rewrite, and the reviewer returned BLOCKED on an
unknown writer and a changed hash, as
`.agents/skills/repo-code-review/SKILL.md:74-75` directs for a moving diff
boundary. `.agents/references/change-workflow.md:184` states when style re-runs
before `Build required`, and the Step 6 `Order:` paragraph at `:201-216` orders
style before the reviews only after an `/over-engineering-review` validator
change (`:209-214`). Neither gives a `/resolve-findings` round's style re-run an
order against that round's re-review. The cost was a re-review blocked on a
writer main's own dispatch had started.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: a1e5fa3a-8a30-4a1b-9c7f-3c6b5de2f177
- Worktree/branch UUID: 2ea16005-f59e-4086-a845-0c0236be3a46
- Session branch: claude/2ea16005-f59e-4086-a845-0c0236be3a46
- Worktree: .claude\worktrees\BrokenEngine\2ea16005-f59e-4086-a845-0c0236be3a46
- Landing ref: claude/2ea16005-f59e-4086-a845-0c0236be3a46
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CloseChangeWorkflowBriefAndStyleOrderGaps.md`,
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
the `## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

For symptom 1, the author recommends that the `/plan-alternatives` bullet limit
`Governing paths:` to paths the researcher may read, and have main restate a fact
that exists only in the gitignored scratch tree as text in the brief, under that
bullet's existing exclusions. The rationale is that the worker rule keeps
researchers blind to the Plan snapshot in that tree, so the brief recipe is the
side to change.

For symptom 2, the author recommends one sentence in the Step 6 `Order:`
paragraph: when a `/resolve-findings` round changed C++, `/code-style-review`
over the newly changed ranges finishes before that round's re-review starts. The
rationale is that this mirrors the style-then-review order the same paragraph
already gives a validator change, and that a reviewer whose range stopped moving
has no writer to report.

## Critical files

- `.agents/references/change-workflow.md`

## In scope

- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `/plan-alternatives` bullet of
  `#### Step 2 — Prepare and explore alternatives` and the `Order:` paragraph of
  `#### Step 6 — Review and resolve correctness` in `change-workflow.md`

## Out of scope

- The landed change the session produced
- The researcher rule in `.agents/skills/plan-alternatives/references/worker.md`
  and the `BLOCKED` rule in `.agents/skills/repo-code-review/SKILL.md`
- The `builder` bullets of Step 4 and Step 5, and any order between the style
  re-run's build and the re-review
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2. Trigger: one subsystem's tool behavior, namely what a
`/plan-alternatives` brief carries and when `/code-style-review` runs in a
`/resolve-findings` round. Escalate to Tier 3 if the fix reaches build/bootstrap
coordination that can block other sessions. No determinism, CRC, serialization,
replay, wire, threading, affinity, allocation, or shader exposure. Researchers
stay blind to the Plan rationale, Plan scope sections, and Plan file path. Never
embed transcript paths or home paths.

## Acceptance criteria

- A `/plan-alternatives` brief written from the Step 2 bullet cites no path
  `.agents/skills/plan-alternatives/references/worker.md` `## Rules` bars, and
  the bullet states how a fact held only in the gitignored scratch tree reaches
  the researcher
- The Step 6 `Order:` paragraph states that `/code-style-review` over a
  `/resolve-findings` round's newly changed ranges finishes before that round's
  re-review starts
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
