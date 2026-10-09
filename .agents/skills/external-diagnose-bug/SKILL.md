---
name: external-diagnose-bug
description: >-
  Find and prove the root cause of a bug or performance regression before a fix
  exists or after one fails its retest. Use when the user says "diagnose" or
  "debug this", or reports something misbehaving, desyncing, mismatched CRC, or
  slow, or when a fix fails its Change Workflow retest. Diagnosis only — never
  fixes, commits, or lands.
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
- A fix fails its retest at the Change Workflow Review and resolve correctness
  step.

## Inputs

Require the reported symptom and expected behavior, the reproduction scope,
the session baseline when one exists, and every supplied log, capture, command
result, or other evidence. A Review and resolve correctness step dispatch also
names that origin, carries the failed retest as evidence, and gives the fix's
stated root cause as an unproven hypothesis. Name any missing input needed to
build a reproducing signal.

## Handoff

Return the shared handoff form in
[`../../references/subagent-handoff.md`](../../references/subagent-handoff.md),
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

The shared `Build required` field names the exact targets the manager must
rebuild, or `none`. Each shared `Residuals` row names an unproven branch or
missing environment/input; use `none` when absent.

`Changed files` is `none` because a diagnosis never edits a file.

Use `PASS` only with a proven root cause. Use `BLOCKED` when the diagnosis
cannot be proven, naming what evidence, input, or environment is still needed.
On a Review and resolve correctness step dispatch, return that `BLOCKED`
wherever the steps would otherwise ask the user.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The numbered diagnosis steps and rules.
