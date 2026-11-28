---
name: implement-plan
description: >-
  Dispatch and perform one assigned slice of an approved Broken Engine plan,
  followed by a same-context audit of the implementation assumptions. Use for
  Change Workflow implementation work, including disjoint slices, and when the
  worker that made existing changes is asked to audit its assumptions.
allowed-tools: [Read, Write, Edit, Glob, Grep, "Bash(git diff *)", "Bash(git status *)", PowerShell]
---

# Implement Plan

## Purpose

One `implementer` makes the smallest complete change for its assigned slice of
an approved plan, then audits its own implementation assumptions in the same
context, and returns a handoff of changed files, triggers, and residuals.

## When to use

- Change Workflow implementation work, including one disjoint slice of it.
- Audit-only mode, when the worker that made existing changes is asked to audit
  its own assumptions.
- If already running as the assigned worker, do not dispatch again.

## Inputs

The main session reads this skill, then dispatches exactly one `implementer`
with a self-contained brief and no inherited conversation context — fresh on
Claude or `fork_turns: "none"` on Codex, per the authoritative default in
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
`## Delegated execution context`.
That worker performs both implementation and its same-context assumption audit.

Supply the authoritative task-brief fields
([`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
`## Task brief`), with `Acceptance` holding only the assigned items' focused
self-checks, plus these skill-specific fields, in both implementation and
audit-only modes:

- mode: `implementation` or `audit-only`;
- final approved plan and execution card (when present), cited per
  `subagent-reporting.md` `## Task brief`, plus assigned items (section and
  label/bullet), allowed file scope, and post-plan user changes (`none` is
  valid);
- session baseline used for all attribution;
- pre-existing ownership snapshot naming every already changed or untracked
  path and its owner/outcome (`none` is valid);
- risk triggers and reviewer focus.

## Handoff

Return the shared handoff form in
[`../../references/subagent-handoff.md`](../../references/subagent-handoff.md)
(`## Handoffs`, including its selector-citation rule), extended with:

- `Self-audit resolved` — one row each: Claim -> Check -> Result, with its fix
  and recheck; or none.
- `Affected-site triggers` — one row each: kind, symbol/pattern, and search
  scope; or `none found`.
- `Propagation required` — `/update-affected-code` with the code scope, or
  `N/A — no code changed`.
- `Reviewer focus areas` — verify X holds when Y, or none.
- `Runtime acceptance requests` — cite the plan/card path plus needed `##`
  selector per `subagent-handoff.md` `## Handoffs`, then add one row per
  criterion missing setup, action, observation, or required evidence there
  (only the missing parts); give all four parts when no cited home exists; or
  none.

Each shared `Build required` row names its distinct target and that target's
configuration/platform inline in every case; when the field would push the
handoff past the shared character cap, only the per-file attribution — which
selected project-member `.cpp` belongs to which target, and which targets
consume a changed header — moves with the overflow below. Each shared
`Residuals` row names a contradiction, incomplete item, or blocker with
evidence, using `none` when absent.

Name each changed file once; past the shared character cap, the overflow file
cited under `Evidence` follows `../../references/subagent-handoff.md`
`## Handoffs`. Build requests must be executable without rediscovery wherever
the per-`.cpp` detail sits, inline or in that cited file: it names the exact
target, configuration/platform, and selected project-member path, and each
changed header names every consuming target and configuration/platform. The
manager dispatches the assigned build/runtime role and routes its concise result
or later fix work.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The steps and rules the dispatched
  worker follows.
- [`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
  `## Task brief` — task-brief fields.
- [`../../references/subagent-handoff.md`](../../references/subagent-handoff.md)
  — the shared handoff form.
