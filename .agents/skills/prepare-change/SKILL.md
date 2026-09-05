---
name: prepare-change
description: >-
  Prepare a change before its plan reviews: gather the repository evidence that
  fixes the risk tier and, at Tier 2+, draft the implementation plan as a file
  and the execution card. Use for the `implementer` preparation the Change
  Workflow Approve and classify and Prepare and explore alternatives steps
  require, for a change that is not a claimed executable Plan. Preparation only;
  never implements the change.
allowed-tools: [Read, Write, Edit, Glob, Grep]
---

# Prepare Change

## Purpose

One `implementer` produces the repository evidence that fixes the risk tier, and,
for the Tier-2+ plan preparation the Prepare and explore alternatives step
dispatches, the implementation plan as a file with the draft execution card
inside it. A Tier-1 dispatch made only to
classify the work returns the evidence alone.

## When to use

- The `implementer` preparation bullets of the Approve and classify and Prepare
  and explore alternatives steps in root
  [AGENTS.md](../../../AGENTS.md) — Tier 2 and above, or any tier where
  classifying the work needs repository evidence.
- Not for a claimed executable Plan: [`../next-plan/SKILL.md`](../next-plan/SKILL.md)
  keeps its own preparation route.
- Runs in the delegated execution context of
  [`../../references/subagent-reporting.md`](../../references/subagent-reporting.md).

## Inputs

Supply the authoritative task-brief fields
([`../../references/subagent-reporting.md`](../../references/subagent-reporting.md))
plus these skill-specific fields:

- user intent, quoted where its exact wording binds;
- the request text, or the plan document this change comes from when it is not a
  claimed executable Plan;
- session baseline.

## Handoff

Return the shared handoff form in
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md),
extended with these fields:

```text
Prepared plan: <path readable from the worktree root, carrying `## In scope` and `## Out of scope` verbatim; `none` when the dispatch prepared no plan>
Classification: Tier 1 | Tier 2 | Tier 3 — trigger, with repository-path:line evidence
Execution card: <heading inside the prepared plan file, headed as `/plan-audit` `## Inputs` requires; `none` when the dispatch prepared no plan>
Unresolved decisions: <one row each, decision and the options main chooses between, or none>
```

When a plan was prepared, main passes that one file to `/plan-audit`, which reads the scope headings and
the card marker from the supplied path alone
([`../plan-audit/SKILL.md`](../plan-audit/SKILL.md), `## Inputs`); never inline
the plan or card text in the handoff.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The steps and rules the dispatched
  worker follows.
- [`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
  — task-brief fields and the shared handoff form.
- [`../plan-audit/SKILL.md`](../plan-audit/SKILL.md) — the plan-file and
  execution-card contract this preparation must satisfy.
