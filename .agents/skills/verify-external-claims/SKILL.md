---
name: verify-external-claims
description: >-
  Verify reviewer-requested external API, language, specification, or library
  claims without editing the repository. Use when a Broken Engine review, plan
  audit, grill, or finding-resolution pass needs an authoritative verdict on
  non-obvious external behavior. Returns one verdict per single checkable
  claim.
allowed-tools: [Read, Grep, Glob, Agent]
---

# Verify External Claims

## Purpose

One `locator` resolves requested external facts for main to decide.

## Inputs

Dispatch one `locator` with the authoritative shared task-brief fields from
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
`## Task brief` and require each request to contain:

- a stable claim ID, API/symbol/rule, and one checkable proposition;
- the dependent finding, item, or decision and why the verdict matters;
- applicable project target, version, platform, extensions/features, flags, or
  constraints, plus the smallest relevant repository paths or symbols;
- a proposed official URL or exact upstream identifier when known.

## Handoff

The `locator` returns one row per claim under each of `Sources`,
`Decisive checks`, and `Per-proposition verdicts`.

Each `Sources` row is one line on this form:

```text
<claim ID> — <official source identity and version/revision/tag/commit; exact section/symbol/citation; shortest decisive quotation or faithful rule statement, or for an undecided claim the closest relevant one; optional official immutable link> | <none — exact unavailable evidence>
```

Each `Decisive checks` row is one line on this form:

```text
<claim ID> — applicability: <path:line and target/version/extensions/features/flags | none — exact missing configuration>
```

Each `Per-proposition verdicts` row is one line on this form:

```text
<claim ID> — VERIFIED | REFUTED | UNRESOLVED — <exact proposition> — <direct implication for dependent item, without deciding it>[ — missing: <exact primary evidence a found source leaves undecided, UNRESOLVED only>]
```

Complete the report with the remaining shared handoff lines
(`../../references/subagent-handoff.md`, `## Handoffs`); this read-only
workflow never changes a tracked file and never requires a build, and
`Residuals` names each unresolved claim by the claim ID of its `UNRESOLVED`
`Per-proposition verdicts` row instead of restating its evidence. Preserve
external source citations and their decisive quotations once, in each `Sources`
row, inline or in the over-cap file; do not replace a row's evidence with a
summary. Main presents the evidence and decides every dependent finding, item,
or plan choice from the returned verdicts.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The locator's identifier, research,
  evidence-validation, verdict, and status steps and rules.
