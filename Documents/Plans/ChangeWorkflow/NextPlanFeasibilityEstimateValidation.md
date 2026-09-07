<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T17:29:06.812Z","dependsOn":[]} -->
# Fix: /next-plan preparation dispatch — quantitative feasibility estimate accepted without an inventory-preservation check

## Context

During a `/next-plan` run whose claimed Plan trimmed `Engine/Source/File/AGENTS.md`
in place, the step 4 preparation `implementer` returned a quantitative feasibility
estimate as a Finding: a whole-file floor of roughly 2,400-2,600 tokens, derived
from two trial rewrites of the file's `## Replay Streams` section that the handoff
stated kept all 53 rules. Main carried that floor into the approval presentation,
and the user approved an acceptance criterion built on it.

The implementation `implementer` later proved the estimate wrong. The trial file
`Temp/replay-trim-trial2.md` reached its size only by dropping rule content —
rules RS-4, RS-8, RS-16, RS-17, RS-18, RS-19, RS-20, RS-24, and fragments of
RS-40 — and the rule-preserving rewrite measured 3,343 tokens, well above the
reported floor. The approved size criterion therefore came back as a residual main
had to re-decide after implementation, and the user's approval decision had rested
on a number no check had validated.

The run already had the means to catch this: the same preparation produced a
`## Before`/`## After` rule-ID cross-check inventory. Nothing in the dispatch
required the estimate to be tied to it.

The instruction gap is in
`.agents/skills/next-plan/references/worker.md` step 4, in the paragraph beginning
"The brief bounds the card's verification evidence". That paragraph governs what
verification evidence a preparation handoff returns, and it says nothing about a
quantitative feasibility estimate: it does not require such an estimate to name the
check proving the trial it was measured on preserved the document's rule inventory.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the session
`/next-plan-review` must reach — while the `Landing ref` line names a ref whose
tree actually contains this Plan:
- Client: claude
- Conversation session ID: 3656f895-74c5-4b8a-9101-1c1e80d69753
- Worktree/branch UUID: 412415d0-8439-4ca9-8967-7690feb857d2
- Session branch: claude/412415d0-8439-4ca9-8967-7690feb857d2
- Worktree: .claude\worktrees\BrokenEngine\412415d0-8439-4ca9-8967-7690feb857d2
- Landing ref: claude/412415d0-8439-4ca9-8967-7690feb857d2
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so review
  its result only when the commit is attributable to one session alone (its diff
  limited to that session's files); never review an aggregate or multi-session
  squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the recorded
client and conversation session ID. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

The author's recommendation is one added sentence in step 4's brief-bounding
paragraph, requiring that a preparation handoff returning a quantitative
feasibility estimate name the check that proves the trial the estimate was
measured on preserved the source document's rule or item inventory, and that an
estimate without such a check be reported as unvalidated. Recommended because the
gap is a missing evidence requirement on an existing dispatch, not a missing
mechanism: the cross-check the fix would name already exists in the same run, so
no new script, template field, or gate is needed.

Change Workflow tier: expected Tier 1, mechanical instruction-document wording in
one reference, with no public signature or invariant exposure. Escalate if the fix
turns out to require a new gate in the step 4 Done condition or a change to the
execution-card template, which would make it scoped tool behavior. Never embed
transcript paths or home paths.

## Critical files

- `.agents/skills/next-plan/references/worker.md`

## In scope

- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to step 4 of
  `.agents/skills/next-plan/references/worker.md`, in the paragraph beginning
  "The brief bounds the card's verification evidence"

## Out of scope

- The landed documentation change the observing session produced
- The step 4 Done condition, the execution-card template in
  `.agents/skills/next-plan/SKILL.md`, and the approval-presentation contract
- Token-measurement scripts and their thresholds
- Unrelated skills and scripts; any transcript path or transcript text in the
  repository

## Risk tier and invariants

Expected Tier 1 (mechanical instruction wording); escalate per `## Design` if the
fix reaches the step 4 gate or the card template. The preparation handoff contract
stays a single contract bound by `## Handoff` in `.agents/skills/next-plan/SKILL.md`;
the Plan stays immutable during preparation; no transcript path or home path enters
the repository.

## Acceptance criteria

- Step 4's brief-bounding paragraph states that a quantitative feasibility estimate
  returned as a Finding must name the check proving its trial preserved the source
  document's rule or item inventory, and that an estimate lacking it is reported as
  unvalidated
- The recorded symptom — an unvalidated floor reaching an approval presentation —
  no longer follows from the instruction text
- `/validate-skill` passes for `next-plan`, and
  `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid`, `code: ok`

## Notes

The observing session's failure was in the returned estimate, not in the handoff
form: the preparation worker used the declared Finding row correctly, so the fix is
an added evidence requirement rather than a handoff-shape correction.
