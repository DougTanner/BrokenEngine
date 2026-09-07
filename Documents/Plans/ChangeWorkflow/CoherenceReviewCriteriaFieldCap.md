<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T17:24:38.559Z","dependsOn":[]} -->
# Fix: /coherence-review — Tier-1 `Criteria` field overruns the handoff cap and restates cited evidence

## Context

`.agents/skills/coherence-review/SKILL.md` `## Handoff` declares a `Criteria`
extension field for its Tier-1 combined pass and defines it by reference to
`.agents/skills/verify-acceptance/SKILL.md` `## Handoff`, which requires "one row
per approved criterion and invariant, one line each, in the approved order".

Observed symptom in this session: the Tier-1 combined `/coherence-review` pass
returned a 7,074-character handoff whose `Criteria` field carried 12 rows. That
exceeds the 10-row per-field cap in `.agents/references/subagent-reporting.md`
`## Handoffs`, and the rows restated evidence inline — quoted file text, grep
counts, and spot-check lists — that the same handoff had already cited under
`Evidence` as a `Temp/` path plus selector. The manager had to read duplicated
evidence out of an over-cap field to decide the pass.

The two contracts conflict as written: `Criteria` requires one row per approved
criterion, which for an approved set larger than ten cannot satisfy the shared
10-row field cap, and neither contract says what the row's evidence cell should
contain, so a worker can inline the full evidence it has already cited
elsewhere. The wording is therefore the defect rather than worker conduct.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 69bbf8b5-adb7-4a69-819f-cb2508d8fbc0
- Worktree/branch UUID: 9698e458-3242-4687-bda8-debbe6e9fd4a
- Session branch: claude/9698e458-3242-4687-bda8-debbe6e9fd4a
- Worktree: .claude\worktrees\BrokenEngine\9698e458-3242-4687-bda8-debbe6e9fd4a
- Landing ref: claude/9698e458-3242-4687-bda8-debbe6e9fd4a
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design

First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the
recorded client and conversation session ID above. Then make the smallest fix
inside the `## In scope` boundary below.

The author's recommendation, to be confirmed by that root-cause pass, is a
wording change in one place only — either the `Criteria` definition in
`.agents/skills/verify-acceptance/SKILL.md` `## Handoff` or the Tier-1 combined
mode guidance in `.agents/skills/coherence-review/SKILL.md` `## Handoff`,
whichever root-causing shows is the owning layer — that states two things the
current text leaves open:

- each row is criterion — verdict — evidence citation as path plus selector,
  rather than inlined evidence text the handoff cites elsewhere; and
- a criteria set larger than the shared 10-row field cap follows the existing
  over-cap rule in `.agents/references/subagent-reporting.md` `## Handoffs`,
  moving the full row set to an evidence file cited as path plus selector, with
  the handoff still carrying what the manager needs to decide.

The recommendation is to add no new mechanism: the over-cap route already exists
and the fix should reference it rather than restate it. If root-causing shows the
fix lies outside the boundary below, surface it for re-planning instead of
expanding scope.

## Critical files

- `.agents/skills/coherence-review/SKILL.md` — `## Handoff`, Tier-1 combined mode
- `.agents/skills/verify-acceptance/SKILL.md` — `## Handoff`, the `Criteria`
  definition
- `.agents/references/subagent-reporting.md` — `## Handoffs`, read-only authority
  for the field cap and the over-cap route

## In scope

- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `## Handoff` sections of
  `.agents/skills/coherence-review/SKILL.md` and
  `.agents/skills/verify-acceptance/SKILL.md`

## Out of scope

- The landed change the session produced
- Changing `.agents/references/subagent-reporting.md`, the shared handoff form,
  the 10-row field cap, or the over-cap rule
- Changing what either skill reviews, its steps, its triggers, or any other
  handoff field
- Unrelated skills and scripts; any transcript path or transcript text in the
  repository

## Risk tier and invariants

Expected Tier 2: the edit changes a handoff contract, which
`/plan-simplicity-review` `### Trigger: when a skill edit is behavior` classifies
as behavior rather than documentation. Escalate if the fix reaches
build/bootstrap coordination. Invariants: every approved criterion and invariant
still gets its own verdict somewhere the manager can read; `Status` stays `PASS`
only when every criterion passes; the shared cap and over-cap route stay
authoritative and unduplicated. Never embed transcript paths or home paths.

## Acceptance criteria

- The `Criteria` contract states the row form as criterion, verdict, and an
  evidence citation given as path plus selector, in exactly one owning file.
- The `Criteria` contract routes a criteria set over the shared field cap through
  the existing over-cap rule by reference, without restating that rule.
- A Tier-1 combined `/coherence-review` pass over an approved set larger than the
  cap can produce a handoff within the shared per-field and whole-handoff caps
  with no evidence text duplicated between `Criteria` and `Evidence`.
- `/validate-skill` passes for both edited skill packages, and
  `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` with `code: ok`.
