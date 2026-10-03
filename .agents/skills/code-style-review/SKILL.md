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
- `Paths` — repository-relative path prefixes restricting a session-changed
  scope, each matching that path or anything below it; absent to review the
  whole session.
- `Jev` — `skip`, supplied only after the user says "skip jev"; the worker then
  never runs the judgment script. Absent otherwise.

The scanner also has a whole-file mode that lists the `style-rule-<n>`
candidates on every line of named tracked `*.h`/`*.cpp` files, each a path
relative to the repository root with no `./` prefix:
`pwsh -NoProfile -Command "& '.agents/scripts/Find-SessionCandidates.ps1' -RepositoryRoot '<absolute repository toplevel>' -Path '<file>','<file>'"`.
A review never runs it; a caller passes one directory's files per run.

## Handoff

Return the shared handoff form in
[`../../references/subagent-handoff.md`](../../references/subagent-handoff.md)
`## Handoffs`, extended with these fields placed before `Residuals`:

- `Scope` — session-changed ranges or the caller-supplied cleanup scope; plus
  which of the ranges or the scan was unavailable, with its code.
- `Fixes Applied` — one row per fix: file:line, Rule N, correction; or none.
- `Renames and Required Builds` — one row per rename: old → new, propagated
  C++ references; or none.
- `Routed Findings` — routed candidates with file:line, proposed finding,
  classification or domain-review route; or none. Give one row per candidate,
  including confirmed rule 49 entries, without repeating them in `Judgment`.
  Full rows may move to evidence under the shared Handoffs overflow rules;
  retain the routed-candidate count inline when they move.
- `Documentation Residuals` — one row each: identifier, file:line, and
  `/update-claude-docs` or the caller; or none.
- `Functions/regions touched` — one row per function or region, or none.
- `Judgment` — `confirmed: <count>; false flags: <count>` for adjudicated
  flagged rule 49 entries; `none` when the script returned zero flagged rule 49
  entries; `skipped (user)` on `Jev: skip`; `not applicable (cleanup scope)`
  for a caller-supplied scope; `not run — <code>: <message>` when the script
  returned no usable result; plus one `partial — <message>` row on a
  `blocks.partial` result. Counts cover only entries available for adjudication.

The shared `Evidence` field cites
`Temp/code-style-review-judgment.json` with the `status`, `code`,
and `blocks` selectors when that result document exists, and
`blocks[*].flagged` only where present. For
adjudicated flagged rule 49 entries, it also cites
`Temp/code-style-review-adjudication.md ## Judgment` for the complete per-entry
decisions. Per-entry adjudication rows are evidence, not required
inline text.

The shared `Build required` field names the exact affected targets, or `none`.
Each shared `Residuals` row names an unresolved item; use `none` when absent.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: the numbered review,
  rename, and cleanup steps and the judgment rules behind them.
