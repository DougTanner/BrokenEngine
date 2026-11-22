---
name: create-follow-up-plans
description: >-
  Converts proven pre-existing or out-of-scope Change Workflow residuals into
  evidence-backed follow-up Plans under `Documents/Plans/<area>/` (or
  `Projects/Coretide/Documents/Plans/<area>/` for Coretide files) with tracked
  scheduler metadata. Never route an in-scope acceptance failure out of the
  active change. Also records non-duplicate review findings, three-row workflow
  incidents, and standing-landing or `/save-plan` follow-up Plans.
allowed-tools: [Read, Write, Edit, Glob, Grep, PowerShell]
---

# Create Follow-up Plans

## Purpose

Turn eligible residuals into concise, evidence-backed executable debt Plans
under `Documents/Plans/<area>/`, or under
`Projects/Coretide/Documents/Plans/<area>/` in the session Coretide clone for a
Plan naming a Coretide file, with tracked scheduler metadata.

## When to use

- A Change Workflow residual is proven pre-existing or outside the approved
  implementation boundary of the active change.
- Review findings must be recorded without duplicating an existing Plan.
- A key of `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md`
  reaches its third row.
- Standing landing confirmation
  ([`../finalize-changes/SKILL.md`](../finalize-changes/SKILL.md)
  `### Landing confirmation`) needs a follow-up Plan, or `/save-plan` needs a
  companion Plan for saved-file gaps bare `/next-plan` will not pick up.
- Not for these proposals, which the worker rejects, stating why:
  - an in-scope acceptance failure, including required structural work: it
    remains a blocker in the active change;
  - a worker's deviation from a handoff form the emitting skill correctly
    declares: the skill's text is already correct and the deviation is
    worker conduct;
  - a stale, disproven, fixed, stylistic-only, or evidence-free proposal.

## Inputs

Dispatch one `implementer` with these fields:

- `Objective` — the active intent or plan the proposals arose from.
- `Scope` — affected symbols and files, and the session changed-file list.
- `Fixed decisions` — prior reviewer or user decisions to preserve.
- `Evidence` — direct finding evidence, originating step/unmet criterion, and
  residuals, or the standing-landing / `/save-plan` evidence in
  [`references/worker.md`](references/worker.md) `## Rules`.
- `Session provenance` — supplied only for a tooling-friction follow-up; an
  ordinary debt follow-up takes none. A `/next-plan` run sources the values per
  `.agents/skills/next-plan/references/follow-up-provenance.md`.

The worker creates the Plan file with the repository-owned Plan-file writer
script, so a brief must not name a writer.

## Handoff

Return the shared handoff from `.agents/references/subagent-handoff.md`
`## Handoffs`, including every proposal exactly once. The shared `Findings`
value is `none`. Each shared `Changed files` row names a created Plan with its
path, gap, and metadata, or an updated existing Plan with its path and whether
the update is prose-only or a dependency update, a support-folder file with its
Plan's path, or `WorkflowIncidents.md` with the deleted key. Each shared
`Decisive checks` row names a duplicate mapping from proposal to existing Plan
path, a Plan's Change Workflow tier and trigger plus its dependencies and
Coordination, the plan validate result, or the required
verification/finalization route. The shared `Build required` value is `none`.
Each shared `Residuals` row names an unrecorded item, conflict, or
blocker and reason; use `none` when absent.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: the steps and the rules
  governing them.
