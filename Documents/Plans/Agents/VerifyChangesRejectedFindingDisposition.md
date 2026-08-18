<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T12:05:33.453Z","dependsOn":[]} -->
# Fix: /verify-changes — a rejected Required review finding leaves no auditable record

## Context

Observed symptom, from a `/next-plan-review` of an earlier landed session: a
`/repo-code-review` dispatch returned `NEEDS_ACTION` with one Required finding
at `Tools/WorktreeCli/PlanScheduler.cpp:601` (cycle classification for newly
admitted descendant session trees) at `19:30:22.616Z`. The next visible manager
action, at `19:31:18.600Z`, dispatched only an unrelated documentation fix and
the style review. The finding was never implemented, and no rejection rationale
appears in any durable record. The landing-gate `/verify-changes` pass then
reported no unresolved accepted findings, which is true of accepted findings and
silent about the rejected one, so the landed record cannot show why a Required
finding did not land.

Current behavior, read from the working tree:

- `.agents/skills/verify-changes/SKILL.md` `## Required inputs` (`:19-33`) asks
  the manager for "concise ordered handoffs for ... reviews", with no
  per-finding disposition.
- Its `## Acceptance table` (`:91-95`) reconciles each review as
  `review | delegated/inline | findings | accepted-fixed | refuted | unresolved |
  evidence` and requires zero unresolved *accepted* findings. `refuted` is a bare
  count with no reason attached to any individual finding.
- The root `AGENTS.md` Change Workflow Step 5 makes rejection routine ("Manager
  decides which Sol findings to accept ... rejecting speculative findings ... is
  the default"), so a rejected Required finding is an expected, frequent event
  whose rationale currently lives only in the manager's unlogged reasoning.

This Plan records auditability only. It takes no position on whether the
`PlanScheduler.cpp:601` rejection was correct, and it creates no obligation to
implement any rejected finding.

The misbehaving surface is `/verify-changes`' evidence contract, outside the
`## In scope` boundary of the Plan the observing session had claimed, so this is
tooling friction rather than an in-scope acceptance failure of the change that
session landed.

Verify every cited line number against the working tree before editing — the
numbers above may have moved since.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a ref
whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 1b583079-2807-42e9-930f-2394560b2edc
- Worktree/branch UUID: 9210ba0b-3bb1-4251-80be-0a74eef865cc
- Session branch: claude/9210ba0b-3bb1-4251-80be-0a74eef865cc
- Worktree: .claude\worktrees\BrokenEngine\9210ba0b-3bb1-4251-80be-0a74eef865cc
- Observed in an earlier session: the six fields above are the observing
  session's, not the recording session's. That session's landed commit is
  `44b1259d50eaf4582bae5e25d5e35386abc3619b` ("Accept landing-candidate sessions
  in plan list and rename quarantined to excluded"), a single-session landing
  commit whose tree contains that session's work.
- Landing ref: `claude/a9c4f4cc-6cd6-4bf1-8511-9702c6308d1f`, the recording
  session's branch, whose tip is that session's final commit. Fallback once that
  ref is gone: `git log --diff-filter=A --format=%H -- <this plan path>`, but a
  periodic Plan-history squash can make it return an unrelated aggregate commit,
  so review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.

## Design

The `/next-plan-review` that produced this residual has already run and proved
the symptom above, so no further transcript review is required before
implementing. The provenance block is retained only so an implementer who needs
more context can reach the observing session while it is still available.

Amend `.agents/skills/verify-changes/SKILL.md` only, in two places:

1. `## Required inputs`: the reviews handoff bullet additionally requires, for
   every finding any review in this change returned, an explicit `accepted` or
   `rejected` disposition supplied by the manager, with a one-line reason on each
   `rejected` finding.
2. `## Acceptance table`: the review reconciliation row's `refuted` count must be
   backed by that per-finding list, so a rejected finding presented without a
   stated reason makes the row non-passing.

Change nothing else in the skill: the acceptance-table row shape, the
zero-unresolved-accepted-findings requirement, the read-only boundary, and every
other required input stay as they are. Add no new artifact type, no script, and
no new report section.

## Critical files

- `.agents/skills/verify-changes/SKILL.md` — the `## Required inputs` reviews
  bullet (`:26-28`) and the review reconciliation sentence in
  `## Acceptance table` (`:91-95`)

## In scope

- The two regions of `.agents/skills/verify-changes/SKILL.md` named above:
  require a per-finding `accepted` or `rejected + one-line reason` disposition,
  and bind the `refuted` count to it

## Out of scope

- Whether the `PlanScheduler.cpp:601` rejection was correct, and any obligation
  to implement a rejected finding
- `/repo-code-review`, `/codex-review`, `/scope-review`, `/adversarial-review`,
  and every other reviewer skill's own finding format
- `/resolve-findings` and the root `AGENTS.md` Step 5 decision policy
- Every other `## Required inputs` condition, the reviewed-diff derivation, the
  `## Executable Plan check`, and the `## Output` contract
- The landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior, documentation only); escalate if the fix
reaches root `AGENTS.md` workflow policy or a reviewer skill's finding format.
The verification contract must stay fail-closed and read-only: the added
requirement may only make a row non-passing, never let a row pass on weaker
evidence, and `/verify-changes` still never fixes or waives a finding.

## Acceptance criteria

- A manager following `.agents/skills/verify-changes/SKILL.md` alone must supply
  a disposition for every finding any review returned, and a one-line reason for
  each rejected one
- A rejected finding presented without a stated reason makes its review row
  non-passing
- `/validate-skill` passes for the changed `SKILL.md`; WorktreeCli
  `plan validate` exits `0` with `status: valid` and `code: ok`

## Notes

This Plan is keyed to the (skill, symptom) pair: `/verify-changes` accepts a
change whose reviewer findings include a rejected Required finding with no
recorded rationale. A later observation of the same pair is a duplicate, not a
new residual.
