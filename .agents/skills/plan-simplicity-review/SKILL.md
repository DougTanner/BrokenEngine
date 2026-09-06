---
name: plan-simplicity-review
description: >-
  Review an implementation plan that adds new code or modifies
  non-documentation behavior (the root AGENTS.md Plan review step owns when to
  dispatch; this skill's `## When to use` owns what counts as new code and
  behavior) for YAGNI/KISS violations before implementation: ultra-rare
  edge-case handling, speculative hardening, problems a plainly simpler
  mechanism or a deferred ASSERT would solve, and bandaid fixes that suppress
  a symptom instead of removing its root cause. Runs at every tier inside
  one delegated `reviewer`, in parallel with /plan-audit where that runs;
  skip a plan for which the trigger does not fire. Findings only, with no
  edits, harness work, user interview, or further delegation.
allowed-tools: [Read, Grep, Glob, PowerShell]
---

# Plan Simplicity Review

## Purpose

Judge one question: are the plan's changes the simplest complete way to serve
the repository? Returns findings only, each with a simpler alternative or the
options the user must choose between.

## When to use

Use this skill at every tier, for every implementation plan with a step that
adds new code or that modifies non-documentation behavior, tracked executable
Plans and session-prepared plans alike; a plan for which the trigger does not
fire skips it.

New code means a new tracked file, function, class, system, script, guard or
recovery path, or configuration surface absent at the session baseline —
including an addition inside an existing file. Non-documentation behavior means
C++, shaders, scripts, or the skill behavior the Trigger section below defines.

Dispatch it once per `../../references/subagent-reporting.md`, in parallel with
`/plan-audit` on the same plan snapshot where `/plan-audit` runs (Tier 2+) and
standalone where it does not (Tier 1, and the save-time dispatch from
`/save-plan`).

If later planning adds substantial implementation complexity, main runs this
skill once more on the final plan before implementation; it does not loop, and
any further expansion is a separately approved planning stage.

### Trigger: when a skill edit is behavior

A skill edit is behavior when it changes frontmatter, an invocation, delegation,
or routing rule, a bundled script or that script's documented invocation, a
workflow step or its ordering, an input, output, or handoff contract, a trigger
condition, or a threshold. It is documentation — which takes precedence,
including for a frontmatter edit — when it only rewords, reorders, formats,
retitles, or clarifies behavior that stays exactly as it was.

## Inputs

- `/save-plan`: the final unsaved Plan body snapshot, supplied inline or by
  exact file path.
- Change Workflow preparation or a bounded final rerun: the manager supplies
  the final complete plan presented for implementation, including the
  execution card and exact refinements when they carry resolved work, inline
  or by exact file path; on a bounded final rerun, treat that supplied final
  plan as the review input.
- The manager's trigger evidence: which plan steps add code or modify
  non-documentation artifacts.
- On a bounded final rerun, the manager's trigger evidence and classification
  also identify which steps materially enlarge the solution.
- User intent and known residuals.

## Handoff

For each finding:

> `PSR-F-###` Critical|Required|Recommended `plan-path:line` — class — concrete problem — simpler alternative — disposition: simplify | user-judgment — evidence: `<locator>`

Each finding's concrete problem, simpler alternative, and disposition appear
exactly once, as one single-line `Findings` row of the shared handoff in the
form above; every other field that refers to a finding names it by its
`PSR-F-###` ID and adds only its own required content, never restating that
finding's problem, alternative, or disposition. For an inline plan,
`plan-path:line` is the line in the supplied plan text. The class is one of
`rare-edge-case`, `speculative-hardening`, `overbuilt-mechanism`,
`assert-and-defer`, `bandaid-fix`, `plan-not-worth-executing`. The simpler
alternative is the applicable Q3-Q9 result, including Q6's root-cause fix sketch
for a `bandaid-fix`; for a `user-judgment` disposition it is instead the options
with their one-line costs and a recommendation. The evidence locator is one
stable `repository-path:line`, `plan-path:line`, supplied artifact selector, or
named command output; write `none supplied` only when Q1 classifies the case as
hypothetical because occurrence evidence is missing.

A `plan-not-worth-executing` finding cites the plan's goal or title line and is
always the only finding in the report.

Its row states what replaces the plan — nothing, or a much smaller change.

For a run with findings, the Q1 occurrence/likelihood classification and its
supporting detail, the full Q2 cost comparison, and the per-question judgment
notes travel in a gitignored `Temp/` file, under one `## PSR-F-###` heading per
finding, cited under `Evidence`. The handoff does not restate them.

Return:

```text
Steps reviewed: <added-code and modified-artifact steps>
```

Follow that extension field with the shared handoff lines
(`../../references/subagent-reporting.md`, `## Handoffs`); this findings-only
review never changes a tracked file and never requires a build.

Use `NEEDS_ACTION` for findings and `BLOCKED` only when a required input is
missing.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The reviewer's steps and judgment rules.
- [`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
  — delegated execution context and the shared handoff form.
