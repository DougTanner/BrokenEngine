---
name: code-style-review
description: Reviews and auto-fixes provably meaning-preserving C++ style violations in session-changed ranges or an explicit cleanup scope, over a fixed subset of the `Documents/C++StyleGuide.txt` rules. Use after C++ changes or for a requested style, naming, or formatting cleanup; routes semantic candidates for classification instead of changing behavior.
allowed-tools: [Read, Write, Edit, Grep, PowerShell]
---

# Code Style Review

## Purpose

Fixed violations of the `Documents/C++StyleGuide.txt` rule subset
[`references/worker.md`](references/worker.md) names, in the selected ranges,
plus the session's residue removed and semantic candidates routed to the caller.

## When to use

- After session C++ changes, at the Change Workflow Run targeted pre-review
  checks step.
- For a requested C++ style, naming, or formatting cleanup over a scope the
  caller supplies.
- Not for shader-only or non-C++ changes, and not for behavior or interface
  defects, which are `/repo-code-review` work.
- Not for comment content — what a comment says and whether it should exist —
  which is `/comment-review` work.

## Inputs

- `Scope` — the C++ files and ranges of a caller-supplied cleanup scope, or
  none to review the ranges changed in this session.
- `Baseline` — the full 40-character session baseline SHA and the absolute
  repository toplevel, required for a session-changed scope, plus any untracked
  paths the review must cover.
- `Jev` — `skip`, supplied only after the user says "skip jev"; the worker then
  hand-reads the gated rules itself and never runs the judgment script. Absent
  otherwise.

## Handoff

Return the shared handoff form in
[`../../references/subagent-handoff.md`](../../references/subagent-handoff.md)
`## Handoffs`, extended with these fields placed before `Residuals`:

- `Scope` — session-changed ranges, or the caller-supplied cleanup scope.
- `Fixes Applied` — one row per fix: file:line, Rule N, correction; or none.
- `Renames and Required Builds` — one row per rename: old → new, propagated
  C++ references; or none.
- `Routed Findings` — one row per routed candidate: file:line, proposed
  finding, classification or domain-review route; or none.
- `Documentation Residuals` — one row each: identifier, file:line, and
  `/update-claude-docs` or the caller; or none.
- `Functions/regions touched` — one row per function or region, or none.
- `Judgment` — one row per flagged gated-rule entry: `path:line`, the rule, its
  probability, and `confirmed` or `false flag` from adjudication; `none` when
  the script returned zero flagged entries; `skipped (user)` when the fallback
  ran on `Jev: skip`; `not applicable (cleanup scope)` for a caller-supplied
  scope; `not run — <code>: <message>` with `Status: BLOCKED`.

A `BLOCKED` handoff from the judgment step halts the workflow: main tells the
user the code and message, and any re-dispatch is user-directed — with the
`Jev` input (`## Inputs`) when the user directs it, without it when the user
reports the cause fixed (key set, service back).

The shared `Build required` field names the exact affected targets, or `none`.
Each shared `Residuals` row names an unresolved item; use `none` when absent.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: the numbered review,
  rename, and cleanup steps and the judgment rules behind them.
