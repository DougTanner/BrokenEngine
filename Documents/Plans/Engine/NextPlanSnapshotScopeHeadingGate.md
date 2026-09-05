<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T19:33:39.841Z","dependsOn":[]} -->
# Fix: `/next-plan` worker step 4 — its Done condition does not gate the snapshot's two scope headings, so `/plan-audit` blocks on a snapshot the handoff already passed

## Context
Observed in one `/next-plan` run's checkpoint, in the run that claimed
`Documents/Plans/Engine/FinalizeSmartGitLaunchFromMain.md`.

`.agents/skills/next-plan/references/worker.md:52-56` requires the scratch Plan
snapshot the preparation `implementer` writes for `/plan-audit` to carry the
"exact `## In scope` and `## Out of scope` sections with content intact". Step
4's Done condition immediately above it (`:48-50`) tests only two things: that
the execution card carries every field of the card template, and that the
preparation handoff cites the card as one file path plus `##` selector. Nothing
in that condition tests the two top-level headings, so a handoff can satisfy the
Done condition while the snapshot lacks them.

`/plan-audit` requires them: `.agents/skills/plan-audit/SKILL.md:34-37`
(`## Inputs`) states the snapshot body must carry the plan's `## In scope` and
`## Out of scope` sections, "headings verbatim, with their content intact",
because the citation check reads heading presence from the supplied file alone.
`.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1:358-360` is that check:
it reports `headings.inScopePresent` and `headings.outOfScopePresent` by matching
`^##\s+In scope\s*$` and `^##\s+Out of scope\s*$` against the supplied file's
lines.

What the ungated condition produced in the observed run: the preparation worker
wrote the snapshot with the scope content nested under a `## Resolved Plan`
heading rather than as the two top-level headings, returned a handoff that met
the Done condition, and `/plan-audit` returned `Status: BLOCKED` with the
residual "the snapshot omits the `## In scope` and `## Out of scope` sections
this skill's `## Inputs` requires". That cost one snapshot-revision round trip
back to the preparation worker plus one bounded re-audit dispatch before the Plan
review step could proceed.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 1719526a-7614-4ec0-8bc7-5ec42d5d2c1b
- Worktree/branch UUID: 0fa3347b-1c8f-44e8-986c-2a4b749d7a0a
- Session branch: claude/0fa3347b-1c8f-44e8-986c-2a4b749d7a0a
- Worktree: .claude\worktrees\BrokenEngine\0fa3347b-1c8f-44e8-986c-2a4b749d7a0a
- Landing ref: claude/0fa3347b-1c8f-44e8-986c-2a4b749d7a0a, this session's
  branch, which rides the same landing commit as the claimed Plan's completion.
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/NextPlanSnapshotScopeHeadingGate.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`;
the gap is visible in the cited files without any transcript. Only when the
transcript is genuinely needed, in a new session run
`/next-plan-review claude/0fa3347b-1c8f-44e8-986c-2a4b749d7a0a` in bounded
friction mode, supplying the recorded client and the conversation session ID
recorded above. Then make the smallest fix inside the `## In scope` boundary
below.

This author's recommendation for that fix, offered as a starting point rather
than a binding decision: extend step 4's Done condition so that, in the case
where a scratch snapshot is the `/plan-audit` input, it is not done until the
snapshot carries `## In scope` and `## Out of scope` as top-level headings
verbatim — and name the existing check that settles it, running
`pwsh -NoProfile -File .agents/skills/plan-audit/scripts/Test-PlanCitations.ps1 <snapshot path>`
and requiring `headings.inScopePresent` and `headings.outOfScopePresent` to be
`true` before the preparation handoff is returned. If the preparation brief form
in the same step enumerates what the snapshot must contain, the same requirement
belongs there too, so the worker is told before it writes rather than after it
blocks. Rationale: the requirement already exists one paragraph below the Done
condition and the checking script already exists and is already documented for
`/plan-audit`; the fix is to make the existing condition test the existing
requirement, adding no new machinery.

Alternatives considered and not recommended: relaxing `/plan-audit`'s `## Inputs`
to accept nested scope sections, which would weaken the diff boundary the two
scope sections give the audit; and adding a new snapshot-validation script, which
is new machinery when `Test-PlanCitations.ps1` already reports both flags.

If root-causing shows the fix lies outside the boundary below — for example that
`/plan-audit`'s `## Inputs` or the citation script must change instead — surface
it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/next-plan/references/worker.md` — step 4's Done condition and
  the scratch-snapshot paragraph that follows it (currently `:48-56`), the
  authorized fix boundary
- `.agents/skills/plan-audit/SKILL.md` — read-only: the `## Inputs` requirement
  the snapshot must satisfy (currently `:34-37`)
- `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1` — read-only: the
  existing `headings.inScopePresent` / `headings.outOfScopePresent` reporting
  (currently `:358-360`)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting prose fix, confined to step 4 of
  `.agents/skills/next-plan/references/worker.md`: its Done condition, the
  scratch-snapshot paragraph, and the preparation brief wording in that same step
  where it enumerates what the snapshot must contain

## Out of scope
- Every other step of `.agents/skills/next-plan/references/worker.md`, and
  `.agents/skills/next-plan/SKILL.md`
- `.agents/skills/plan-audit/SKILL.md`, its `## Inputs`, and
  `.agents/skills/plan-audit/references/worker.md`
- `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1` and every other
  script under `.agents/scripts/` and `.agents/skills/**/scripts/`; adding any
  new script
- `.agents/references/subagent-reporting.md` and the shared handoff form
- The landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one skill's documented workflow: what the
preparation `implementer` must produce and what its handoff is gated on) under
the root `AGENTS.md` risk tiers; this author's classification, to be confirmed at
the Approve and classify step. The trigger is the change to a documented tool
workflow's Done condition rather than to prose alone. It touches no
determinism/CRC, wire/protocol, serialization, replay, threading, or trust
surface. Escalate to Tier 3 only if the fix reaches build/bootstrap coordination.

Invariants to preserve:
- The claimed Plan file stays immutable during preparation, and every
  contradiction between the Plan and current code still returns to main as an
  execution card correction rather than an edit
- The existing card-template and file-plus-selector citation parts of the Done
  condition keep their current meaning
- The snapshot route stays optional: a run whose `/plan-audit` input is the
  claimed Plan path itself gains no new obligation
- The bundled-script rule's canonical
  `pwsh -NoProfile -File <repo-relative path>` invocation from the worktree root
  is used for any check the fix names
- The file keeps its existing encoding and line endings; no transcript path or
  home path enters the repository

## Acceptance criteria
- Step 4, read on its own, states a Done condition that a snapshot lacking either
  top-level scope heading cannot satisfy, and names the check that settles it
- Wherever step 4 tells the preparation worker what the snapshot must contain,
  the two headings are stated before the handoff rather than only afterwards
- The diff touches only the file named in `## In scope`
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed file
  where the root `AGENTS.md` Apply the triggered cleanup step triggers them
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` and `code: ok`

## Notes
`Documents/Plans/Engine/PlanAlternativesClaimedPlanSnapshotTransport.md` records
a neighbouring gap on the producing side — `/plan-alternatives`' `/next-plan`
claim bullet names no transport for the redrafted snapshot — and places
`.agents/skills/next-plan/references/worker.md` out of its own scope, which this
Plan owns. The two are disjoint by file and neither blocks the other. Whichever
lands second re-locates its region by content rather than by the line numbers
recorded here, and drops any wording the first landing already made true.
