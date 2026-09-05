<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T16:38:02.206Z","dependsOn":[]} -->
# Fix: `/plan-alternatives` — the `/next-plan` claim route names no producer or transport for the redrafted plan material, so main hand-authors both

## Context
Observed in one `/next-plan` run's checkpoint, in the run that claimed
`Documents/Plans/Engine/FinalizePreConfirmationRoundTrips.md`.

`.agents/skills/plan-alternatives/SKILL.md:99-106` (the `/next-plan` claim bullet
of `### User presentation`) says only that the claimed Plan file is immutable, so
"the preparation `implementer` records the chosen approach on the execution card
and implementation follows the card". It names no artifact that carries the
redrafted scope and card onward, no file the worker writes it to, and no way for
that material to reach main without travelling through main's context — even
though `.agents/skills/next-plan/references/worker.md:51-54` already assumes a
scratch snapshot file exists as the `/plan-audit` input whenever the claimed Plan
path itself cannot be used.

What that missing transport produced in the observed run: with no documented
route, main hand-authored one in the dispatch brief it wrote to resume the
preparation `implementer` after the user picked an alternative. That improvised
brief suspended the standing overflow route in
`.agents/references/subagent-reporting.md` `## Handoffs` — it told the worker not
to write a scratch file at all and lifted the 40-line whole-handoff cap — and put
an unenforced character cap in its place. The returned reply exceeded that
substituted cap, arriving as the run's single over-threshold tool result: the
context-efficiency envelope records `SendMessage`, the resumed preparation
dispatch, 21,607 characters. Main then hand-corrected the resulting snapshot
across seven separate `Edit` calls before it could be used.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 552a615b-9166-4c5b-8991-7e9c279d2dcf
- Worktree/branch UUID: def6cf77-44da-402b-b5e4-a597e25a7971
- Session branch: claude/def6cf77-44da-402b-b5e4-a597e25a7971
- Worktree: .claude\worktrees\BrokenEngine\def6cf77-44da-402b-b5e4-a597e25a7971
- Landing ref: claude/def6cf77-44da-402b-b5e4-a597e25a7971, this session's
  branch, whose landing commit will contain this Plan.
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/PlanAlternativesClaimedPlanSnapshotTransport.md`,
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
`/next-plan-review claude/def6cf77-44da-402b-b5e4-a597e25a7971` in bounded
friction mode, supplying the recorded client and the conversation session ID
recorded above. Then make the smallest fix inside the `## In scope` boundary
below.

This author's recommendation for that fix, offered as a starting point rather
than a binding decision, and taken from the checkpoint reviewer's proposed
bounding: extend the `/next-plan` claim bullet so it states, in the claimed-Plan
case where the user picks an alternative, that the preparation `implementer`
writes both the redrafted execution card and a plan snapshot carrying
`## In scope` and `## Out of scope` to `Temp/plan-snapshot-<slug>.md`, and
returns Findings plus an `Evidence:` line giving that path and the `##`
selectors, under the ordinary `.agents/references/subagent-reporting.md`
`## Handoffs` overflow rule rather than any per-dispatch substitute; main then
passes that path to `/plan-audit` and `/plan-simplicity-review` and lifts only
the part of the snapshot it must present to the user. Rationale: the material is
exactly what the preparation worker already produces, the file-plus-selector
route is the one the shared handoff reference already prescribes for oversized
material, and naming it in the skill removes main's need to invent a transport
per run — which is what suspended the caps and forced the hand-correction pass.

Alternatives considered and not recommended: leaving the bullet as is and relying
on main to improvise a brief each time, which is the observed failure; and adding
a script that assembles the snapshot, which is new machinery for a gap one
sentence closes.

If root-causing shows the fix lies outside the boundary below — for example that
`/next-plan`'s step 4 or `/plan-audit`'s `## Inputs` must change instead — surface
it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/plan-alternatives/SKILL.md` — the `/next-plan` claim bullet of
  `### User presentation` (currently `:99-106`), the authorized fix boundary
- `.agents/references/subagent-reporting.md` — read-only: the `## Handoffs` caps
  and the file-plus-selector overflow route the fix cites
- `.agents/skills/next-plan/references/worker.md` — read-only: step 4's
  Done-condition scratch-snapshot sentence (currently `:51-54`), the consumer
  that already assumes such a file exists

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting prose fix, confined to the `/next-plan` claim bullet of
  `### User presentation` in `.agents/skills/plan-alternatives/SKILL.md`, naming
  the party that writes the redrafted card and snapshot, the file the snapshot is
  written to, the handoff route that cites it, and what main passes onward

## Out of scope
- `.agents/skills/next-plan/references/worker.md`, including step 4's
  Done-condition scratch-snapshot sentence — `## Notes` below names the Plan that
  owns it
- The other two route bullets of the same list (the Tier-2+ non-claim route and
  the Tier-1 route), `### Comparison`, `## Inputs`, `## When to use`, and the
  frontmatter of `.agents/skills/plan-alternatives/SKILL.md`
- `.agents/skills/plan-alternatives/references/worker.md`, which holds only the
  dispatched researcher's steps
- `.agents/references/subagent-reporting.md`, including the handoff form and its
  size caps
- `/plan-audit` and `/plan-simplicity-review` inputs, rules, and handoffs
- Adding any script, and every existing script under `.agents/scripts/` and
  `.agents/skills/**/scripts/`
- The landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one skill's documented workflow: what the
preparation `implementer` writes and what main hands to the Plan review
reviewers) under the root `AGENTS.md` risk tiers; this author's classification,
to be confirmed at the Approve and classify step. The trigger is the change to a
documented tool workflow rather than to prose alone. It touches no
determinism/CRC, wire/protocol, serialization, replay, threading, or trust
surface. Escalate to Tier 3 only if the fix reaches build/bootstrap coordination.

Invariants to preserve:
- The claimed Plan file stays immutable during preparation, and every
  contradiction between the Plan and current code still returns to main as an
  execution card correction rather than an edit
- The snapshot carries `## In scope` and `## Out of scope` with their headings
  verbatim and their content intact, so `/plan-audit` still has a diff boundary
- The verbatim user question in `### User presentation` and the two other route
  bullets stay unchanged
- `SKILL.md` remains the public file: it states what a parent session needs to
  dispatch and consume the skill, and delegates mechanics to the references it
  already cites
- The file keeps its existing encoding and line endings; no transcript path or
  home path enters the repository

## Acceptance criteria
- The `/next-plan` claim bullet, read on its own, names exactly one party as the
  writer of the redrafted card and snapshot, the path it is written to, and what
  main passes to `/plan-audit` and `/plan-simplicity-review`
- A resumed preparation dispatch following the fixed bullet needs no per-dispatch
  substitute for the `.agents/references/subagent-reporting.md` `## Handoffs`
  caps, and its handoff stays within them
- The diff touches only the file named in `## In scope`
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed file
  where the root `AGENTS.md` Apply the triggered cleanup step triggers them
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` and `code: ok`

## Notes
`Documents/Plans/Engine/NextPlanPreparationSnapshotAuthorship.md` records the
sibling gap on the consuming side — `/next-plan` worker step 4 requires a scratch
snapshot without naming its author — and owns that file's region, which this Plan
places out of scope. The two are disjoint by file and neither blocks the other:
this Plan closes the claimed-Plan alternatives route in
`/plan-alternatives`, that one closes step 4's Done condition. Whichever lands
second re-locates its region by content rather than by the line numbers recorded
here, and drops any wording the first landing already made true.
