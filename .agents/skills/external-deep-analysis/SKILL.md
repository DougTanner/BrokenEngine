---
name: external-deep-analysis
description: >-
  Run a two-phase C++ analysis pipeline on a file or directory: architecture
  review for cross-file shape, then refactor-clean for in-function mechanics.
  Verify findings, route proven debt through the repository follow-up-plan and
  finalization workflows, and report an area debt score. Use only when the user
  explicitly requests a deep or full code analysis; never select it for routine
  changes or general code questions. Security auditing is excluded.
disable-model-invocation: true
allowed-tools: [Read, Write, Edit, Grep, Glob, Agent, Bash, PowerShell]
---

# Deep Analysis Pipeline

## Purpose

When the Phase-0 triage gate passes, run the two native analysis skills in
order, verify their findings independently, then hand proven residuals to the
repository's plan-authoring and finalization workflows.

## When to use

- The user explicitly requests a deep or full code analysis.
- Never for routine changes or general code questions.
- Security auditing is excluded.

## Inputs

Require a target path. For a directory, default to files directly within it;
include descendants only when the user explicitly requests recursion. State the
resolved mode and exact target in every phase invocation. An exact-file target
remains exact-file scope.

## Handoff

After successful verification and the applicable finalization outcome,
report:

- exact target, scope mode, file count, and applicable authorities;
- metric profile, target and corpus coverage, the target's and corpus's
  `structuralErosion`, `verbosity`, and `excessDecisions` values, scoped-hint total/emitted counts,
  and every metric residual, so successive runs over the same target show the
  trend;
- architecture and refactor-clean finding counts;
- created, updated, duplicate-mapped, rejected, and residual items, with every
  oversized file still shown as `run /reduce-file <path>`;
- each authored plan's final tier trigger, acceptance boundary, and dependency
  outcome from `create-follow-up-plans`;
- one area Debt Score: LOW for nearly all Quick Win/Small, MODERATE for
  mostly Small/Medium, HIGH for multiple Large or any Architectural, and
  CRITICAL for several Architectural items or a core-invariant threat;
- verification result, finalization outcome, and tracked Plan validation state.

A `triage skip` reports only the target and metric bullets plus its evaluated
signals; the remaining bullets do not apply.

The Debt Score is a run retrospective only; never put it in a Plan. Do not call
a written Plan claimed or scheduler-visible until finalization confirms its
tracked bytes landed.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The phase steps and rules the pipeline
  runs.
