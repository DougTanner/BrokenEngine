<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-22T19:22:49.600Z","dependsOn":[]} -->
# Fix: `.agents/skills/next-plan` — main copies review findings or execution card checks inline into subagent briefs instead of citing `Temp/` paths and selectors

## Context

Four incident rows in the same worktree across two consecutive `/next-plan`
runs recorded main reading execution card check bodies or repeating Plan review
findings and applied changes inline in subagent briefs instead of citing
gitignored `Temp/` artifact paths, finding IDs, and section selectors:

- 2026-11-22, `agy/4004df43-5824-4718-822f-007c9e5d0963`: selector Plan review
  write-in, pre-edit, and slice briefs; main restated review rows and card
  fixtures inline; bounding: cite `Temp/` review outputs and
  `## Execution card` selectors with manager decisions.
- 2026-11-22, `agy/4004df43-5824-4718-822f-007c9e5d0963`: selector pre-edit
  execution card checks 1-2; main read both check ranges before dispatching the
  pre-edit implementer; bounding: pass snapshot `## Execution card` selector
  and check IDs.
- 2026-11-22, `agy/4004df43-5824-4718-822f-007c9e5d0963`: selector planner
  revision brief for 12 accepted Plan findings; main repeated all 12 findings
  inline; bounding: cite both `Temp/` review receipts by finding ID with inline
  manager decisions.
- 2026-11-22, `agy/4004df43-5824-4718-822f-007c9e5d0963`: selector difference
  `/plan-simplicity-review` prompt; main repeated all 12 applied changes inline;
  bounding: cite revised snapshot `## Mechanism` and `## Execution card` by
  changed item and trigger IDs.

Current tree observations:

- `.agents/skills/next-plan/SKILL.md` step 5 (`:133-138`), step 8 (`:175-189`),
  and `## Handoff` (`:271-275`) delegate Plan review write-in, difference
  `/plan-simplicity-review`, and pre-edit acceptance checks to
  `.agents/references/change-workflow.md` without requiring path-plus-selector
  or finding-ID citations for those dispatches.
- `.agents/references/change-workflow.md` `#### Step 3 — Plan review`
  (`:245-254`) directs main to dispatch a fresh `planner` to write accepted
  findings into the snapshot and a fresh `reviewer` for the difference
  `/plan-simplicity-review`, without stating that main cites `Temp/` review
  receipt paths by finding ID (with inline decisions) and revised snapshot
  section selectors instead of repeating finding text or applied changes inline.
- `.agents/references/change-workflow.md` `#### Step 4 — Implement and
  propagate` (`:264-272`) directs main to read each `before the first edit`
  check from its marked line to the check's end and extract the executor,
  build, and scenario steps for `builder` and `runner` briefs (`:269-272`).
  That rule pulls full marked check ranges into main context (including before
  the recorded pre-edit `implementer` dispatch, which noted the check reads
  without evidence that check bodies were pasted into that brief).
- `.agents/skills/plan-simplicity-review/SKILL.md` `## Inputs` (`:59-61`) asks
  the manager to supply "the difference those findings added" without
  specifying selector/item-ID citations into the revised snapshot rather than
  inline change summaries.
- `.agents/references/subagent-reporting.md` `## Task brief` (`:94-98`) and
  `## Handoffs` (`:118`) state that `Evidence` passes on as path plus selector,
  without specifically addressing `Temp/` review receipt finding IDs or
  `## Execution card` pre-edit check IDs.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: agy
- Conversation session ID: 5520d05d-dd0b-4061-98a4-ae41008f3e21
- Worktree/branch UUID: 4004df43-5824-4718-822f-007c9e5d0963
- Session branch: agy/4004df43-5824-4718-822f-007c9e5d0963
- Worktree: .agy\worktrees\BrokenEngine\4004df43-5824-4718-822f-007c9e5d0963
- Landing ref: agy/4004df43-5824-4718-822f-007c9e5d0963
  The review ref the invoker passes to `/next-plan-review` has two cases. When
  the `Observed in an earlier session` line is present, it is that line's
  landed commit hash, with no lookup. When that line is absent, the recording
  session is the observing session, and worktree reuse moves its branch past
  its landing, so the invoker never reviews the ref itself: it resolves the ref
  to the commit that added this Plan with
  `git log --follow --diff-filter=A -n 1 --format=%H <ref> -- <this plan path>`
  and passes that commit. When the recorded ref is gone or that command prints
  nothing (a later session moved this Plan), it runs the same command with
  `<ref>` omitted. A periodic Plan-history squash can make either command
  return an unrelated aggregate commit, and a landing that also deletes a
  similar file can make it return another session's commit, so review the
  result only when the commit is attributable to the observing session recorded
  above alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before `/cleanup-worktrees` removes the worktree recorded
  above: Codex discovery requires a registered worktree, and Claude or agy
  review requires the conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's
