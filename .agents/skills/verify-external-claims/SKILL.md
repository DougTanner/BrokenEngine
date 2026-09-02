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

Resolve requested external facts for the caller to decide.

## External Claim Requests

Require each request to contain:

- a stable claim ID, API/symbol/rule, and one checkable proposition;
- the dependent finding, item, or decision and why the verdict matters;
- applicable project target, version, platform, extensions/features, flags, or
  constraints, plus the smallest relevant repository paths or symbols;
- a proposed official URL or exact upstream identifier when known.

## Handoff

Return the complete evidence inline:

```text
Sources:
- <claim ID> — <official source identity and version/revision/tag/commit; exact section/symbol/citation; optional official immutable link | none — exact unavailable evidence>
Decisive checks:
- <claim ID> — applicability: <path:line and target/version/extensions/features/flags | none — exact missing configuration>; rule: <short evidence that settles the question on its own | none — exact missing primary evidence>
Per-proposition verdicts:
- <claim ID> — VERIFIED | REFUTED | UNRESOLVED — <exact proposition> — <direct implication for dependent item, without deciding it>
```

Complete the report with the remaining shared handoff lines
(`../../references/subagent-reporting.md`, `## Handoffs`); this read-only
workflow never changes a file and never requires a build, and each unresolved
claim with its exact missing evidence belongs in `Residuals`. Preserve exact
citations; do not replace evidence with a summary.

## References

- [`references/worker.md`](references/worker.md) — worker entry: the identifier,
  delegation, and result-handling steps, and the rules. Main itself runs those
  steps, dispatching the `locator`, so main reads this file to run the skill.
