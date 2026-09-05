<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T14:35:59.588Z","dependsOn":[]} -->
# Fix: next-plan worker step 4 — the scratch plan snapshot is required but never assigned an author, so main re-authors it

## Context
Observed in one `/next-plan` run's checkpoint, in the run that claimed
`Documents/Plans/Engine/NextPlanDirtyPlanClaimRecovery.md`.

`.agents/skills/next-plan/references/worker.md:47-52` (step 4's Done condition)
says that when the `/plan-audit` input is a scratch snapshot instead of the
claimed Plan path, "that snapshot carries the plan's `## In scope` and
`## Out of scope` sections exactly as `/plan-audit`'s `## Inputs` requires". It
never says who writes that snapshot, and no other step assigns it either.

What that unassigned authorship produced in the observed run:

- The preparation `implementer` dispatched per step 4 returned a 65-line,
  9,347-character handoff, over the 40-line whole-handoff cap in
  `.agents/references/subagent-reporting.md` `## Handoffs:94-99`. The excess was
  fix-shape prose — the chosen fix, the rejected alternatives, and six execution
  card corrections C1-C6 — carried verbatim into main.
- Main then re-authored that same material itself, as an 8,577-character `Write`
  of the scratch snapshot `Temp/next-plan-dirty-claim-snapshot.md`, carrying
  `## In scope`, `## Out of scope`, `## Chosen mechanism`, and
  `## Execution card`, because `/plan-audit` `## Inputs`
  (`.agents/skills/plan-audit/SKILL.md:34-44`) takes a path and never inline
  text.

So the same content was produced twice: once into main's context as oversized
handoff prose, and once out of main's context as a file main typed. Main is the
one session whose context the root `AGENTS.md` asks to keep clean, and it is
also the only party the current text leaves able to write the snapshot.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 7cf2e7b7-b7f4-407f-9dd3-aa1f75b98311
- Worktree/branch UUID: 8563dd54-fd14-4d11-b23e-72b12683ef32
- Session branch: claude/8563dd54-fd14-4d11-b23e-72b12683ef32
- Worktree: .claude\worktrees\BrokenEngine\8563dd54-fd14-4d11-b23e-72b12683ef32
- Landing ref: claude/8563dd54-fd14-4d11-b23e-72b12683ef32, this session's
  branch, whose landing commit will contain this Plan.
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/NextPlanPreparationSnapshotAuthorship.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`; the gap is visible in the cited files without any transcript. Only
when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/8563dd54-fd14-4d11-b23e-72b12683ef32` in bounded
friction mode, supplying the recorded client and the conversation session ID
recorded above. Then make the smallest fix inside the `## In scope` boundary
below.

This author's recommendation for that fix, offered as a starting point rather
than a binding decision: in the same step 4 Done-condition sentence, name the
preparation `implementer` as the writer of the scratch snapshot — it writes the
sections `/plan-audit` `## Inputs` requires plus the execution card, and cites
the snapshot back to main under `Evidence` as path plus an `## Execution card`
selector — leaving main to receive only the card corrections and the shared
handoff rows. Rationale: the snapshot's content is exactly what the preparation
worker already produced, so writing it there removes both the oversized handoff
and main's retyping in one prose sentence, and the "path plus selector" route is
the one `.agents/references/subagent-reporting.md` `## Handoffs` already
prescribes for oversized material.

Alternatives considered and not recommended: leaving authorship with main and
only tightening the handoff cap wording, which keeps the duplicate authoring;
and adding a script that assembles the snapshot, which is new machinery for a
gap a sentence closes.

If root-causing shows the fix lies outside the boundary below — for example that
`/plan-audit`'s `## Inputs` must change rather than step 4 — surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/next-plan/references/worker.md` — step 4, the Done-condition
  paragraph about the scratch snapshot (currently `:47-52`)
- `.agents/skills/plan-audit/SKILL.md` — `## Inputs` (`:34-44`), the requirement
  the snapshot satisfies; read-only unless a cross-reference is needed
- `.agents/references/subagent-reporting.md` — read-only: the handoff caps and
  the path-plus-selector route

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting prose fix, confined to step 4 of
  `.agents/skills/next-plan/references/worker.md` — the Done-condition
  scratch-snapshot sentence and, if the fix needs it, the dispatch sentence that
  states what the preparation `implementer` returns
- If and only if the fix needs it, a cross-reference in `## Inputs` of
  `.agents/skills/plan-audit/SKILL.md` naming who supplies the snapshot

## Out of scope
- Steps 1-3 and 5-9 of `.agents/skills/next-plan/references/worker.md`, and
  step 4's opening paragraph line layout
- `.agents/references/subagent-reporting.md`, including the handoff form and its
  size caps
- `.agents/skills/next-plan/SKILL.md` frontmatter and the execution card
  template
- Every script under `.agents/skills/next-plan/scripts/` and
  `.agents/scripts/`; adding any new script
- `/plan-audit`'s audit rules, findings form, and handoff
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (mechanical documentation prose describing an existing
delegation step; no public signature, no runtime behavior, no invariant
exposure) under the root `AGENTS.md` risk tiers; this author's classification,
to be confirmed at the Approve and classify step. The trigger is the
documentation-only nature of the change. Escalate to Tier 2 if the fix changes
what `/plan-audit` accepts as an input or any script behavior.

Invariants to preserve:
- The claimed Plan stays immutable during preparation, and every contradiction
  between the Plan and current code still returns to main as a card correction
  rather than an edit
- The snapshot still carries `## In scope` and `## Out of scope` with their
  headings verbatim and their content intact, so `/plan-audit`'s citation check
  still has a diff boundary
- Step 4 still ends on a checkable done-condition and keeps its numbering
- No paragraph in the changed region exceeds four source lines
- The file keeps its existing encoding and line endings; no transcript path or
  home path enters the repository

## Acceptance criteria
- Step 4, read on its own, names exactly one party as the writer of the scratch
  snapshot and states where that snapshot is cited from
- A preparation handoff following the fixed step 4 can stay within the 40-line
  and 20,000-character caps in `.agents/references/subagent-reporting.md`
  `## Handoffs` without dropping anything `/plan-audit` needs
- The diff touches only the files named in `## In scope`
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed
  files where the root `AGENTS.md` Apply the triggered cleanup step triggers
  them
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` and `code: ok`

## Notes
`Documents/Plans/Engine/NextPlanStepFourParagraphReflow.md` also targets step 4
of the same file, but a disjoint region: the line layout of step 4's opening
paragraph (`:26-31`). Neither Plan blocks the other; whichever lands second
re-locates its region by content rather than by the line numbers recorded here.
