---
name: external-diagnose-bug
description: >-
  Find and prove the root cause of a bug or performance regression before any
  fix exists. Use when the user says "diagnose" or "debug this", or reports
  something misbehaving, desyncing, mismatched CRC, or slow. Diagnosis only —
  never fixes, commits, or lands.
allowed-tools: [Read, Grep, Glob, Edit, PowerShell]
---

# Diagnose Bug

## Purpose

Own the front half of a bug: from "something is broken" to a root cause proven
by evidence. `/resolve-findings` and the Change Workflow own the fix; this skill
stops at the handoff.

Adapted from an external MIT-licensed skill; see [LICENSE](LICENSE).

## When to use

- The user says "diagnose" or "debug this".
- The user reports something misbehaving, desyncing, mismatched CRC, or slow.

## Inputs

Require the reported symptom and expected behavior, the reproduction scope,
the session baseline when one exists, and every supplied log, capture, command
result, or other evidence. Name any missing input needed to build a reproducing
signal.

## Handoff

Return the shared handoff form in
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md),
extended with these fields:

- `Root cause` — one sentence, with file:line.
- `Diagnosis evidence` — the inspection, command output, or log lines that
  prove it.
- `Reproducing signal` — the exact command or inspection, and its red result.
- `Hypotheses ruled out` — one row each: the hypothesis, and the check that
  killed it.
- `Proposed acceptance check` — a check matching the signal: harness scenario,
  replay check, compile result, or profiling baseline.
- `Instrumentation removed` — `yes` with the marker searched, or `none added`.
- `Build required` — the exact targets the manager must rebuild, or none.
- `Residuals` — unproven branch, missing environment/input, or none; last.

`Changed files` is `none` because a diagnosis never edits a file.

Use `PASS` only with a proven root cause. Use `BLOCKED` when the diagnosis
cannot be proven, naming what evidence, input, or environment is still needed.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The numbered diagnosis steps and rules.
