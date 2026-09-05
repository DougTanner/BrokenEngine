<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-04T12:45:21.278Z","dependsOn":[]} -->
# Fix: next-plan worker step 4 opens with a 6-line paragraph the skill-skeleton step shape forbids

## Context
`.agents/references/skill-skeleton.md:15` states the step shape every
`.agents/skills/` worker follows: numbered steps, one imperative each, each
ending on a checkable done-condition, and `no paragraph over 4 lines`.

`.agents/skills/next-plan/references/worker.md:26-31` breaches the line limit.
Step 4's opening paragraph runs six lines — from `4. Dispatch one preparation`
through `main as a card correction rather than an edit.` — where the rule allows
at most four. The breach is in the source line layout, not in the rendered text:
the wording is correct and the two rules it carries are the ones the step exists
to state.

Those two rules are the reason the paragraph could not be reflowed when the
breach was found. A Review and resolve correctness reviewer in the observing
session proved it, and the
fixer confirmed it, but that session's approved plan and execution card both made
those exact bytes an invariant required to survive byte-identical, because the
paragraph carries step 4's Plan-immutability rule (the claimed Plan is immutable
during preparation) and its current-code-wins rule (a contradiction between the
Plan and current code returns to main as a card correction rather than an edit).
Reflowing them inside that change would have contradicted an already-reviewed
decision, so the fix was deferred to its own approved change: this Plan. The
breach is pre-existing and predates that session's change.

Session provenance (machine-local; not reproducible after cleanup). The observing
session is also the session that recorded this Plan:
- Client: claude
- Conversation session ID: b3d0f392-0458-4f50-b642-7b0ad61f1252
- Worktree/branch UUID: 9d7cce4d-ad51-4a59-88e4-d300aceef2fa
- Session branch: claude/9d7cce4d-ad51-4a59-88e4-d300aceef2fa
- Worktree: .claude\worktrees\BrokenEngine\9d7cce4d-ad51-4a59-88e4-d300aceef2fa
- Landing ref: this Plan was authored on that session's own branch, so its tip
  `claude/9d7cce4d-ad51-4a59-88e4-d300aceef2fa` is the ref whose tree contains
  this Plan until the session lands.
  Fallback once the recorded branch is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/NextPlanStepFourParagraphReflow.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so trust its result only when the commit is attributable to one session
  alone (its diff limited to that session's files).

## Design
Reflow that one paragraph so no paragraph in it exceeds four source lines, with
its meaning unchanged.

This author's recommendation for the fix, offered as a starting point rather than
a binding decision: split the six lines into two paragraphs at the semicolon that
already separates the two rules — a first paragraph holding the dispatch
imperative and its task-brief link, and a second holding the Plan-immutability
and current-code-wins rules — leaving the existing later paragraphs of step 4
untouched. Rationale: it is the smallest edit that satisfies the limit, it keeps
each surviving paragraph on one idea, and it changes no word of the two rules.
The alternative considered and not recommended is rewording the paragraph to fit
in four lines, which would restate rules that a prior review already accepted in
their current wording and so risks a silent meaning change.

Both rules must survive with their meaning unchanged, whatever split the fix
session chooses: the claimed Plan is immutable during preparation, and current
code wins over the Plan and card, with every contradiction returning to main as a
card correction rather than an edit.

## Critical files
- `.agents/skills/next-plan/references/worker.md` (step 4's opening paragraph,
  currently lines 26-31)
- `.agents/references/skill-skeleton.md` (read-only: owns the rule being
  satisfied; see `## Out of scope`)

## In scope
- Reflowing the single paragraph at `.agents/skills/next-plan/references/worker.md:26-31`
  so no resulting paragraph exceeds four source lines, preserving the wording of
  the Plan-immutability and current-code-wins rules

## Out of scope
- Every other paragraph, step, and rule in
  `.agents/skills/next-plan/references/worker.md`, including the rest of step 4
- Any other file's paragraphs, including a repository-wide sweep for the same
  rule elsewhere
- Editing `.agents/references/skill-skeleton.md` or changing the four-line rule
- Changing what step 4 means: no new rule, no removed rule, no reordering of the
  step sequence
- Adding a check, script, or lint that enforces the four-line rule
- Any transcript path or home path in the repository

## Risk tier and invariants
Expected Tier 1 (mechanical documentation line layout; no public signature, no
behavior, no invariant exposure) under the root `AGENTS.md` risk tiers; this
author's classification, to be confirmed at the Approve and classify step. The
trigger is the
documentation-only nature of the change. Escalate only if the fix session finds
it cannot satisfy the limit without changing wording.

Invariants to preserve:
- Step 4 still states that the claimed Plan is immutable
- Step 4 still states that current code wins, and that every contradiction
  returns to main as a card correction rather than an edit
- Step 4 still ends on a checkable done-condition and keeps its numbering and
  position in the step sequence
- The file keeps its existing encoding and line endings

## Acceptance criteria
- No paragraph in the reflowed region of
  `.agents/skills/next-plan/references/worker.md` exceeds four source lines
- The Plan-immutability and current-code-wins sentences are present with their
  meaning unchanged
- The diff touches only that one region of that one file
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` and `code: ok`
