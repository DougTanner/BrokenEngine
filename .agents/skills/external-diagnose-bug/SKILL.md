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

## Handoff

```markdown
Root cause: <one sentence, with file:line>
Evidence: <inspection, command output, or log lines that prove it>
Reproducing signal: <exact command or inspection, and its red result>
Hypotheses ruled out: <hypothesis — the check that killed it>
Proposed acceptance check: <check matching the signal — harness scenario, replay check, compile result, or profiling baseline>
Instrumentation removed: yes — <marker searched> | none added
```

Follow those extension fields with the shared handoff lines
(`../../references/subagent-reporting.md`, `## Handoffs`); this diagnosis-only
workflow reports `Status: DIAGNOSED | BLOCKED` instead of the shared values and
always reports `Changed files: none`, `Build required` names the exact targets
the manager must rebuild or `none`, and an unproven branch or missing
environment belongs in `Residuals`.

Use `DIAGNOSED` only with a confirmed root cause. Use `BLOCKED` when no
reproducing signal could be built, naming what you need.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The numbered diagnosis steps and rules.
