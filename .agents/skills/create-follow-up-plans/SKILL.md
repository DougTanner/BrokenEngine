---
name: create-follow-up-plans
description: Converts proven pre-existing or out-of-scope Change Workflow residuals into concise, evidence-backed follow-up Plans under `Documents/Plans/<area>/` with tracked scheduler metadata. Do not route an in-scope acceptance failure out of the active change. Also use when asked to record review findings without duplicating existing Plans, and for tooling-friction follow-ups recorded at a /next-plan claim exit.
allowed-tools: [Read, Write, Edit, Glob, Grep, PowerShell]
---

# Create Follow-up Plans

## Purpose

Turn eligible residuals into concise, evidence-backed executable debt Plans
under `Documents/Plans/<area>/`, with tracked scheduler metadata.

## When to use

- A Change Workflow residual is proven pre-existing or outside the approved
  implementation boundary of the active change.
- Review findings must be recorded without duplicating an existing Plan.
- A tooling-friction follow-up is recorded at a `/next-plan` claim exit.
- Not for routing an in-scope acceptance failure out of the active change.

## Inputs

- `Objective` — the active intent or plan the proposals arose from.
- `Scope` — affected symbols and files, and the session changed-file list.
- `Fixed decisions` — prior reviewer or user decisions to preserve.
- `Evidence` — direct finding evidence, the originating step and unmet
  acceptance criterion, and related residuals.
- `Session provenance` — supplied only for a tooling-friction or
  context-efficiency follow-up; an ordinary debt follow-up takes none. A
  `/next-plan` run sources the values per
  `.agents/skills/next-plan/references/follow-up-provenance.md`.

## Handoff

Return the shared handoff from `.agents/references/subagent-reporting.md`
`## Handoffs`, including every proposal exactly once, with these rows:

- `Findings`: none.
- `Changed files`: one row per created Plan (path — gap and metadata); one row
  per updated existing Plan (path — prose-only or dependency update).
- `Decisive checks`: one row per duplicate mapping (proposal -> existing Plan
  path); one row per Plan with its Change Workflow tier and trigger and its
  dependencies and Coordination; one row for the plan validate result; one row
  for the required verification/finalization route.
- `Build required`: none.
- `Residuals` stays last: unrecorded item, conflict, or blocker and reason, or
  none.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: the steps and the rules
  governing them.
