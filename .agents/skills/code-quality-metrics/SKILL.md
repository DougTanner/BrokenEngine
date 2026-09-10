---
name: code-quality-metrics
description: >-
  Capture deterministic C++ code-quality metrics for an exact file, directory, or recursive
  repository scope, or compare a listed set of target paths against a full Git baseline.
  Use when a quality snapshot, clone/complexity trend, or review advisory is needed without
  changing source, grading contributors, or automatically prescribing refactors.
allowed-tools: [Read, PowerShell]
---

# Code Quality Metrics

## Purpose

Capture deterministic C++ code-quality metrics for an exact file, directory, or
recursive repository scope, or compare a listed set of target paths against a
full Git baseline.

## When to use

Any of these, without changing source, grading contributors, or automatically
prescribing refactors:

- A quality snapshot is needed.
- A clone or complexity trend is needed.
- A review advisory is needed.

Per-path and per-line-range `bt-token-v1` counts come from
`.agents/scripts/Measure-Tokens.ps1`, not from this skill; the root
[AGENTS.md](../../../AGENTS.md) bundled-script rule owns its invocation form.

## Inputs

Supply these fields on the task brief form in
[`subagent-reporting.md`](../../references/subagent-reporting.md) `## Task brief`.
The per-mode invocation sections of [`references/worker.md`](references/worker.md)
own the exact parameter spellings for `Snapshot`, `Compare`, and
`BootstrapIdentity`; [`references/HistoryContract.md`](references/HistoryContract.md)
`## Invocation` owns them for the history modes. Do not restate a command line in
the brief.

- Mode: `Snapshot`, `Compare`, `BootstrapIdentity`, or the history modes
  `Contract` and `Generate` (worker `### Snapshot`, `### Compare`,
  `### Bootstrap identity`, `### History`).
- Target path and scope kind — `Exact`, `Directory`, or `Recursive` — for
  `Snapshot` (worker `### Snapshot`).
- Targets file and full-SHA baseline for `Compare` (worker `### Compare`).
- Full-SHA base commit and tip commit for `Contract` and `Generate`, plus for
  `Generate` the UTC date in `YYYY-MM-DD` form and a new output directory
  beneath `Temp/`.
- The absolute repository root, which every mode takes.

## Handoff

Report the result as advisory evidence. Name the scope, coverage omissions, suppression reasons, and
comparison cohort before interpreting a delta. Do not turn a metric into a landing gate, person
score, or automatic refactor instruction.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The invocation steps and the per-mode
  contracts.
