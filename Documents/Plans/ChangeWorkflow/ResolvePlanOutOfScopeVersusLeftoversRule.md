<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-09T15:09:23.165Z","dependsOn":[]} -->
# Fix: Change Workflow Leftovers rule — no precedence against a Plan's out-of-scope residual wording, and no preparation-time application

## Context
Observed symptom: the claimed Plan
`Documents/Plans/Engine/NetworkServerOverEngineeringCleanup.md` says in
`## Out of scope` (line 94): "Any check, branch, or file not listed as a
candidate, including similar-looking sites the executor notices; report those
as residuals instead." The Leftovers paragraph of
`.agents/references/change-workflow.md` `#### Step 8 — Verify the acceptance
table` (line 158) says a small proven leftover, "such as the same defect at a
sibling site, is fixed inside the current change without asking".
`.agents/references/scope-authorization.md` (lines 7-18) counts scope added
under the Leftovers rule as authorized but also marks any region matching an
`## Out of scope` entry `unauthorized`, and no file states which wins.

In the run, the preparation worker routed the proven sibling at
`Engine/Source/Network/Client/ClientSessionRuntime.h:48` at baseline
`adec1d07` (the `kConnected` / `mpServerPeer == nullptr` check in
`SendGameRequest`) to
`/create-follow-up-plans`. Main carried it as a planned follow-up through two
Plan-review rounds and implementation, then launched `/create-follow-up-plans`,
stopped it, and folded the site in as candidate C24, together with three more
siblings `/plan-audit` found (C25-C27). Folding them in after implementation
cost a second full review, build, and harness cycle that applying the Leftovers
rule during preparation, before Plan review, would have avoided. The Leftovers
paragraph's only timing sentence covers leftovers routed to a follow-up Plan
("One proven before the Step 6 dispatch is authored first"); it says nothing
about applying the fix-now rule at preparation, and the `/next-plan` step-4
preparation brief (`.agents/skills/next-plan/SKILL.md` `## Steps` step 4) does
not ask for it.

The same out-of-scope wording appears in
`Documents/Plans/Engine/FileOverEngineeringCleanup.md` (line 77) and
`Documents/Plans/Engine/GraphicsOverEngineeringCleanup.md` (line 89), so the
conflict recurs when those Plans are claimed.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 488ec650-1b83-4287-8ec0-fe428cc8d548
- Worktree/branch UUID: 1da29a17-c83f-44c7-bead-da6ddce9072e
- Session branch: claude/1da29a17-c83f-44c7-bead-da6ddce9072e
- Worktree: .claude\worktrees\BrokenEngine\1da29a17-c83f-44c7-bead-da6ddce9072e
- Landing ref: claude/1da29a17-c83f-44c7-bead-da6ddce9072e (the observing
  session records and lands this Plan itself; the branch survives exactly as
  long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/ResolvePlanOutOfScopeVersusLeftoversRule.md`, but a periodic
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
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author's recommendation, for the fix session to confirm: state once, in the
Leftovers paragraph, which rule governs when a Plan's `## Out of scope` wording
routes a small proven sibling to residuals, and align
`scope-authorization.md` with that answer; then have preparation apply the
Leftovers rule to siblings it proves, so they enter the Plan review as approved
scope. Rationale: the observed cost came from the sibling being folded in after
implementation rather than before review. Which side should win is a decision
the fix session owns; this Plan does not make it.

## Critical files
- `.agents/references/change-workflow.md` — `#### Step 8 — Verify the
  acceptance table`, Leftovers paragraph
- `.agents/references/scope-authorization.md` — authorization source and
  pass 1
- `.agents/skills/next-plan/SKILL.md` — `## Steps` step 4 preparation brief

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the three sections named in
  `## Critical files`

## Out of scope
- The landed change the session produced
- Rewording the `## Out of scope` sections of existing Plans, including
  `FileOverEngineeringCleanup.md` and `GraphicsOverEngineeringCleanup.md`
- `/prepare-change`, `/plan-audit`, and `/create-follow-up-plans`, unless
  root-causing proves the fix needs them; surface that for re-planning
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: the Change Workflow's leftover and scope
authorization rules); escalate if the fix reaches build/bootstrap coordination.
The scope ceiling in `scope-authorization.md` stays enforceable: only a small
(Tier 1 or 2, bounded, decidable) proven leftover may enter scope. Never embed
transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  one rule states which governs when a Plan's out-of-scope wording conflicts
  with the Leftovers rule, `scope-authorization.md` agrees with it, and
  preparation applies it to siblings proven before Plan review
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
