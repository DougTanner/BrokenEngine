---
name: resolve-findings
description: >-
  Resolve an explicitly accepted review finding, compile failure, or runtime
  failure within the Broken Engine Change Workflow. Use for delegated fix work
  after the manager supplies fixed evidence, intent classification, scope, and
  session baseline.
allowed-tools: [Read, Grep, Glob, Edit, "Bash(git diff *)", "Bash(git status *)", PowerShell]
---

# Resolve Findings

## Purpose

Fixes only the review findings, compile failures, and runtime failures the
manager explicitly accepted and assigned, and returns the fix handoff. Runs
inside one delegated `implementer` that does not delegate.

## When to use

- The manager has accepted a review finding, compile failure, or runtime failure
  and assigns the fix, supplying its evidence, classification, scope, and
  session baseline.

## Inputs

Require a self-contained assignment carrying the authoritative task-brief fields
(`../../references/subagent-reporting.md`) plus these skill-specific fields:

- accepted finding or failure evidence and its prescribed check;
- classification: intent `conformance` or `plan_delta`, and scope
  `non_structural` or `structural`;
- assigned files/functions, session baseline, and pre-existing ownership
  snapshot;
- known build target/configuration when relevant.

Reconstruct a missing detail only when the assignment and worktree make it
unambiguous. Otherwise do not edit; report the missing input as a residual.

Accept only `conformance + non_structural`. Report
`PLAN DELTA REQUIRED: yes` without editing when the correction would change
approved behavior, scope, acceptance criteria, or verification obligations.
Structural work also returns to the manager: an in-scope acceptance failure
blocks the active change; proven pre-existing or out-of-scope work may become a
follow-up; user-approved expanded scope re-enters `/implement-plan` after the
manager updates the authoritative plan.

## Handoff

Return the shared handoff form in `../../references/subagent-handoff.md`,
extended with one compact item table and these fields:

```markdown
| Item | Result | Confirmed root cause and evidence | Fixed region |
|---|---|---|---|
| <item> | <change made, or none with reason> | <path:line or log evidence> | <path:line or none> |
```

Each shared `Decisive checks` row names the item, focused check, and result.
Each cell holds its value and at most one clause; the reasoning behind it
follows the over-cap file rule in `../../references/subagent-handoff.md`,
`## Handoffs`.

- `PLAN DELTA REQUIRED` — `no`, or `yes` with the reason and the manager
  action.
- `Self-audit resolved` — one row each: Claim -> Check -> Result, with its fix
  and recheck; or none.
- `Affected-site triggers` — one row each: kind, symbol/pattern, and search
  scope; or `none found`.
- `Propagation required` — `/update-affected-code` with the code scope, or
  `N/A — no code changed`.
- `External/API verification requests` — one row each: symbol/rule,
  proposition, dependent item, version/configuration, and candidate official
  source; or none.
- `Reviewer focus areas` — the condition the independent verifier must try to
  disprove, or none.

Name each changed file once, and when the changed set would push a field past
the shared row cap give the count in `Changed files` and move the per-file rows
to the file cited under `Evidence` as path plus `##` selector, on the shared
form's terms. Each shared `Build required` row names its distinct target and
that target's configuration/platform inline in every case; only the per-file
attribution — which selected project-member `.cpp` belongs to which target, and
which targets consume a changed header — moves with the overflow. Build
requests must be executable without rediscovery wherever the per-`.cpp` detail
sits, inline or in that cited file. Each
shared `Residuals` row names the unresolved or out-of-scope item, its
evidence, and the next owner/action. A requested build is `builder` work
dispatched by the manager, not a passed check. The manager dispatches
independent verification as a separate role after the fix and required checks
complete. Use `PASS` when every assigned item is fixed with no fix-work
residual, `NEEDS_ACTION` when manager action remains, and `BLOCKED` when
missing required evidence prevents work.

A round that resumes this worker with a newly assigned item returns this same
handoff, narrowed to that round: the item table carries only the newly assigned
item's row, `Changed files` only the rows this round produced, and `Decisive
checks` only this round's rows; the extension fields keep their shapes and
report this round's state.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Fix steps and rules for the dispatched
  worker.
