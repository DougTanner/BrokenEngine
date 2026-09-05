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

- After session C++ changes, as the Change Workflow cleanup step.
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

## Handoff

Return the shared handoff form in
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
`## Handoffs`, extended with these fields placed before `Residuals`:

```text
Scope: session-changed ranges | caller-supplied cleanup scope
Fixes Applied: <one row each: file:line — Rule N — correction, or none>
Renames and Required Builds: <one row each: old → new — propagated C++
  references, or none>
Build required: <exact affected targets, or none>
Routed Findings: <one row each: file:line — proposed finding —
  classification/domain-review route, or none>
Documentation Residuals: <one row each: identifier — file:line —
  `/update-claude-docs` or caller, or none>
Functions/regions touched: <function or region, or none>
Residuals: <unresolved item, or none>
```

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: the numbered review,
  rename, and cleanup steps and the judgment rules behind them.
