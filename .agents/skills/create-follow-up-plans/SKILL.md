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

The worker creates the Plan file with the repository-owned Plan-file writer
script, so a brief must not name a writer.

## Handoff

Return the shared handoff from `.agents/references/subagent-handoff.md`
`## Handoffs`, including every proposal exactly once. The shared `Findings`
value is `none`. Each shared `Changed files` row names a created Plan with its
path, gap, and metadata, or an updated existing Plan with its path and whether
the update is prose-only or a dependency update. Each shared `Decisive checks`
row names a duplicate mapping from proposal to existing Plan path, a Plan's
Change Workflow tier and trigger plus its dependencies and Coordination, the
plan validate result, or the required verification/finalization route. The
shared `Build required` value is `none`. Each shared `Residuals` row names an
unrecorded item, conflict, or blocker and reason; use `none` when absent.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: the steps and the rules
  governing them.
