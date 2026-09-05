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

## Handoff

Report the result as advisory evidence. Name the scope, coverage omissions, suppression reasons, and
comparison cohort before interpreting a delta. Do not turn a metric into a landing gate, person
score, or automatic refactor instruction. Interpret `excessDecisions` as net scope evidence:
unchanged means only no net decision removal, never redistribution without source-diff evidence. A
target decrease proves simplification only when the diff shows decision removal and the corpus shows
no attributable offset elsewhere, or a separately evidenced structural benefit independently
justifies extraction. It is not an outlier or Phase-0 hint metric.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The invocation steps and the per-mode
  contracts.
