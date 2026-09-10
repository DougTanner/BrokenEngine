<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T18:09:53.178Z","dependsOn":[]} -->
# Trim the `/next-plan` run checkpoint: no-work dispatch and envelope telemetry

## Context
A repository survey of workflow mechanisms that cost work on every run but change
no decision found two inside the `/next-plan` run checkpoint:

1. Unconditional dispatch on a terminal path that produced no work (survey rank
   A5). `.agents/skills/next-plan/SKILL.md:125-132` requires the checkpoint on
   every terminal path — "a `none-available` or other claim stop, a deferral, a
   blocker, or a refused approval comes straight here from wherever it stopped"
   — and the post-checkpoint outcome table repeats that per row
   (`.agents/skills/next-plan/SKILL.md:162-166`). Each such run still costs one
   fresh `reviewer` dispatch
   (`.agents/skills/next-plan/references/run-checkpoint.md:8-15`) plus that
   reviewer's `Measure-SessionContext.ps1` and `Get-TranscriptProjection.ps1`
   runs. On a claim stop there is no implementation and no isolation content to
   review, the context lens fires only on rows marked `overThreshold: true`
   (`.agents/skills/next-plan-checkpoint-review/references/worker.md:68-70`),
   and a `none-available` stop is already excluded from friction as a
   "documented normal stop"
   (`.agents/skills/next-plan-checkpoint-review/references/worker.md:38-39`).
2. Numeric envelope telemetry (survey rank A6). `totalChars`, `lineCount`, and
   `toolResultCount` are emitted by
   `.agents/skills/next-plan-checkpoint-review/scripts/Measure-SessionContext.ps1:182-188`,
   whose own comment says "totalChars is telemetry only: the verdict turns on
   per-result breaches"
   (`.agents/skills/next-plan-checkpoint-review/scripts/Measure-SessionContext.ps1:180-181`);
   the worker rule repeats that it "never produces a finding on its own"
   (`.agents/skills/next-plan-checkpoint-review/references/worker.md:68-70`).
   The numeric `Rows at or over threshold:` count in the summary block
   (`.agents/skills/next-plan-checkpoint-review/SKILL.md:77-92`) is likewise not
   consumed: the recorded handoff line carries Plan paths, not the count
   (`.agents/skills/next-plan/references/run-checkpoint.md:22-29`).

The survey rested on current text and on one session's observations, not on
measured run outcomes, so this Plan validates each claim before removing
anything.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: 87c59217-31e1-486d-a91d-84088032264f
- Worktree/branch UUID: 2a97729c-6824-417f-863f-c6dbb60b910d
- Session branch: claude/2a97729c-6824-417f-863f-c6dbb60b910d
- Worktree: .claude/worktrees/BrokenEngine/2a97729c-6824-417f-863f-c6dbb60b910d
- Originating step: user instruction "Create followup plans to validate then fix
  these", given after the Verify the acceptance table step of the session that
  removed the delegated handoff `Executor` self-report line.

## Design
Two stages, per mechanism. Stage 2 for a mechanism runs only when its stage-1
checks hold; a mechanism whose check fails is left exactly as it is — stop and
report it, with no partial edit — while the other mechanism may still proceed.

### Stage 1 — validation (read-only)
- No-work dispatch (mechanism 1). Read every lens in
  `.agents/skills/next-plan-checkpoint-review/references/worker.md` and confirm
  that on a run which claimed nothing and changed nothing, each lens is either
  excluded by its own rule or has no input: the friction lens by the
  documented-normal-stop exclusion at `:38-39` and its precision guard at
  `:64-67`, the context lens by the `overThreshold: true` gate at `:68-70`, and
  the isolation lens by having no implementation content in span. The check ends
  when each lens has been traced to its exclusion or to an empty input on that
  run shape. The claim fails — and mechanism 1 stops — if any lens can still
  produce a finding on a run that claimed nothing and changed nothing; the
  observed counter-example is friction in the claim machinery itself, which
  fires before the stop.
- Numeric telemetry (mechanism 2). Search `.agents/` for `totalChars`,
  `lineCount`, `toolResultCount`, and `Rows at or over threshold`, and confirm
  every hit is a producer, its own documentation, or the summary line itself,
  with no rule branching on a value. The check ends with that search result in
  hand.