`## Context` — confirming which instruction owner should carry each rule without
duplicating prose across layers (progressive disclosure): (1) pre-edit
`before the first edit` dispatch in `.agents/references/change-workflow.md`
`#### Step 4 — Implement and propagate` (identifying marked check IDs and
executor/build requirements without pulling full check bodies into main context
or pasting steps inline, or moving the check-ID summary into the preparation
handoff/card format), (2) Plan review write-in and difference
`/plan-simplicity-review` brief citations in
`.agents/references/change-workflow.md` `#### Step 3 — Plan review`,
`.agents/skills/plan-simplicity-review/SKILL.md` `## Inputs`, and
`.agents/skills/next-plan/SKILL.md` (and `.agents/references/subagent-reporting.md`
only if a shared rule belongs there). Only when the transcript is genuinely
needed, in a new session run `/next-plan-review <review ref>` in bounded
friction mode, with the review ref the provenance block above defines,
supplying the recorded client (`agy`) and conversation session ID
(`5520d05d-dd0b-4061-98a4-ae41008f3e21`). Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

## Critical files

- `.agents/skills/next-plan/SKILL.md` — step 5 (`:133-138`), step 8
  (`:175-189`), and `## Handoff` (`:271-275`).
- `.agents/references/change-workflow.md` — `#### Step 3 — Plan review`
  (`:245-254`) and `#### Step 4 — Implement and propagate` (`:264-272`).
- `.agents/skills/plan-simplicity-review/SKILL.md` — `## Inputs` (`:54-64`).
- `.agents/references/subagent-reporting.md` — `## Task brief` (`:94-98`) and
  `## Handoffs` (`:118`), only if a shared citation rule belongs there rather
  than in `change-workflow.md`.
- `.agents/references/execution-card.md` — `## Template`, only if a compact
  check-ID / executor line format is needed so main can identify pre-edit
  executors without reading full check bodies.

## In scope

- Root-cause investigation as `## Design` states.
- The smallest resulting fix, confined to these regions:
  - `.agents/skills/next-plan/SKILL.md` — `## Steps` (step 5 and step 8) and
    `## Handoff`.
  - `.agents/references/change-workflow.md` — `#### Step 3 — Plan review` and
    `#### Step 4 — Implement and propagate`.
  - `.agents/skills/plan-simplicity-review/SKILL.md` — `## Inputs`.
  - Only if required by the chosen single-owner placement:
    `.agents/references/subagent-reporting.md` (`## Task brief` and
    `## Handoffs`) and `.agents/references/execution-card.md` (`## Template`).

## Out of scope

- The landed change the observing session produced.
- Changing `/next-plan` claim scripts, WorktreeCli, or non-Plan-review
  `/resolve-findings` / `/external-diagnose-bug` briefs (tracked separately in
  `WorkflowIncidents.md`).
- Unrelated skills/scripts, C++ or shader code, Visual Studio project files,
  and any transcript path or transcript text in the repo.

## Risk tier and invariants

Expected Tier 2 (scoped tool/workflow behavior — dispatch and brief-citation
instructions in `.agents/skills/next-plan/SKILL.md`,
`.agents/references/change-workflow.md`,
`.agents/skills/plan-simplicity-review/SKILL.md`, and optionally
`.agents/references/subagent-reporting.md` /
`.agents/references/execution-card.md`; no determinism/CRC, wire,
serialization, `.pack`/`kiVersion`, replay, affinity, threading, allocation,
shader, build, or live `/agent-harness` exposure); escalate if the fix reaches
build/bootstrap coordination or independently owned subsystems. Keep single
ownership per progressive disclosure and avoid increasing
`.agents/skills/next-plan/SKILL.md` past host injection limits. Never embed
transcript paths or home paths.

## Acceptance criteria

- The documented Step 3 (Plan review write-in and difference
  `/plan-simplicity-review`) and Step 4 (pre-edit acceptance checks) dispatch
  rules direct main to cite gitignored `Temp/` review receipt paths with
  finding IDs (plus inline manager decisions) and snapshot `## Execution card`
  / `## Mechanism` selectors with check or item IDs instead of reading full
  check bodies or copying finding, change, or execution-card fixture text
  inline.
- The recorded full-check reads and inline repetition of finding prose, change
  prose, or execution-card fixture text no longer reproduce in the affected
  dispatches: a bounded dispatch check across Step 3 (Plan review write-in and
  difference `/plan-simplicity-review`) and Step 4 (`before the first edit`
  pre-edit checks) verifies that main has enough information to route pre-edit
  work without reading full check bodies, that each brief cites `Temp/` receipt
  or snapshot selectors plus finding, check, or item IDs, that no brief repeats
  finding prose, change prose, or execution-card fixture text inline, and that
  the cited selectors carry all evidence the dispatched workers need.
- Every modified skill package passes
  `pwsh -NoProfile -File .agents/skills/external-skill-creator/scripts/Validate-Skill.ps1 -Path <skill dir>`.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.

## Notes

- Live `/agent-harness` verification does not apply.
- No live Plan dependencies or reciprocal `## Coordination` constraints apply.