### Stage 2 — removal and downgrade
- Mechanism 1: replace the terminal-path clause in
  `.agents/skills/next-plan/SKILL.md:125-132` and the matching post-checkpoint
  table rows with a single condition — the checkpoint runs when the run
  performed work. Recommended definition of "performed work", stated once in
  `.agents/skills/next-plan/references/run-checkpoint.md` and referenced from
  `SKILL.md`: the run held a claim at any point, or changed a tracked file.
  Rationale: this keeps the checkpoint on every run that could produce a
  finding, including a refused approval and a blocker after a claim, and drops
  it only on the claim stop where every lens is already excluded. The
  post-checkpoint table rows keep their claim-disposition and next-action
  columns unchanged; only their "run the checkpoint once" instruction becomes
  conditional. The known cost of this trim — friction observed before a claim
  stop goes unreported until `/next-plan-review` — is accepted, and the fix
  session states it in the reference so a reader can see it.
- Mechanism 2: drop `totalChars`, `lineCount`, and `toolResultCount` from the
  `broken-engine-context-efficiency/v1` envelope and from the reviewer's rules,
  and reduce the summary line's numeric branch to a pass/needs-review word,
  keeping the `skipped (<code>)` and `skipped (breach-rows-truncated)` forms
  unchanged because
  `.agents/skills/next-plan/references/run-checkpoint.md:22-29` routes on them.
  Rationale: the envelope's verdict already turns on `overThresholdCount` and
  `topResults`, which stay.

## Critical files
- `.agents/skills/next-plan/SKILL.md` — step 9 and `### Post-checkpoint outcomes`
- `.agents/skills/next-plan/references/run-checkpoint.md` — `## Dispatch` and
  `## Measurement states`
- `.agents/skills/next-plan-checkpoint-review/SKILL.md` — the summary block and
  its numeric rule
- `.agents/skills/next-plan-checkpoint-review/scripts/Measure-SessionContext.ps1`
  — the envelope construction
- `.agents/skills/next-plan-checkpoint-review/references/worker.md` — the
  telemetry caveat in step 8

## In scope
- The stage-1 validation reads above
- In `.agents/skills/next-plan/SKILL.md`: step 9's terminal-path clause and the
  "run the checkpoint once" instruction in each `### Post-checkpoint outcomes`
  row
- In `.agents/skills/next-plan/references/run-checkpoint.md`: the one statement
  of when the checkpoint runs, and the summary-line column of the
  `## Measurement states` table where the numeric form changes
- In
  `.agents/skills/next-plan-checkpoint-review/scripts/Measure-SessionContext.ps1`:
  the `lineCount`, `toolResultCount`, and `totalChars` envelope members and the
  now-unused computation feeding them, plus the comment that described them
- In `.agents/skills/next-plan-checkpoint-review/SKILL.md`: the summary block's
  numeric branch
- In `.agents/skills/next-plan-checkpoint-review/references/worker.md`: the
  `totalChars` telemetry caveat in step 8

## Out of scope
- `overThresholdCount`, `topResults`, `thresholds`, `verdict`, and
  `breachRowsTruncated`, and the whole per-result breach path
- The three lenses' own rules, precision guard, and span rules
- `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`
- `.agents/skills/next-plan/scripts/` claim, defer, and completion scripts, and
  the claim dispositions in the post-checkpoint table
- `/next-plan-review` and its measurement rules
- Any transcript path, transcript text, or machine-local path in the repository

## Risk tier and invariants
Expected Tier 2: scoped behavior of the `/next-plan` checkpoint route and one
bundled script, under `.agents/references/risk-tiers.md`'s "one subsystem's ...
tool behavior". No determinism, wire, serialization, threading, or trust surface
is touched, and no landing or claim mechanism changes. Escalate to Tier 3 if the
fix reaches the claim scripts or the landing gate. Invariants: the schema version
`broken-engine-context-efficiency/v1` is bumped or its removed members are
absent from every consumer in the same change; every `## Measurement states` row
still maps to exactly one recorded handoff line; a run that held a claim or
changed a tracked file still reaches the checkpoint exactly once.

## Acceptance criteria
- A run that claimed nothing and changed no tracked file dispatches no
  checkpoint reviewer, and `.agents/skills/next-plan/SKILL.md` states that
  condition by reference to one owning definition
- A run that held a claim at any point, or changed a tracked file, still reaches
  the checkpoint exactly once, on every terminal path
- `Measure-SessionContext.ps1` emits no `totalChars`, `lineCount`, or
  `toolResultCount`, and running it against any transcript still returns a
  well-formed envelope whose `verdict` matches its `overThresholdCount`
- No file under `.agents/` reads a removed member or the numeric row count
- `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1` reports the
  `validate-skill` and `markdown-links` rows passing for both changed packages
- A mechanism whose stage-1 check failed is unchanged in the diff and named in
  the session's report
